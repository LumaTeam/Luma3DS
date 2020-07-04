# Luma3DS-3GX Plugin Edition
*Noob-proof (N)3DS "Custom Firmware", with 3GX plugins support*

### 3GX Plugin Edition
This edition of **Luma3DS** allows the loading of **.3GX plugins** in Luma3DS, which are otherwise officially unsupported.


### How to install this Edition
1. download the latest `boot.firm` from [the releases page](https://github.com/mind-overflow/Luma3DS-3GX/releases/latest)
2. put the downloaded `boot.firm` file in the `root` directory of your SD card (`sd:/boot.firm`), overwriting the official Luma3DS `boot.firm`.
3. (re)boot your 3DS, and when prompted, enable:
    - "Enable game patching"
    - "Show NAND or user string in System Settings"
4. press `START` and let your 3DS boot.

You successfully installed the 3GX Plugin Loader! Now, proceed to the next step to learn how to install and enable 3GX plugins.

### How to install 3GX plugins
Plugins have to be installed in the `sd:/luma/plugins` folder.
Usually, you need to put your specific plugin in the `<TITLEID>` subdirectory, eg: `sd:/luma/plugins/<TITLEID>/<filename>.3gx`.
However, a `default.3gx` plugin can also be placed in the main `sd:/luma/plugins` directory: `sd:/luma/plugins/default.3gx`.

So:
``` yaml
    sd:/luma/plugins/default.3gx # will be loaded for all games, low priority
    sd:/luma/plugins/<TITLEID>/<filename>.3gx # will only be loaded for the specified title, high priority
```

Now you know how to install 3GX plugins! Proceed to the next step to learn how how to enable 3GX plugins.

### How to enable 3GX plugins
1. when booted, press `L + D-Pad Down + Select` to open the Rosalina menu.
2. Press `D-Pad Down` again until `Plugin Loader`, is selected, then press `A` and set it to `[Enabled]`.

Done! You learned to install the 3GX Plugin loader, install 3GX Plugins and enable them. Now, simply launch the game you want to play and press `SELECT` to open up the 3GX menu!


### Luma3DS introduction
**Luma3DS** is a program to patch the system software of (New) Nintendo (2)3DS handheld consoles "on the fly", adding features such as per-game language settings, debugging capabilities for developers, and removing restrictions enforced by Nintendo such as the region lock.

It also allows you to run unauthorized ("homebrew") content by removing signature checks.
To use it, you will need a console capable of running homebrew software on the Arm9 processor.

Since v8.0, Luma3DS has its own in-game menu, triggerable by <kbd>L+Down+Select</kbd> (see the [release notes](https://github.com/LumaTeam/Luma3DS/releases/tag/v8.0)).

#
### Compiling
* Prerequisites
    1. git
    2. [makerom](https://github.com/jakcron/Project_CTR) in PATH
    3. [firmtool](https://github.com/TuxSH/firmtool)
    4. Up-to-date devkitARM+libctru
1. Clone the repository with `git clone https://github.com/mind-overflow/Luma3DS-3GX.git`
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
