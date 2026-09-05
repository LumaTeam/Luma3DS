# Experimental HOME Menu ExeFS overrides

With **Enable game patching** enabled, the loader attempts to patch HOME Menu's
`OpenFileDirectly` call so it can open replacement files on the SD card:

```text
/luma/titles/<16-digit uppercase TitleID>/exefs/banner
/luma/titles/<16-digit uppercase TitleID>/exefs/icon
```

Use the game's base TitleID (or the demo's TitleID), not HOME Menu's TitleID or
an update's TitleID. The files have no extension: `icon` is a complete SMDH and
`banner` is a complete ExeFS banner. Restart HOME Menu after enabling the patch.

## Scope and behavior

* This targets HOME Menu's **reads of other titles**, before those titles are
  launched. Adding paths to the game's existing RomFS redirection would not
  intercept these reads.
* Only CTR applications (`00040000`) and demos (`00040002`), main content index
  zero, and exact `icon`/`banner` names are eligible. NAND, SD, and gamecard
  source media use the same per-TitleID replacement path.
* Replacement files are opened read-only. If the replacement open fails,
  including an IPC transport failure, the original request is restored and
  sent normally. Once a replacement opens successfully, its handle is returned
  directly; malformed files or later read errors do not cause another fallback.
* This first implementation requires Luma to be running in SD mode. CTRNAND
  replacements, other processes' reads, archive-handle-based `OpenFile` calls,
  RomFS, `.code`, update content, and `logo` are outside its scope.
* HOME Menu's icon cache is not modified or invalidated. Existing cached icons
  may remain visible, including after removing an override. Extended banners
  supplied through extdata can also take precedence over the ExeFS banner.
* No console/HOME Menu version has yet been validated with this implementation.
  Unsupported code layouts leave the original call unchanged; compilation and
  synthetic execution tests alone do not establish HOME Menu compatibility.

## Implementation

The installer runs in the existing game-patching block for the six HOME Menu
TitleIDs, after IPS/BPS and RomFS patches. It requires an ARM literal load of the
`OpenFileDirectly` IPC header and a unique call to a `svc 0x32; bx lr` wrapper
in that function. It changes that call, rather than globally intercepting SVC
requests. Ambiguous matches are rejected without modifying the executable.

The position-independent ARM payload is placed in zero-filled `.text` page
padding. Space for the existing RomFS payload is reserved at the beginning of
the padding; existing code and data are not reclaimed. If insufficient padding
is available, the new hook is skipped rather than preventing HOME Menu startup.
Neither the kernel nor Process9 is changed.

The IPC layout follows [libctru's OpenFileDirectly implementation](https://github.com/devkitPro/libctru/blob/36fe1ada5b7ebe53ba4decda36d764a55f8fefb6/libctru/source/services/fs.c).
The title-content binary paths are also used by
[FBI's SMDH reader](https://github.com/Steveice10/FBI/blob/ec259153e25fc77ee999b8caadac7e73d2e5e41a/source/fbi/task/listtitles.c).

Every invocation uses 160 bytes of stack, including saved registers, a complete
52-byte IPC request, and the replacement path. There is no shared mutable
payload state and no additional FS session or archive lifetime to manage.

## Automated validation

Build normally with devkitARM, libctru, makerom, and firmtool. Then run:

```sh
python3 -m pip install unicorn pyelftools
python3 tests/test_exefsredir.py sysmodules/loader/loader.elf
```

The tests execute the built ARM installer and a relocated copy of the payload.
They exercise file paths, both allowed title classes, all source media types,
IPC and file-open failure fallback, request filtering, stack/register
preservation, ambiguous/missing SDK patterns, occupied/insufficient padding,
backward calls, and coexistence with reserved RomFS padding. FS replies are
stubbed; these tests do not emulate the 3DS filesystem or HOME Menu.

## Hardware validation required before marking the PR ready

Record the console model, system version, region, HOME Menu TitleID/version,
and exact Luma commit for each test. Keep a known-good `boot.firm` for rollback.

1. Verify that the call is actually patched on a legally obtained HOME Menu
   code dump, or inspect the loaded code with a debugger. A successful boot
   alone is not evidence that the optional hook was installed.
2. Use valid replacement icon/banner files for a test application. Check the
   displayed icon, localized title, banner model, and banner audio. Account for
   an existing icon cache; back it up before any manual cache refresh.
3. Test a digital title, a gamecard title, and a demo; compare against an
   unmodified title and test with only one of the two override files present.
4. Remove the override files, test with game patching disabled, and repeat
   after restarting HOME Menu. Confirm ordinary titles still open and launch.
5. Test alongside HOME Menu RomFS patches and the target game's RomFS, IPS/BPS,
   and locale patches. Exercise suspend/resume and repeated title selection.
6. Confirm behavior on Old and New 3DS and multiple regions/versions. Record
   unsupported code layouts explicitly rather than reporting them as passes.
