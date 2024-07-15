---
name: Bug report
about: Use this to report bugs you encounter with Luma3DS. Make sure you upload the crash dumps if Luma3DS crashes.
---

<!--
-- THIS IS NOT A SUPPORT FORUM! For support go here:
-- Nintendo Homebrew: https://discord.gg/MjzatM8
--
-- Rosalina feature requests go here: https://github.com/LumaTeam/Luma3DS/issues/752
--
-- Also check the Wiki (https://github.com/LumaTeam/Luma3DS/wiki) before making an issue.
--
-- For GBA/DSiWare/DS/AGB_FIRM/TWL_FIRM problems: use https://github.com/MechanicalDragon0687/TWLFix-CFW and update your system.
-- If you're using an emu/redNAND try installing anything on it to sysNAND.
-- Please make sure to read "Enable game patching" https://github.com/LumaTeam/Luma3DS/wiki/Options-and-usage before posting any issues about the "Enable game patching" option(s).
--
-- Luma updaters that don't support Boot9Strap/Sighax won't work.
-- This is due to support for non-B9S/Sighax entrypoints being dropped.
--
-- Please fill in the placeholders.-->
**System model:**

[New 2DS XL, New 3DS XL, New 3DS, Old 2DS, Old 3DS XL, Old 3DS]

**SysNAND version (+emu/redNAND version if applicable):**

[e.g. 11.17.0-50U SysNAND, 11.17.0-50E EmuNAND]
<!--You can check which version you're on in System Settings. It will be on the bottom right of the top screen.-->

**Entrypoint (How/what you're using to boot Luma3DS):**

[e.g. Boot9Strap/Sighax, etc.]

**Luma3DS version:**

[e.g. v13.1.2 stable or if using non-releases specify the commit like this https://github.com/LumaTeam/Luma3DS/commit/988ec17ebfce513fc4589f7b12e0d6e3894ae542]

**Luma3DS configuration/options:**

Default EmuNAND: ( )
<!--This option is only available if there's at least one EmuNAND.-->

Screen brightness: ( )

Splash: ( )

Splash duration: ( )

PIN lock: ( )

New 3DS CPU: ( )
<!--This option is only available on New 3DS (XL)/New 2DS XL.-->

Hbmenu autoboot: ( )

--

Autoboot EmuNAND: ( )
<!--This option is only available if there's at least one EmuNAND.-->

Enable loading external FIRMs and modules: ( )
<!--Firmware (.bin) files are not required by Luma, or NTR CFW anymore.
-- If you're having issues with this option enabled try deleting them from the luma folder on the root of the SD card or /rw/luma on CTRNAND and disabling this option.-->

Enable game patching: ( )

Redirect app. syscore threads to core2: ( )
<!--This option is only available on New 3DS (XL)/New 2DS XL.-->

Show NAND or user string in System Settings: ( )

Show GBA boot screen in patched AGB_FIRM: ( )

--


**Explanation of the issue:**






**Steps to reproduce:**

1.

2.


**Dump file (if applicable):**
<!--If the issue leads to a crash you must ensure the "Disable Arm11 exception handlers"
-- option is not disabled in config.ini.
-- The error message will tell you where the dump is.
-- Zip the dmp file and drag & drop it below.-->
