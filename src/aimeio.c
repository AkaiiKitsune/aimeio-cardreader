#include "aimeio.h"
#include "scard/scard.h"

// Reader Thread
static HANDLE READER_POLL_THREAD;
static bool READER_POLL_STOP_FLAG;

static struct aime_io_config aime_io_cfg;
static struct card_data card_data;

#pragma region CONFIG
// This is used because otherwise loading a reader with a given name while keeping the comment inline in the config file fails..
void RemoveCommentAndTruncate(wchar_t *value)
{
    size_t len = wcslen(value);
    wchar_t *semicolon = NULL;

    // Remove comments
    semicolon = wcschr(value, L';');
    if (semicolon != NULL)
    {
        *semicolon = L'\0';
        len = wcslen(value);
    }

    // Trim trailing whitespaces
    while (len > 0 && (value[len - 1] == L' ' || value[len - 1] == L'\t' || value[len - 1] == L'\r' || value[len - 1] == L'\n'))
    {
        value[--len] = L'\0';
    }
}

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

    GetPrivateProfileStringW(
        L"aimeio",
        L"readerName",
        L"",
        cfg->reader_name,
        _countof(cfg->reader_name),
        filename);
    RemoveCommentAndTruncate(cfg->reader_name);

    cfg->reader_optional = GetPrivateProfileIntW(
        L"aimeio",
        L"readerOptional",
        0,
        filename);

    cfg->disable_buzzer = GetPrivateProfileIntW(
        L"aimeio",
        L"disableBuzzer",
        0,
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
            printf("aime_io_read_id_file: %S: Error: Line %d does not exist\n", path, LineToRead);
            hr = E_FAIL;
            goto end;
        }

        if (c == '\n')
        {
            currentLine++;
            offset++;
        }

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
            printf("aime_io_read_id_file: %S: Error parsing line %d\n", path, LineToRead);
            hr = E_FAIL;
            goto end;
        }

        bytes[i] = byte;
    }

    // Check if the line is not nbytes long
    if (fgetc(f) != '\n' && !feof(f))
    {
        printf("aime_io_read_id_file: %S: Error: Line %d is not %zu bytes long\n", path, LineToRead, nbytes);
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
#pragma endregion

#pragma region READER
static unsigned int __stdcall aime_io_poll_thread_proc(void *ctx)
{
    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_poll_thread_proc(). \r\n");
    while (!READER_POLL_STOP_FLAG)
    {
        if (card_data.card_type == 0) // Halting polling once a card is found, waiting for the game to read it's value.
            scard_poll(&card_data);   // We're trying to find a card. If we do, the card's id and type are written to card_data.
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
    int ret = AllocConsole(); // At init we want to open a console...
    FILE *fp;

    if (ret != 0)                               // someone might already allocated a console - seeing this on fufubot's segatools
        freopen_s(&fp, "CONOUT$", "w", stdout); // only when we allocate a console, we need to redirect stdout

    memset(&card_data, 0, sizeof(card_data)); // We init the card_data structure

    // We then read the segatools config file to get settings.
    aime_io_config_read(&aime_io_cfg, L".\\segatools.ini");

    printf("aime_io_init: Initializing SmartCard\n"); //  Find and initialize reader(s)
    if (!scard_init(aime_io_cfg))
    {
        printf("aime_io_init: Couldn't init SmartCard\n"); // If we couldn't init reader, error out.
        if (!aime_io_cfg.reader_optional)
            return E_FAIL;

        printf("aime_io_init: Reader is optional, using keyboard only !\n"); // If however the readerOptional flag is set to 1 in segatools.ini, continue with keyboard only.
        return S_OK;
    }

    printf("aime_io_init: Starting reader thread.\n");

    // Start reader thread
    READER_POLL_STOP_FLAG = false;
    READER_POLL_THREAD = (HANDLE)_beginthreadex(
        NULL,
        0,
        aime_io_poll_thread_proc,
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

    // Don't do anything more if the scan key is not held
    if (!(GetAsyncKeyState(aime_io_cfg.vk_scan) & 0x8000))
        return S_OK;

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

    HRESULT hr;
    printf("aime_io_nfc_poll: Attempting to read card %d from file. \r\n", card);

    // Try AiMe IC
    hr = aime_io_read_id_file(aime_io_cfg.aime_path, card_data.card_id, 10, card);
    if (SUCCEEDED(hr) && hr != S_FALSE)
    {
        card_data.card_type = Mifare;
        return S_OK;
    }

    // Try FeliCa IC
    hr = aime_io_read_id_file(aime_io_cfg.felica_path, card_data.card_id, 8, card);
    if (SUCCEEDED(hr) && hr != S_FALSE)
    {
        card_data.card_type = FeliCa;
        return S_OK;
    }

    // We found nothing.
    return S_OK;
}

HRESULT aime_io_nfc_get_aime_id(uint8_t unit_no, uint8_t *luid, size_t luid_size)
{
    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_nfc_get_aime_id(unit_no : %d). \r\n", unit_no);

    assert(luid != NULL);
    assert(luid_size == 10);

    if (unit_no != 0)
        return S_FALSE;

    if (card_data.card_type == Mifare)
    {
        memcpy(luid, card_data.card_id, luid_size);
        printf("aime_io_nfc_get_aime_id: Sending Aime card with luID %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X\r\n\n", card_data.card_id[0], card_data.card_id[1], card_data.card_id[2], card_data.card_id[3], card_data.card_id[4], card_data.card_id[5], card_data.card_id[6], card_data.card_id[7], card_data.card_id[8], card_data.card_id[9]);

        memset(&card_data, 0, sizeof(card_data)); // Reset card_data structure
        return S_OK;
    }

    return S_FALSE;
}

HRESULT aime_io_nfc_get_felica_id(uint8_t unit_no, uint64_t *IDm)
{
    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_nfc_get_felica_id(unit_no : %d). \r\n", unit_no);

    assert(IDm != NULL);
    if (unit_no != 0)
        return S_FALSE;

    if (card_data.card_type == FeliCa)
    {
        uint64_t val = 0;
        for (size_t i = 0; i < 8; i++)
            val = (val << 8) | card_data.card_id[i];

        *IDm = val;
        printf("aime_io_nfc_get_felica_id: Sending FeliCa card with serial %02X%02X %02X%02X %02X%02X %02X%02X\r\n\n", card_data.card_id[0], card_data.card_id[1], card_data.card_id[2], card_data.card_id[3], card_data.card_id[4], card_data.card_id[5], card_data.card_id[6], card_data.card_id[7]);

        memset(&card_data, 0, sizeof(card_data)); // Reset card_data structure
        return S_OK;
    }

    // if (HasCard)
    // {
    //     polling = false;
    //     HasCard = false;

    //     uint64_t val;
    //     for (int i = 0; i < 8; i++)
    //     {
    //         val = (val << 8) | UID[i];
    //     }
    //     *IDm = val;
    //     printf("aime_io_nfc_get_felica_id: FeliCa card has been scanned ! %llx\r\n", val);

    //     return S_OK;
    // }
    return S_FALSE;
}

void aime_io_led_set_color(uint8_t unit_no, uint8_t r, uint8_t g, uint8_t b)
{
    if (aime_io_cfg.debug)
        printf("DEBUG: aime_io_led_set_color(unit_no : %d, r : %d, g : %d, b : %d). \r\n", unit_no, r, g, b);
}
#pragma endregion