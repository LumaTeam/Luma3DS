CakeBrah
========

This is a fork of Brahma, that loads CakeHax payloads in the environment they expect.  
This means mostly setting the framebuffer offsets and mode right.  

How to use this in your project
-------------------------------

Look at [CakeHax](https://github.com/mid-kid/CakeHax) for this. It's pretty much the same.  
No injection with dd is needed, as it loads the payload from your .dat file.  
The different configuration flags are detailed below.  

### Makefile options

Name            |Default                             |Description
----------------|------------------------------------|-----------
dir\_out        |$(CURDIR)                           |Where the output files should be placed (3dsx and smdh).
name            |Cakes.dat                           |The name of the .dat file from which the payload will be loaded.
filepath        |                                    |Path in the SD card where the .dat file is located.
APP\_TITLE      |Cakes                               |The title of the app shown in the Homebrew Menu.
APP\_DESCRIPTION|Privileged ARM11/ARM9 Code Execution|The description of the app shown in the Homebrew Menu.
APP\_AUTHOR     |patois                              |The author of the app shown in the Homebrew Menu.
