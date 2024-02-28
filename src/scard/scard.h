/**
 * MIT-License
 * Copyright (c) 2018 by nolm <nolan@nolm.name>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Modified version.
 */

#pragma once
#include <winscard.h>
#include <src/aimeio.h>

// Card types
enum AIME_CARDTYPE
{
    Mifare = 0x01,
    FeliCa = 0x02
};

// Structure containing card_type and card_id
struct card_data
{
    uint8_t card_type;
    uint8_t card_id[32];
};

/*
    Initialize the smartcard reader

    - config: struct loaded from segatools.ini
*/
bool scard_init(struct aime_io_config config);

/*
    Checks if a new card has been detected

    - card_data: struct containing the card data
*/
void scard_poll(struct card_data *card_data);

/*
    Read the card's data

    - card_data: struct containing the card data
    - _hContext: context for the card reader
    - _readerName: name of the card reader to use
*/
void scard_update(struct card_data *card_data, SCARDCONTEXT _hContext, LPCTSTR _readerName);
