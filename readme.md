# AimeIO CardReader

This allows you to use smartcard readers (specifically the acr122u) with [segatools](https://gitea.tendokyu.moe/Dniel97/segatools)

# Acknowledgments

This is a plugin destined to be used with [Dniel97's](https://gitea.tendokyu.moe/Dniel97)'s segatools fork (but should work on others too).

SmartCard implementation taken from [spicetools](https://github.com/spicetools/spicetools)

Initial project made by [nat](https://gitea.tendokyu.moe/nat/aimeio-pcsc)

# Usage

To use it with a game, copy `aimeio.dll` to your `segatools` folder and add the following to your `segatools.ini`:

```ini
[aimeio]
path=aimeio.dll
```

If you scan a newer AIC-based Aime, its FeliCa IDm will be provided to the game. The game will not see the correct "access code," but the IDm should be unique to each card so that particular card can still track your plays. 

As for Mifare cards, their access code won't be 1:1 with a real reader (i still need to fix this). You can still use them and they will work though !

# Build

To build this, you'll need two things :

- [Meson 1.1.0](https://mesonbuild.com)
- [Build Tools pour Visual Studio 2022](https://visualstudio.microsoft.com/fr/downloads/)

Once you've edited your build64.bat file to point to your local installation of the VS2022 build tools, run build64.bat and the output will be located in `bin/aime.dll`.