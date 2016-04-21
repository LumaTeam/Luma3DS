# SaltFW

Slim Alternative Firmware for 3DS!
It's so light and minimalist, you won't even notice it's there.

### Features: 
- No payload support.
- No config, perfect for minimalists and children.
- No firmware.bin (unless you want to one for NTR).
- No EmuNAND.
- Automatic firmware patching.
- Automatic TWL/AGB patching.
- Reboot patches.
- Optional splash screen.

### How to install:
1. Drop `SaltFW.bin` to the root of your SD.
2. Set your choice of bootmanager to load `/SaltFW.bin` **OR** Rename `SaltFW.bin` to `arm9loaderhax.bin`
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

### Compiling
You'll need armips, [bin2c](https://sourceforge.net/projects/bin2c/), and a recent build of [makerom](https://github.com/profi200/Project_CTR) added to your PATH. [HERE](http://www91.zippyshare.com/v/ePGpjk9r/file.html) is a pre-compiled version of armips for Windows.  
Then, just run "make" and everything should work!  
You can find the compiled files in the 'out' folder.


### Credits
AuroraWright for her great coding.
Rei for his great ReiNand.
And everyone they mentioned in their credits sections.
Thanks, all of you.

### License
See `LICENSE.md`
