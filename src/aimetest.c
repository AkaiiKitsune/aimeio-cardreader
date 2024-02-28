#include <windows.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "aimeio.h"

int main()
{
    printf("AIMETEST\r\n---------\r\n");
    // printf("api version = %04x\r\n", chuni_io_get_api_version()); /* not compatible with older dlls */
    printf("aime_io_init() : \n");
    switch (aime_io_init())
    {
    case E_FAIL:
        printf("AIMETEST: aime_io_init() returned E_FAIL. Reader is either missing or incompatible !\r\n");
        return E_FAIL;

    case S_OK:
        printf("AIMETEST: aime_io_init() returned S_OK !\r\n");
        break;

    default:
        printf("AIMETEST: aime_io_init() returned an unknown state !\r\n");
        return E_FAIL;
    }

    // printf("aime_io_led_set_color(red) : ");
    // aime_io_led_set_color(0, 255, 0, 0);
    // printf("OK\r\n");
    // Sleep(2000);
    // printf("aime_io_led_set_color(green) : ");
    // aime_io_led_set_color(0, 0, 255, 0);
    // printf("OK\r\n");
    // Sleep(2000);
    // printf("aime_io_led_set_color(blue) : ");
    // aime_io_led_set_color(0, 0, 0, 255);
    // printf("OK\r\n");
    // Sleep(2000);
    // aime_io_led_set_color(0, 0, 0, 0);

    printf("AIMETEST: Running input loop. Press Ctrl+C to exit.\r\n");

    uint8_t luid[10] = {0};
    uint64_t IDm = 0;
    while (1)
    {
        if (aime_io_nfc_poll(0) == S_OK)
        {
            if (aime_io_nfc_get_felica_id(0, &IDm) == S_OK)
            {
                // aime_io_led_set_color(0, 0, 255, 0);
                printf("AIMETEST: Found FeliCa card with IDm %llx\r\n\n", IDm);
            }
            if (aime_io_nfc_get_aime_id(0, luid, 10) == S_OK)
            {
                // aime_io_led_set_color(0, 0, 0, 255);
                printf("AIMETEST: Found old card with luID %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X\r\n", luid[0], luid[1], luid[2], luid[3], luid[4], luid[5], luid[6], luid[7], luid[8], luid[9]);
            }
            Sleep(500);
            // printf("poll ok but no card?!\r\n");
        }
        // Sleep(300);
        // aime_io_led_set_color(0, 0, 0, 0);
    }

    return 0;
}