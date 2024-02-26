#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "aimeio.h"
#include "scard/scard.h"

char module[] = "CardReader";

// Reader Thread
static bool READER_RUNNER_INITIALIZED = false;
static HANDLE READER_POLL_THREAD;
static bool READER_POLL_STOP_FLAG;

static bool polling = false;
static bool HasCard = false;
uint8_t UID[8] = {0};

struct aime_io_config
{
    bool debug;
    wchar_t aime_path[MAX_PATH];
    wchar_t felica_path[MAX_PATH];
    bool felica_gen;
    uint8_t vk_scan;
};

static struct aime_io_config aime_io_cfg;
static uint8_t aime_io_aime_id[10];
static uint8_t aime_io_felica_id[8];
static bool aime_io_aime_id_present;
static bool aime_io_felica_id_present;

#pragma region CONFIG
static void aime_io_config_read(struct aime_io_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->debug = GetPrivateProfileIntW(
        L"aimeio",
        L"debug",
        0,
        filename);

    GetPrivateProfileStringW(
        L"aimeio",
        L"aimePath",
        L"DEVICE\\aime.txt",
        cfg->aime_path,
        _countof(cfg->aime_path),
        filename);

    GetPrivateProfileStringW(
        L"aimeio",
        L"felicaPath",
        L"DEVICE\\felica.txt",
        cfg->felica_path,
        _countof(cfg->felica_path),
        filename);

    cfg->felica_gen = GetPrivateProfileIntW(
        L"aimeio",
        L"felicaGen",
        1,
        filename);

    cfg->vk_scan = GetPrivateProfileIntW(
        L"aimeio",
        L"scan",
        VK_RETURN,
        filename);

    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_config_read(filename : %ls). \r\n", filename);
}

static HRESULT aime_io_read_id_file(const wchar_t *path, uint8_t *bytes, size_t nbytes, int LineToRead)
{
    HRESULT hr;
    FILE *f;
    size_t i;
    int byte;
    long offset;
    int currentLine = 0;

    f = _wfopen(path, L"r");
    if (f == NULL) // If the file doesn't exist, we bail.
        return S_FALSE;

    memset(bytes, 0, nbytes);

    // Calculate the offset to the start of the desired line
    offset = 0;
    while (currentLine < LineToRead)
    {
        int c = fgetc(f);
        if (c == EOF)
        {
            // If the end of the file is reached before the desired line, print an error
            printf("%s: %S: Error: Line %d does not exist\n", module, path, LineToRead);
            hr = E_FAIL;
            goto end;
        }

        if (c == '\n')
            currentLine++;

        offset++;
    }

    // Seek to the calculated offset
    fseek(f, offset, SEEK_SET);

    // Read the desired line and extract hexadecimal values
    for (i = 0; i < nbytes; i++)
    {
        int r = fscanf(f, "%02x", &byte);

        if (r != 1)
        {
            printf("%s: %S: Error parsing line %d\n", module, path, LineToRead);
            hr = E_FAIL;
            goto end;
        }

        bytes[i] = byte;
    }

    // Check if the line is not nbytes long
    if (fgetc(f) != '\n' && !feof(f))
    {
        printf("%s: %S: Error: Line %d is not %zu bytes long\n", module, path, LineToRead, nbytes);
        hr = E_FAIL;
        goto end;
    }

    hr = S_OK;

end:
    if (f != NULL)
    {
        fclose(f);
    }

    return hr;
}

static HRESULT aime_io_generate_felica(const wchar_t *path, uint8_t *bytes, size_t nbytes)
{
    size_t i;
    FILE *f;

    assert(path != NULL);
    assert(bytes != NULL);
    assert(nbytes > 0);

    srand(time(NULL));

    for (i = 0; i < nbytes; i++)
        bytes[i] = rand();

    /* FeliCa IDm values should have a 0 in their high nibble. I think. */
    bytes[0] &= 0x0F;

    f = _wfopen(path, L"w");

    if (f == NULL) // If we somehow can't create the file, we error out.
    {
        printf("%s: %S: fopen failed: %i\n", module, path, (int)errno);
        return E_FAIL;
    }

    for (i = 0; i < nbytes; i++)
        fprintf(f, "%02X", bytes[i]);

    fprintf(f, "\n");
    fclose(f);

    printf("%s: Generated random FeliCa ID\n", module);

    return S_OK;
}

#pragma endregion

#pragma region READER SPECIFIC
static unsigned int __stdcall reader_poll_thread_proc(void *ctx)
{
    if (aime_io_cfg.debug)
        printf("DEBUG: reader_poll_thread_proc(). \r\n");
    while (!READER_POLL_STOP_FLAG)
    {
        if (!HasCard && polling)
        {
            uint8_t _UID[8] = {0};
            scard_update(_UID);

            if (_UID[0] > 0) // If a card was read, format it properly and set HasCard to true so the game can insert it on next frame.
            {
                printf("%s: Read card %02X%02X%02X%02X%02X%02X%02X%02X\n", module, _UID[0], _UID[1], _UID[2], _UID[3], _UID[4], _UID[5], _UID[6], _UID[7]);
                for (int i = 0; i < 8; i++)
                    UID[i] = _UID[i];

                HasCard = true;
            }
        }
    }

    return 0;
}
#pragma endregion

