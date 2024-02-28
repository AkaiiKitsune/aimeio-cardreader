#include "scard.h"

#define MAX_APDU_SIZE 255
int readCooldown = 500;
// set to detect all cards, reduce polling rate to 500ms.
// based off acr122u reader, see page 26 in api document.
// https://www.acs.com.hk/en/download-manual/419/API-ACR122U-2.04.pdf

#define PARAM_POLLRATE 0xDFu
static const BYTE PARAM_SET_PICC[5] = {0xFFu, 0x00u, 0x51u, PARAM_POLLRATE, 0x00u};
static const BYTE COMMAND_GET_UID[5] = {0xFFu, 0xCAu, 0x00u, 0x00u, 0x00u};

// return bytes from device
#define PICC_SUCCESS 0x90u
#define PICC_ERROR 0x63u

enum scard_atr_protocol
{
    SCARD_ATR_PROTOCOL_ISO14443_PART3 = 0x03,
    SCARD_ATR_PROTOCOL_FELICA_212K = 0x11,
};

// winscard_config_t WINSCARD_CONFIG;
static SCARDCONTEXT hContext = 0;
static SCARD_READERSTATE reader_state;
static LONG lRet = 0;

bool scard_init(struct aime_io_config config)
{
    if ((lRet = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hContext)) != SCARD_S_SUCCESS)
    {
        //  log_warning("scard", "failed to establish SCard context: {}", bin2hex(&lRet, sizeof(LONG)));
        return FALSE;
    }

    // get list of readers
    LPTSTR reader_list = NULL;
    auto pcchReaders = SCARD_AUTOALLOCATE;
    lRet = SCardListReaders(hContext, NULL, (LPTSTR)&reader_list, &pcchReaders);

    switch (lRet)
    {
    case SCARD_E_NO_READERS_AVAILABLE:
        printf("scard_init: No readers available\n");
        return FALSE;

    case SCARD_S_SUCCESS:
        LPTSTR reader_name = NULL;
        int readerNameLen = 0;

        // Iterate through the multi-string to get individual reader names
        printf("scard_init: listing all readers : ");
        while (*reader_list != '\0')
        {
            printf("%s, ", reader_list);
            reader_list += strlen(reader_list) + 1;
        }
        printf("\r\n", reader_list);

        // if the readerName array is populated, replace the first reader in the list
        char ReaderCharArray[sizeof(config.reader_name)];
        wcstombs(ReaderCharArray, config.reader_name, sizeof(ReaderCharArray));
        if (strcmp(ReaderCharArray, "") != 0)
        {
            size_t newLen = strlen(ReaderCharArray) + 1;
            LPSTR newMszReaders = (LPSTR)malloc(newLen);
            if (newMszReaders != NULL)
            {
                // Copy the new selected reader
                strcpy(newMszReaders, ReaderCharArray);

                // Update the original pointer to the new modified list
                reader_list = newMszReaders;
            }
            printf("scard_init: Forced using reader : %hs\n", ReaderCharArray);
        }

        // Connect to reader and send PICC operating params command
        SCARDHANDLE hCard;
        DWORD dwActiveProtocol;
        lRet = SCardConnect(hContext, reader_list, SCARD_SHARE_DIRECT, 0, &hCard, &dwActiveProtocol);
        if (lRet != SCARD_S_SUCCESS)
        {
            printf("scard_init: Error connecting to the reader: 0x%08X\n", lRet);
            return FALSE;
        }
        printf("scard_init: Connected to reader: %s, sending PICC params\n", reader_list);

        // set the reader params
        DWORD cbRecv = MAX_APDU_SIZE;
        BYTE pbRecv[MAX_APDU_SIZE];
        lRet = SCardControl(hCard, SCARD_CTL_CODE(3500), PARAM_SET_PICC, sizeof(PARAM_SET_PICC), pbRecv, cbRecv, &cbRecv);
        Sleep(100);
        if (lRet != SCARD_S_SUCCESS)
        {
            printf("scard_init: Error setting PICC params : 0x%08X\n", lRet);
            return FALSE;
        }

        if (cbRecv > 2 && pbRecv[0] != PICC_SUCCESS && pbRecv[1] != PARAM_POLLRATE)
        {
            printf("scard_init: PICC params not valid 0x%02X != 0x%02X\n", pbRecv[1], PARAM_POLLRATE);
            return FALSE;
        }

        // Disconnect from reader
        if ((lRet = SCardDisconnect(hCard, SCARD_LEAVE_CARD)) != SCARD_S_SUCCESS)
            printf("scard_init: Failed SCardDisconnect : 0x%08X\n", lRet);

        // Extract the relevant names from the multi-string.
        readerNameLen = lstrlen(reader_list);
        reader_name = (LPTSTR)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(TCHAR) * (readerNameLen + 1));
        memcpy(reader_name, &reader_list, (size_t)(readerNameLen + 1));

        if (reader_name)
            printf("scard_init: Using reader : %s\n", reader_name);

        memset(&reader_state, 0, sizeof(SCARD_READERSTATE));
        reader_state.szReader = reader_name;
        return TRUE;

    default:
        printf("scard_init: Failed SCardListReaders: 0x%08X\n", lRet);
        return FALSE;
    }
}

