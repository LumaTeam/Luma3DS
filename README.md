# Luma3DS
*Noob-proof (N)3DS "Custom Firmware"*

## What it is

**Luma3DS** is a program to patch the system software of (New) Nintendo 3DS handheld consoles "on the fly", adding features (such as per-game language settings and debugging capabilities for developers) and removing restrictions enforced by Nintendo (such as the region lock).
It also allows you to run unauthorized ("homebrew") content by removing signature checks.
To use it, you will need a console capable of running homebrew software on the ARM9 processor. We recommend [Plailect's guide](https://3ds.hacks.guide/) for details on how to get your system ready.

Since Luma3DS v8.0, Luma3DS has its own in-game menu, triggerable by `L+Down+Select` (see the [release notes](https://github.com/AuroraWright/Luma3DS/releases/tag/v8.0)).

---

## Compiling

First you need to clone the repository with: `git clone https://github.com/AuroraWright/Luma3DS.git`
To compile, you'll need [armips](https://github.com/Kingcom/armips) and a build of a recent commit of [makerom](https://github.com/profi200/Project_CTR) added to your PATH. You'll also need to install [firmtool](https://github.com/TuxSH/firmtool), its README contains installation instructions.
You'll also need to update your [libctru](https://github.com/smealum/ctrulib) install, building from the latest commit.
Here are [Windows](https://buildbot.orphis.net/armips/) and [Linux](https://ev1l0rd.s-ul.eu/mEIk4atQ) builds of armips (thanks to who compiled them!).
Run `make` and everything should work!
You can find the compiled files in the `out` folder.

---

## Setup / Usage / Features

See https://github.com/AuroraWright/Luma3DS/wiki

---

## Credits

See https://github.com/AuroraWright/Luma3DS/wiki/Credits

---

## Licensing

This software is licensed under the terms of the GPLv3.
You can find a copy of the license in the LICENSE.txt file.

You can also use all of `sysmodules/rosalina/include/gdb/`, all of `sysmodules/rosalina/source/gdb/`, `sysmodules/rosalina/include/gdb.h`, `sysmodules/rosalina/source/gdb.c`, `sysmodules/rosalina/include/sock_util.h` and `sysmodules/rosalina/source/sock_util.c` under the terms of the GPLv2, as an option. If you choose to do so, you needn't include the GPLv3 notices that are present in the beginning of each of these files.