#pragma region AIME
uint16_t aime_io_get_api_version(void)
{
    return 0x0100;
}

HRESULT aime_io_init(void)
{

    // At init we want to open a console...
    int ret = AllocConsole();
    FILE *fp;
    // someone might already allocated a console - seeing this on fufubot's segatools
    if (ret != 0)
        freopen_s(&fp, "CONOUT$", "w", stdout); // only when we allocate a console, we need to redirect stdout

    // We then read the segatools config file to get settings.
    aime_io_config_read(&aime_io_cfg, L".\\segatools.ini");

    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_init(). \r\n");

    //  Find and initialize reader(s)
    if (!READER_RUNNER_INITIALIZED)
    {
        READER_RUNNER_INITIALIZED = true;
        printf("%s: Initializing SmartCard\n", module);

        if (!scard_init())
        {
            printf("%s: Couldn't init SmartCard\n", module);
            return E_FAIL;
        }
    }

    printf("%s: Starting reader thread.\n", module);

    // Start reader thread
    READER_POLL_STOP_FLAG = false;
    READER_POLL_THREAD = (HANDLE)_beginthreadex(
        NULL,
        0,
        reader_poll_thread_proc,
        NULL,
        0,
        NULL);

    return S_OK;
}

HRESULT aime_io_nfc_poll(uint8_t unit_no)
{
    if (aime_io_cfg.debug)
        printf("\n\nDEBUG: aime_io_nfc_poll(unit_no %d). \r\n", unit_no);

    if (unit_no != 0)
        return S_OK;

    polling = true;

    bool sense;
    HRESULT hr;

    // Don't do anything more if the scan key is not held
    sense = GetAsyncKeyState(aime_io_cfg.vk_scan) & 0x8000;
    if (!sense)
    {
        aime_io_aime_id_present = false;
        aime_io_felica_id_present = false;
        return S_OK;
    }

    //  Set which card we want to read (we will read the x'th line in the card's file, x being determined by which key is pressed on the keypad).
    int card = 0;
    for (int key = VK_NUMPAD0; key <= VK_NUMPAD9; key++)
    {
        short keyState = GetAsyncKeyState(key);
        if (keyState & 0x8000) // Check if the most significant bit is set, indicating that the key is down
        {
            card = key - VK_NUMPAD0;
            continue;
        }
    }

    printf("%s: Attempting to read card %d from file. \r\n", module, card);

    // Try AiMe IC
    hr = aime_io_read_id_file(
        aime_io_cfg.aime_path,
        aime_io_aime_id,
        sizeof(aime_io_aime_id),
        card);

    if (SUCCEEDED(hr) && hr != S_FALSE)
    {
        aime_io_aime_id_present = true;
        return S_OK;
    }

    // Try FeliCa IC
    hr = aime_io_read_id_file(
        aime_io_cfg.felica_path,
        aime_io_felica_id,
        sizeof(aime_io_felica_id),
        card);

    if (SUCCEEDED(hr) && hr != S_FALSE)
    {
        aime_io_felica_id_present = true;
        return S_OK;
    }

    // Try generating FeliCa IC (if enabled)
    if (aime_io_cfg.felica_gen)
    {
        hr = aime_io_generate_felica(
            aime_io_cfg.felica_path,
            aime_io_felica_id,
            sizeof(aime_io_felica_id));

        if (FAILED(hr))
            return hr;

        aime_io_felica_id_present = true;
    }

    return S_OK;
}

HRESULT aime_io_nfc_get_aime_id(uint8_t unit_no, uint8_t *luid, size_t luid_size)
{
    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_nfc_get_aime_id(unit_no : %d). \r\n", unit_no);

    assert(luid != NULL);
    assert(luid_size == sizeof(aime_io_aime_id));

    if (unit_no != 0)
        return S_FALSE;

    if (aime_io_aime_id_present)
    {
        memcpy(luid, aime_io_aime_id, luid_size);
        printf("%s: Read Aime card from file with uid ", module);
        for (int i = 0; i < 10; i++)
            printf("%02x ", aime_io_aime_id[i]);
        printf("\r\n");
        return S_OK;
    }

    return S_FALSE;
}

HRESULT aime_io_nfc_get_felica_id(uint8_t unit_no, uint64_t *IDm)
{
    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_nfc_get_felica_id(unit_no : %d). \r\n", unit_no);

    uint64_t val;
    size_t i;

    assert(IDm != NULL);
    if (unit_no != 0)
        return S_FALSE;

    if (aime_io_felica_id_present)
    {
        val = 0;
        for (i = 0; i < 8; i++)
            val = (val << 8) | aime_io_felica_id[i];

        *IDm = val;
        printf("%s: Read FeliCa card from file with uid %llx\r\n", module, val);

        return S_OK;
    }

    if (HasCard)
    {
        polling = false;
        HasCard = false;

        uint64_t val;
        for (int i = 0; i < 8; i++)
        {
            val = (val << 8) | UID[i];
        }
        *IDm = val;
        printf("%s: FeliCa card has been scanned ! %llx\r\n", module, val);

        return S_OK;
    }
    return S_FALSE;
}

void aime_io_led_set_color(uint8_t unit_no, uint8_t r, uint8_t g, uint8_t b)
{
    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_led_set_color(unit_no : %d, r : %d, g : %d, b : %d). \r\n", unit_no, r, g, b);
}
#pragma endregion