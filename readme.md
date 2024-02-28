# AimeIO CardReader

This allows you to a smartcard reader (specifically the acr122u) with segatools

# Acknowledgments

This is a plugin destined to be used with [Dniel97](https://gitea.tendokyu.moe/Dniel97)'s [segatools fork](https://gitea.tendokyu.moe/Dniel97/segatools) (but should work on others too).

SmartCard implementation taken from [spicetools](https://github.com/spicetools/spicetools)

Initial project made by [nat](https://gitea.tendokyu.moe/nat/aimeio-pcsc)

All the logic for reading cards from a file has been taken from [CrazyRedMachine](https://github.com/CrazyRedMachine)'s [RedBoard](https://github.com/CrazyRedMachine/RedBoard) aimeio.

# Usage

Make sure to install the ACR122U's [driver](https://www.acs.com.hk/en/driver/3/acr122u-usb-nfc-reader/) before using this.

To use it with a game, copy `aimeio.dll` to your `segatools` folder and add the following to your `segatools.ini`:

```ini
[aimeio]
path=aimeio.dll    ;Path to aimeio.dll
scan=0x0D          ;Sets the key which will be used to insert a card in game. The default is 'Return'


;Everything below this line is optional.

;readerOptional=1           ;Make reader optional, so that you can still use the keyboard
;readerName="ACS ACR122 0"  ;Manually select which reader to use
;disableBuzzer=0            ;Disable the buzzer 
;aimePath=""                ;Manually specify an aime.txt file
;felicaPath=""              ;Manually specify a felica.txt file
;debug=0                    ;Display function calls
```

## Scanning cards

If you scan a newer AIC-based Aime, its FeliCa IDm will be provided to the game. The game will not see the correct "access code," but the IDm should be unique to each card so that particular card can still track your plays.  

## Inserting cards

By pressing the key you have set in segatools.ini and holding it for a few moments, you will insert a card set in either aime.txt or felica.txt

You can have multiple cards in those files!  
By holding a number from 0 to 9 on your keypad, and then holding enter, you can choose what card to insert.

For example...

```text
aime.txt
---

1a7f3e925cb866d45a3b
a5d04d668bc529e35aaa
```

By only pressing the insert key, you will insert card0, "1a7f3e925cb866d45a3b".  
If you hold 1 on your numpad and press the insert key, card1, "a5d04d668bc529e35aaa" will be inserted !  

This works for up to 10 cards.

# Build

To build this, you'll need two things :

- [Meson 1.1.0](https://mesonbuild.com)
- [Build Tools pour Visual Studio 2022](https://visualstudio.microsoft.com/fr/downloads/)

Once you've edited your build64.bat file to point to your local installation of the VS2022 build tools, run build64.bat and the output will be located in `bin/aime.dll`.