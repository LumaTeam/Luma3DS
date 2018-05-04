# Luma3DS RosalinaMod
*Noob-proof (N)3DS "Custom Firmware"*

## What it is

**Luma3DS** is a program to patch the system software of (New) Nintendo 3DS handheld consoles "on the fly", adding features (such as per-game language settings and debugging capabilities for developers) and removing restrictions enforced by Nintendo (such as the region lock).
It also allows you to run unauthorized ("homebrew") content by removing signature checks.  
To use it, you will need a console capable of running homebrew software on the ARM9 processor. We recommend [Plailect's guide](https://3ds.guide/) for details on how to get your system ready.

Since Luma3DS v8.0, Luma3DS has its own in-game menu, triggerable by `L+Down+Select` (see the [release notes](https://github.com/AuroraWright/Luma3DS/releases/tag/v8.0)).

Luma3DS RosalinaMod allows to install CIA and add other tools to Rosalina

---

## Compiling

First you need to clone the repository with: `git clone https://github.com/AuroraWright/Luma3DS.git`  
To compile, you'll need [armips](https://github.com/Kingcom/armips) and a build of a recent commit of [makerom](https://github.com/profi200/Project_CTR) added to your PATH. You'll also need to install [firmtool](https://github.com/TuxSH/firmtool), its README contains installation instructions.
For now, you'll also need to update your [libctru](https://github.com/smealum/ctrulib) install, building from the latest commit.  
For your convenience, here are [Windows](http://www91.zippyshare.com/v/ePGpjk9r/file.html) and [Linux](https://mega.nz/#!uQ1T1IAD!Q91O0e12LXKiaXh_YjXD3D5m8_W3FuMI-hEa6KVMRDQ) builds of armips (thanks to who compiled them!) and [makerom](https://github.com/Steveice10/buildtools/tree/master/3ds) (thanks @Steveice10!).   
Finally just run `make` and everything should work!  
You can find the compiled files in the `out` folder.

---

## Setup / Usage / Features

See https://github.com/AuroraWright/Luma3DS/wiki

---

**Credits:**

[Luma3DS](https://github.com/AuroraWright/Luma3DS): for code source Luma3ds   
[d0k3](https://github.com/d0k3/Decrypt9WIP): for code source Decrypt9WIP   
[Smealum](https://github.com/smealum/ctrulib): for ctrulib   
[Steveice10](https://github.com/Steveice10/FBI): for FBI   

---

## Licensing

This software is licensed under the terms of the GPLv3.  
You can find a copy of the license in the LICENSE.txt file.
