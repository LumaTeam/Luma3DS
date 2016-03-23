# AuReiNand
*A modification of the ReiNand custom firmware*

**Compiling:**

You'll need armips and [bin2c](https://sourceforge.net/projects/bin2c/) added to your Path. [HERE](http://www91.zippyshare.com/v/ePGpjk9r/file.html) is a pre-compiled version of armips.

Lastly, just run Make and everything should work!

Copy everything in 'out' folder to SD root and run!

**Usage / Features:**

See https://github.com/Reisyukaku/ReiNand and http://gbatemp.net/threads/reinand-mod-o3ds-n3ds-sysnand.411110

The FIRMs you need are [HERE](http://www99.zippyshare.com/v/kEIiQl0x/file.html).

**Credits:**
 
Rei as this is mostly his code.

The offset to detect the console, and to calculate the O3DS NAND CTR are from Decrypt9.

tiniVi suggested me a way to detect a A9LH environment, and figured out screen deinit.

Delebile provided me with the FIRM writes blocking patch.

A skilled reverser gave me the new reboot patch.

The screen init code is from dark_samus, bil1s, Normmatt, delebile and everyone who contributed.

The code for printing to the screen is from CakesFW.
