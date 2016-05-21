# SaltFW

Slim Alternative Firmware for 3DS!  
### Features: 
- No payload support.
- No config, perfect for minimalists and children.
- No EmuNAND.
- Arm9loaderhax only.
- Automatic **firmware protection** patching for all FIRMs.
- Automatic TWL/AGB patching.
- Automatic region-free patching.
- Automatic reboot patching.
- Optional splash screen.
- Optional region/language patching.
- Optional firmware.bin loading for NTR.
- **Install and forget.**

### How to install standalone:
1. Drop `arm9loaderhax.bin` to the root of your SD.
2. DONE!

### How to install and use with a bootmanager:
1. Drop `SaltFW.bin` to the root of your SD.
2. Set your choice of bootmanager to load `/SaltFW.bin` as default.
3. DONE!

### How to use splash/bootlogo
1. Create `SaltFW` folder to the root of your SD.
2. Put `splash.bin` **OR** `splashbottom.bin` **OR** both in `SaltFW` folder
3. DONE!

### How to use NTR CFW
1. Download [this](http://www70.zippyshare.com/v/Fbj6n1vB/file.html).
2. Put `firmware.bin` from the folder for your device, to the root of SD.
3. Install NTR CFW like regular.
4. DONE!

### How to use Region/Language emulation
1. Create `SaltFW` folder in the root of your SD.
2. Create `locales` folder in `SaltFW`.
3. Create a `txt` file titleID of the game you want to edit. (e.g. `0004000000055E00.txt`)
4. Inside the file, put `RGN LN` where RGN is the region code, and LN is the language code. (e.g. `JPN JP`)
5. DONE!

### Compiling
You'll need armips, [bin2c](https://sourceforge.net/projects/bin2c/), and a recent build of [makerom](https://github.com/profi200/Project_CTR) added to your PATH. [HERE](http://www91.zippyshare.com/v/ePGpjk9r/file.html) is a pre-compiled version of armips for Windows.  
Then, just run "make" and everything should work!  
You can find the compiled files in the 'out' folder.

### Credits
AuroraWright for her great coding.  
TuxSH for helping with Luma3DS.  
Rei for his initial ground work.  
And everyone else they mentioned in their credits sections.  

### License
See `LICENSE.txt`
