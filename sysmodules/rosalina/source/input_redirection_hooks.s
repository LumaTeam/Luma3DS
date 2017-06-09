@   This file is part of Luma3DS
@   Copyright (C) 2016-2017 Aurora Wright, TuxSH
@
@   This program is free software: you can redistribute it and/or modify
@   it under the terms of the GNU General Public License as published by
@   the Free Software Foundation, either version 3 of the License, or
@   (at your option) any later version.
@
@   This program is distributed in the hope that it will be useful,
@   but WITHOUT ANY WARRANTY; without even the implied warranty of
@   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
@   GNU General Public License for more details.
@
@   You should have received a copy of the GNU General Public License
@   along with this program.  If not, see <http://www.gnu.org/licenses/>.
@
@   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
@       * Requiring preservation of specified reasonable legal notices or
@         author attributions in that material or in the Appropriate Legal
@         Notices displayed by works containing it.
@       * Prohibiting misrepresentation of the origin of that material,
@         or requiring that modified versions of such material be marked in
@         reasonable ways as different from the original version.

@ Hooks mainly written by Stary (PoC originally by Shiny Quagsire)

.section .rodata
.balign 4
.arm

@@@@@ HID hook @@@@@
.global hidCodePatchFunc
.type   hidCodePatchFunc, %function
hidCodePatchFunc:
push {r4-r6, lr}

push {r3}
mov r5, r1
mrc p15, 0, r4, c13, c0, 3
mov r1, #0x10000
str r1, [r4,#0x80]!
ldr r0, [r0]
svc 0x32
mov r12, r0
pop {r3}

cmp r12, #0
bne skip_touch_cp_cpy

ldrd r0, [r4,#8]    @ load the touch/cp regs into into r0/1
strd r0, [r3, #12]  @ write to r3+12
ldrd r0, [r3, #4]   @ read from r3+4
strd r0, [r5]

skip_touch_cp_cpy:

@                 +0          +4          +8          +12         +16         +20         +24         +28
@                 local hid,  local tsrd  localcprd,  localtswr,  localcpwr,  remote hid, remote ts,  remote circle
@u32 hidData[] = {0x00000FFF, 0x02000000, 0x007FF7FF, 0x00000000, 0x00000000, 0x00000FFF, 0x02000000, 0x007FF7FF};

mov r0, r3 @ Base address.
ldr r1, =0x1ec46000 @ HID reg address.
mov r3, #0xf00
orr r3, #0xff

@ HID reg. Copy +20 => +0 if remote is not exactly 0xfff. Else, pass local through.
ldr r1, [r1]        @ Read HID reg.
ldr r2, [r0, #20]   @ Load remote HID reg.
cmp r2, r3          @ Is remote 0xfff?
movne r1, r2        @ If not, load remote.
str r1, [r0]

@ Touch screen. Copy +24 => +4 if remote is not exactly 0x2000000. Else, pass local through (+12 => +4)
cmp r12, #0         @ full success check

mov r3, #0x2000000
ldreq r1, [r0, #12] @ Load the original (written) touch value.
movne r1, r3
ldr r2, [r0, #24]   @ Load the remote touch value.
cmp r2, r3          @ Is remote 0x2000000?
movne r1, r2        @ If not, load remote.
str r1, [r0, #4]    @ Store.

@ Circle pad. Copy +28 => +8 if remote is not exactly 0x7FF7FF. Else, pass local through. (+16 => +8)
cmp r12, #0         @ full success check

ldr r3, =0x7ff7ff
ldreq r1, [r0, #16] @ Load the original (written) circle value.
movne r1, r3
ldr r2, [r0, #28]   @ Load the remote circle value.
cmp r2, r3          @ Is remote 0x7FF7FF?
movne r1, r2        @ If not, load remote.
str r1, [r0, #8]    @ Store.

ldr r0, [r4,#4]

pop {r4-r6, pc}

.pool

@@@@@ IR hook @@@@@
.global irCodePatchFunc
.type   irCodePatchFunc, %function
irCodePatchFunc:
add lr, lr, #4 @ Skip the address in the hook when we return to the original function
b skip_vars

i2c_readdeviceraw_addr:
    .word 0
redirected_input_addr:
    .word 0

skip_vars:
stmfd sp!, {r4-r5, lr}
ir_check_i2c:
@ Original code. Does an i2c read at device 17 func 5.
@ r0 and r1 were set up by the hook

mov r4, r1                  @ save r1
mov r2, #17
mov r3, #5

adr r12, i2c_readdeviceraw_addr
ldr r12, [r12]
blx r12

adr r5, redirected_input_addr
ldr r5, [r5]

ldr r1, =0x80800081         @ no c-stick activity / zlzr not pressed
cmp r0, #0                  @ check if the i2c read succeeded completely

ldreq r2, [r4]
tsteq r2, #0x100            @ apparently this is set only if the c-stick hasn't been moved yet
movne r2, r1

cmp r1, r2
ldreq r0, [r5]              @ Pull the remote input in.
streq r0, [r4]              @ store it instead of the value read from i2c

@ Return!
mov r0, #0                  @ For ir:user.
ldmfd sp!, {r4-r5, pc}
.pool
