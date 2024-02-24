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

char AccessID[21] = "00000000000000000001";
static bool HasCard = false;
uint8_t UID[8] = {0};

static struct aimepcsc_context *ctx;
// static struct aime_data data;

#pragma region READER SPECIFIC
static unsigned int __stdcall reader_poll_thread_proc(void *ctx)
{
    while (!READER_POLL_STOP_FLAG)
    {
        if (!HasCard)
        {
            scard_update(UID);

            if (UID[0] > 0) // If a card was read, format it properly and set HasCard to true so the game can insert it on next frame.
            {
                printf("%s: Read card %02X%02X%02X%02X%02X%02X%02X%02X\n", module, UID[0], UID[1], UID[2], UID[3], UID[4], UID[5], UID[6], UID[7]);

                // Properly format the AccessID
                uint64_t ReversedAccessID;
                for (int i = 0; i < 8; i++)
                    ReversedAccessID = (ReversedAccessID << 8) | UID[i];
                sprintf(AccessID, "%020llu", ReversedAccessID);

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

    // // int ret;
    // // FILE *fp;

    // // ret = AllocConsole();

    // // // someone might already allocated a console - seeing this on fufubot's segatools
    // // if (ret != 0)
    // // {
    // //     // only when we allocate a console, we need to redirect stdout
    // //     freopen_s(&fp, "CONOUT$", "w", stdout);
    // // }

    // // ctx = aimepcsc_create();
    // // if (!ctx)
    // // {
    // //     return E_OUTOFMEMORY;
    // // }

    // // ret = aimepcsc_init(ctx);

    // // if (ret != 0)
    // // {
    // //     printf("aimeio-pcsc: failed to initialize: %s\n", aimepcsc_error(ctx));
    // //     aimepcsc_destroy(ctx);
    // //     ctx = NULL;
    // //     return E_FAIL;
    // // }

    // // printf("aimeio-pcsc: initialized with reader: %s\n", aimepcsc_reader_name(ctx));

    return S_OK;
}

HRESULT aime_io_nfc_poll(uint8_t unit_no)
{
    return S_OK;
}

HRESULT aime_io_nfc_get_aime_id(uint8_t unit_no, uint8_t *luid, size_t luid_size)
{
    // Lol what
    return S_FALSE;
}

HRESULT aime_io_nfc_get_felica_id(uint8_t unit_no, uint64_t *IDm)
{
    assert(IDm != NULL);

    if (HasCard)
    {
        printf("%s: Card has been scanned ! : %s\n", module, AccessID);
        HasCard = false;

        uint64_t val;
        for (int i = 0; i < 8; i++)
        {
            val = (val << 8) | UID[i];
            UID[i] = 0;
        }
        *IDm = val;

        return S_OK;
    }

    // // assert(IDm != NULL);

    // // if (unit_no != 0 || data.card_type != FeliCa || data.card_id_len != 8)
    // // {
    // //     return S_FALSE;
    // // }

    // // val = 0;

    // // for (i = 0; i < 8; i++)
    // // {
    // //     val = (val << 8) | data.card_id[i];
    // // }

    // // *IDm = val;

    return S_FALSE;
}

void aime_io_led_set_color(uint8_t unit_no, uint8_t r, uint8_t g, uint8_t b)
{
}
#pragma endregion