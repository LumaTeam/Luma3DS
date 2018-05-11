# Comet3DS
*Easy to use (N)3DS "Custom Firmware"*

## What it is

Comet3DS was created by the main devs with one thing in mind -- being more acceptive in PRs. Luma3DS had terrible and cruel staff, but the main devs were kinder, with a more cheerful dispostion. We take about 95% of PRs, so fire away :)
=======
**Luma3DS** is a program to patch the system software of (New) Nintendo 3DS handheld consoles "on the fly", adding features (such as per-game language settings and debugging capabilities for developers) and removing restrictions enforced by Nintendo (such as the region lock).

This is a fork of Aurora Wright's and TuxSH's Luma3DS software. My main goal with this fork is to develop more options for Rosalina loader, and experiment a bit.

---

## Compiling

First you need to clone the repository with: `git clone https://github.com/6100m/Comet3DS.git`  
To compile, you'll need [armips](https://github.com/Kingcom/armips) and a build of a recent commit of [makerom](https://github.com/profi200/Project_CTR) added to your PATH. You'll also need to install [firmtool](https://github.com/TuxSH/firmtool), its README contains installation instructions.
You'll also need to update your [libctru](https://github.com/smealum/ctrulib) install, building from the latest commit.  
Here are [Windows](https://buildbot.orphis.net/armips/) and [Linux](https://mega.nz/#!uQ1T1IAD!Q91O0e12LXKiaXh_YjXD3D5m8_W3FuMI-hEa6KVMRDQ) builds of armips (thanks to who compiled them!) and [makerom](https://github.com/Steveice10/buildtools/tree/master/3ds) (thanks @Steveice10!).   
Run `make` and everything should work!  
You can find the compiled files in the `out` folder.

---

## Setup / Usage / Features

See https://github.com/6100m/Luma3DS/wiki

---

## Discord

Please visit us on our Discord server where you can keep up to date with the development of Comet3DS!
https://discord.gg/gJM6wNU

---

## Licensing

This software is licensed under the terms of the GPLv3.  
You can find a copy of the license in the LICENSE.txt file.

## Are we partners with the Luma3DS Team

No, and we never will be. Even though this is a fork of their project, we are completely seperate. They know absolutely nothing about teamwork :P