void scard_poll(struct card_data *card_data)
{
    lRet = SCardGetStatusChange(hContext, readCooldown, &reader_state, 1);
    if (lRet == SCARD_E_TIMEOUT)
    {
        return;
    }
    else if (lRet != SCARD_S_SUCCESS)
    {
        printf("scard_poll: Failed SCardGetStatusChange: 0x%08X\n", lRet);
        return;
    }

    if (!(reader_state.dwEventState & SCARD_STATE_CHANGED))
        return;

    DWORD newState = reader_state.dwEventState ^ SCARD_STATE_CHANGED;
    bool wasCardPresent = (reader_state.dwCurrentState & SCARD_STATE_PRESENT) > 0;
    if (newState & SCARD_STATE_UNAVAILABLE)
    {
        printf("scard_poll: New card state: unavailable\n");
        Sleep(readCooldown);
    }
    else if (newState & SCARD_STATE_EMPTY)
    {
        printf("scard_poll: New card state: empty\n");
        // scard_clear(unit_no);
    }
    else if (newState & SCARD_STATE_PRESENT && !wasCardPresent)
    {
        printf("scard_poll: New card state: present\n");
        scard_update(card_data, hContext, reader_state.szReader);
    }

    reader_state.dwCurrentState = reader_state.dwEventState;

    return;
}

void scard_update(struct card_data *card_data, SCARDCONTEXT _hContext, LPCTSTR _readerName)
{
    printf("scard_update: Update on reader : %s\n", reader_state.szReader);
    // Connect to the smart card.
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    for (int retry = 0; retry < 100; retry++) // retry times has to be increased since poll rate is set to 500ms
    {
        if ((lRet = SCardConnect(_hContext, _readerName, SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol)) == SCARD_S_SUCCESS)
            break;

        Sleep(20);
    }

    if (lRet != SCARD_S_SUCCESS)
    {
        printf("scard_update: Error connecting to the card: 0x%08X\n", lRet);
        return;
    }

    // set the reader params
    LPCSCARD_IO_REQUEST pci = dwActiveProtocol == SCARD_PROTOCOL_T1 ? SCARD_PCI_T1 : SCARD_PCI_T0;
    DWORD cbRecv = MAX_APDU_SIZE;
    BYTE pbRecv[MAX_APDU_SIZE];

    // Read ATR to determine card type.
    TCHAR szReader[200];
    DWORD cchReader = 200;
    BYTE atr[32];
    DWORD cByteAtr = 32;
    lRet = SCardStatus(hCard, szReader, &cchReader, NULL, NULL, atr, &cByteAtr);
    if (lRet != SCARD_S_SUCCESS)
    {
        printf("scard_update: Error getting card status: 0x%08X\n", lRet);
        return;
    }

    // Only care about 20-byte ATRs returned by arcade-type smart cards
    if (cByteAtr != 20)
    {
        printf("scard_update: Ignoring card with len(%zu) = %02x (%08X)\n", sizeof(cByteAtr), (unsigned int)atr, cByteAtr);
        return;
    }

    printf("scard_update: atr Return: len(%zu) = %02x (%08X)\n", sizeof(cByteAtr), (unsigned int)atr, cByteAtr);

    BYTE cardProtocol = atr[12];
    if (cardProtocol == SCARD_ATR_PROTOCOL_ISO14443_PART3)
    {
        printf("scard_update: Card protocol: ISO14443_PART3\n");
        card_data->card_type = Mifare;
    }

    else if (cardProtocol == SCARD_ATR_PROTOCOL_FELICA_212K) // Handling FeliCa
    {
        printf("scard_update: Card protocol: FELICA_212K\n");
        card_data->card_type = FeliCa;

        // Read mID
        cbRecv = MAX_APDU_SIZE;
        if ((lRet = SCardTransmit(hCard, pci, COMMAND_GET_UID, sizeof(COMMAND_GET_UID), NULL, pbRecv, &cbRecv)) != SCARD_S_SUCCESS)
        {
            printf("scard_update: Error querying card UID: 0x%08X\n", lRet);
            return;
        }

        if (cbRecv > 1 && pbRecv[0] == PICC_ERROR)
        {
            printf("scard_update: UID query failed\n");
            return;
        }

        if ((lRet = SCardDisconnect(hCard, SCARD_LEAVE_CARD)) != SCARD_S_SUCCESS)
            printf("scard_update: Failed SCardDisconnect: 0x%08X\n", lRet);

        if (cbRecv < 8)
        {
            printf("scard_update: Padding card uid to 8 bytes\n");
            memset(&pbRecv[cbRecv], 0, 8 - cbRecv);
        }
        else if (cbRecv > 8)
            printf("scard_update: taking first 8 bytes of len(uid) = %02X\n", cbRecv);

        memcpy(card_data->card_id, pbRecv, 8);
    }

    else
    {
        printf("scard_update: Unknown NFC Protocol: 0x%02X\n", cardProtocol);
        return;
    }

    // Copy UID to struct, reversing if necessary
    // card_info_t card_info;
    // if (shouldReverseUid)
    //     for (DWORD i = 0; i < 8; i++)
    //         card_info.uid[i] = pbRecv[7 - i];
    // else
    //     memcpy(card_info.uid, pbRecv, 8);

    // for (int i = 0; i < 8; ++i)
    //     buf[i] = card_info.uid[i];
}