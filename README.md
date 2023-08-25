# Luma3DS
*Noob-proof (N)3DS "Custom Firmware"*

### What it is
**Luma3DS** is a program to patch the system software of (New) Nintendo (2)3DS handheld consoles "on the fly", adding features such as per-game language settings, debugging capabilities for developers, and removing restrictions enforced by Nintendo such as the region lock.

It also allows you to run unauthorized ("homebrew") content by removing signature checks.
To use it, you will need a console capable of running homebrew software on the Arm9 processor.

Since v8.0, Luma3DS has its own in-game menu, triggerable by <kbd>L+Down+Select</kbd> (see the [release notes](https://github.com/LumaTeam/Luma3DS/releases/tag/v8.0)).

#
### Changes with the official build
Note: some features are taken from [DullPointer's luma fork](https://github.com/DullPointer/Luma3DS) because the main goal of this build is updating DullPointer's fork to the latest luma version.

- Removed auto-copy to ctrnand and creating essential files backup
- Restored UNITINFO and enable rosalina on safe_firm options on the luma config menu (TWL patch option is now with "enable external firms and modules")
- Removed the need of having both "Game Patching" and "External Firms and Modules" enabled for patching sysmodules, just "External Firms and Modules" need to be enabled
- Added shortcuts:
  - Press start + select to toggle bottom screen (nice when you watch videos) inspired by [This](https://github.com/DullPointer/Luma3DS/commit/9fea831656446cbaa2b5b4f6364407bb1b35dee7)
  - Press A + B + X + Y + Start to instantly reboot the console. Useful in case of freeze, but can corrupt your sdcard. This is why this option is now hidden in the `config.ini` file
  - Press Start on Rosalina menu to toggle wifi -> [Original](https://github.com/DullPointer/Luma3DS/commit/c1a20558bed3d792d54069719a898006af20ba85)
  - Press Select on Rosalina menu to toggle LEDs -> [Original](https://github.com/DullPointer/Luma3DS/commit/fa70d374c00e39dee8b9ef54f60deb1da35a0c51) (and press Y to force blue led as a workaround when the battery is low)
- Improved system modules speed -> [Original](https://github.com/Core-2-Extreme/Luma3DS/commit/523b7f75d32e5795c67a16ecd45b28fe188bb08d)
- Added n3ds clock + L2 status in rosalina menu -> [Original](https://github.com/DullPointer/Luma3DS/commit/2dbfa8b5c9b719b7f3056691f54332f42da6de8d)
- Added Software Volume Control -> [Original](https://github.com/DullPointer/Luma3DS/commit/dc636d82492d1e87eb51785fa7f2a98617e7ece9)
- Added extended brightness presets -> [Original](https://github.com/DullPointer/Luma3DS/commit/83e8d471a377bc6960fae00d6694f5fe86dcca42)
- Added permanent brightness calibration -> [Original](https://github.com/DullPointer/Luma3DS/commit/0e67a667077f601680f74ddc10ef88a799a5a7ad)
- Added a option to cut wifi while in sleep mode allowing to save battery -> [Original](https://github.com/DullPointer/Luma3DS/commit/174ed486ab59bd249488c9035682fa7d058d1e80)
- Added the ability to redirect layeredFS path -> [Original](https://github.com/DeathChaos25/Luma3DS/commit/8f68d0a19d2ed80fb41bbe8499cb2b7b027e8a8c)
- Added loading of custom logo while launching a app from luma/logo.bin (Can't find the original commit...)
- Changed colors on config menu because why not
- Continue running after a errdisp error happens, this is the same option as instant reboot because they kinda go together (you decide when to reboot after an error occur).

#
### Compiling
* Prerequisites
    1. git
    2. [makerom](https://github.com/jakcron/Project_CTR) in PATH
    3. [firmtool](https://github.com/TuxSH/firmtool)
    4. Up-to-date devkitARM+libctru
1. Clone the repository with `git clone https://github.com/LumaTeam/Luma3DS.git`
2. Run `make`.

    The produced `boot.firm` is meant to be copied to the root of your SD card for usage with Boot9Strap.

#
### Setup / Usage / Features
See https://github.com/LumaTeam/Luma3DS/wiki

#
### Credits
See https://github.com/LumaTeam/Luma3DS/wiki/Credits

#
### Licensing
This software is licensed under the terms of the GPLv3. You can find a copy of the license in the LICENSE.txt file.

Files in the GDB stub are instead triple-licensed as MIT or "GPLv2 or any later version", in which case it's specified in the file header.
