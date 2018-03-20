/*
 *   This file is part of Luma3DS
 *   Copyright (C) 2016-2017 Aurora Wright, TuxSH
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
 *       * Requiring preservation of specified reasonable legal notices or
 *         author attributions in that material or in the Appropriate Legal
 *         Notices displayed by works containing it.
 *       * Prohibiting misrepresentation of the origin of that material,
 *         or requiring that modified versions of such material be marked in
 *         reasonable ways as different from the original version.
 */

/* This file was entirely written by Duckbill */

#include <3ds.h>
#include "menus/cheats.h"
#include "memory.h"
#include "draw.h"
#include "menu.h"
#include "utils.h"
#include "fmt.h"
#include "ifile.h"

#define MAKE_QWORD(hi,low) \
    ((u64) ((((u64)(hi)) << 32) | (low)))

typedef struct CheatProcessInfo
{
    u32 pid;
    u64 titleId;
} CheatProcessInfo;

typedef struct CheatDescription
{
    u32 active;
    u32 valid;
    u32 keyActivated;
    u32 keyCombo;
    char name[40];
    u32 codesCount;
    u64 codes[0];
} CheatDescription;

CheatDescription* cheats[1024] = { 0 };
u8 cheatFileBuffer[16384] = { 0 };
u32 cheatFilePos = 0;
u8 cheatBuffer[16384] = { 0 };

static CheatProcessInfo cheatinfo[0x40] = { 0 };

static s32 Cheats_FetchProcessInfo(void)
{
    u32 pidList[0x40];
    s32 processAmount;

    s64 sa, textTotalRoundedSize, rodataTotalRoundedSize, dataTotalRoundedSize;

    svcGetProcessList(&processAmount, pidList, 0x40);

    for (s32 i = 0; i < processAmount; i++)
    {
        Handle processHandle;
        Result res = svcOpenProcess(&processHandle, pidList[i]);
        if (R_FAILED(res)) continue;

        cheatinfo[i].pid = pidList[i];
        svcGetProcessInfo((s64 *) &cheatinfo[i].titleId, processHandle, 0x10001);
        svcGetProcessInfo(&textTotalRoundedSize, processHandle, 0x10002);
        svcGetProcessInfo(&rodataTotalRoundedSize, processHandle, 0x10003);
        svcGetProcessInfo(&dataTotalRoundedSize, processHandle, 0x10004);
        svcGetProcessInfo(&sa, processHandle, 0x10005);

        svcCloseHandle(processHandle);
    }

    return processAmount;
}

typedef struct CheatState
{
    u32 index;
    u32 offset;
    u32 data;
    u8 typeELine;
    u8 typeEIdx;

    s8 loopLine;
    u32 loopCount;

    u32 ifStack;
    u32 storedStack;
    u8 ifCount;
    u8 storedIfCount;

} CheatState;

CheatState cheat_state = { 0 };
u8 cheatCount = 0;
u8 hasKeyActivated = 0;
u64 cheatTitleInfo = -1ULL;

char failureReason[64];

static bool Cheat_IsValidAddress(const Handle processHandle, u32 address, u32 size)
{
    MemInfo info;
    PageInfo out;

    Result res = svcQueryDebugProcessMemory(&info, &out, processHandle, address);
    if (R_SUCCEEDED(res) && info.state != MEMSTATE_FREE && info.base_addr > 0 && info.base_addr <= address && address <= info.base_addr + info.size - size) {
        return true;
    }
    return false;
}

static u32 ReadWriteBuffer32 = 0;
static u16 ReadWriteBuffer16 = 0;
static u8 ReadWriteBuffer8 = 0;

static bool Cheat_Write8(const Handle processHandle, u32 offset, u8 value)
{
    if (Cheat_IsValidAddress(processHandle, cheat_state.offset + offset, 1))
    {
        *((u8*) (&ReadWriteBuffer8)) = value;
        return R_SUCCEEDED(svcWriteProcessMemory(processHandle, &ReadWriteBuffer8, cheat_state.offset + offset, 1));
    }
    return false;
}

static bool Cheat_Write16(const Handle processHandle, u32 offset, u16 value)
{
    if (Cheat_IsValidAddress(processHandle, cheat_state.offset + offset, 2))
    {
        *((u16*) (&ReadWriteBuffer16)) = value;
        return R_SUCCEEDED(svcWriteProcessMemory(processHandle, &ReadWriteBuffer16, cheat_state.offset + offset, 2));
    }
    return false;
}

static bool Cheat_Write32(const Handle processHandle, u32 offset, u32 value)
{
    if (Cheat_IsValidAddress(processHandle, cheat_state.offset + offset, 4))
    {
        *((u32*) (&ReadWriteBuffer32)) = value;
        return R_SUCCEEDED(svcWriteProcessMemory(processHandle, &ReadWriteBuffer32, cheat_state.offset + offset, 4));
    }
    return false;
}

static bool Cheat_Read8(const Handle processHandle, u32 offset, u8* retValue)
{
    if (Cheat_IsValidAddress(processHandle, cheat_state.offset + offset, 1))
    {
        Result res = svcReadProcessMemory(&ReadWriteBuffer8, processHandle, cheat_state.offset + offset, 1);
        *retValue = *((u8*) (&ReadWriteBuffer8));
        return R_SUCCEEDED(res);
    }
    return false;
}

static bool Cheat_Read16(const Handle processHandle, u32 offset, u16* retValue)
{
    if (Cheat_IsValidAddress(processHandle, cheat_state.offset + offset, 2))
    {
        Result res = svcReadProcessMemory(&ReadWriteBuffer16, processHandle, cheat_state.offset + offset, 2);
        *retValue = *((u16*) (&ReadWriteBuffer16));
        return R_SUCCEEDED(res);
    }
    return false;
}

static bool Cheat_Read32(const Handle processHandle, u32 offset, u32* retValue)
{
    if (Cheat_IsValidAddress(processHandle, cheat_state.offset + offset, 4))
    {
        Result res = svcReadProcessMemory(&ReadWriteBuffer32, processHandle, cheat_state.offset + offset, 4);
        *retValue = *((u32*) (&ReadWriteBuffer32));
        return R_SUCCEEDED(res);
    }
    return false;
}

static u8 typeEMapping[] = { 4 << 3, 5 << 3, 6 << 3, 7 << 3, 0 << 3, 1 << 3, 2 << 3, 3 << 3 };

static u8 Cheat_GetNextTypeE(const CheatDescription* cheat)
{

    if (cheat_state.typeEIdx == 7)
    {
        cheat_state.typeEIdx = 0;
        cheat_state.typeELine++;
    }
    else
    {
        cheat_state.typeEIdx++;
    }
    return (u8) ((cheat->codes[cheat_state.typeELine] >> (typeEMapping[cheat_state.typeEIdx])) & 0xFF);
}

static u32 Cheat_ApplyCheat(const Handle processHandle, const CheatDescription* const cheat)
{
    cheat_state.index = 0;
    cheat_state.offset = 0;
    cheat_state.data = 0;
    cheat_state.index = 0;
    cheat_state.loopCount = 0;
    cheat_state.loopLine = -1;
    cheat_state.ifStack = 0;
    cheat_state.storedStack = 0;
    cheat_state.ifCount = 0;
    cheat_state.storedIfCount = 0;

    while (cheat_state.index < cheat->codesCount)
    {
        u32 skipExecution = cheat_state.ifStack & 0x00000001;
        u32 arg0 = (u32) ((cheat->codes[cheat_state.index] >> 32) & 0x00000000FFFFFFFFULL);
        u32 arg1 = (u32) ((cheat->codes[cheat_state.index]) & 0x00000000FFFFFFFFULL);
        if (arg0 == 0 && arg1 == 0)
        {
            return 0;
        }
        u32 code = ((arg0 >> 28) & 0x0F);
        u32 subcode = ((arg0 >> 24) & 0x0F);

        switch (code)
        {
            case 0x0:
                // 0 Type
                // Format: 0XXXXXXX YYYYYYYY
                // Description: 32bit write of YYYYYYYY to 0XXXXXXX.
                if (!skipExecution)
                {
                    if (!Cheat_Write32(processHandle, (arg0 & 0x0FFFFFFF), arg1)) return 0;
                }
                break;
            case 0x1:
                // 1 Type
                // Format: 1XXXXXXX 0000YYYY
                // Description: 16bit write of YYYY to 0XXXXXXX.
                if (!skipExecution)
                {
                    if (!Cheat_Write16(processHandle, (arg0 & 0x0FFFFFFF), (u16) (arg1 & 0xFFFF))) return 0;
                }
                break;
            case 0x2:
                // 2 Type
                // Format: 2XXXXXXX 000000YY
                // Description: 8bit write of YY to 0XXXXXXX.
                if (!skipExecution)
                {
                    if (!Cheat_Write8(processHandle, (arg0 & 0x0FFFFFFF), (u8) (arg1 & 0xFF))) return 0;
                }
                break;
            case 0x3:
                // 3 Type
                // Format: 3XXXXXXXX YYYYYYYY
                // Description: 32bit if less than.
                // Simple: If the value at address 0XXXXXXX is less than the value YYYYYYYY.
                // Example: 323D6B28 10000000
            {
                u32 newSkip;
                u32 value = 0;
                if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                if (value < arg1)
                {
                    newSkip = 0;
                }
                else
                {
                    newSkip = 1;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                cheat_state.ifCount++;
            }
                break;
            case 0x4:
                // 4 Type
                // Format: 4XXXXXXXX YYYYYYYY
                // Description: 32bit if greater than.
                // Simple: If the value at address 0XXXXXXX is greater than the value YYYYYYYY.
                // Example: 423D6B28 10000000
            {
                u32 newSkip;
                u32 value = 0;
                if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                if (value > arg1)
                {
                    newSkip = 0;
                }
                else
                {
                    newSkip = 1;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                cheat_state.ifCount++;
            }
                break;
            case 0x5:
                // 5 Type
                // Format: 5XXXXXXXX YYYYYYYY
                // Description: 32bit if equal to.
                // Simple: If the value at address 0XXXXXXX is equal to the value YYYYYYYY.
                // Example: 523D6B28 10000000
            {
                u32 newSkip;
                u32 value = 0;
                if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                if (value == arg1)
                {
                    newSkip = 0;
                }
                else
                {
                    newSkip = 1;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                cheat_state.ifCount++;
            }
                break;
            case 0x6:
                // 6 Type
                // Format: 3XXXXXXXX YYYYYYYY
                // Description: 32bit if not equal to.
                // Simple: If the value at address 0XXXXXXX is not equal to the value YYYYYYYY.
                // Example: 623D6B28 10000000
            {
                u32 newSkip;
                u32 value = 0;
                if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                if (value != arg1)
                {
                    newSkip = 0;
                }
                else
                {
                    newSkip = 1;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                cheat_state.ifCount++;
            }
                break;
            case 0x7:
                // 7 Type
                // Format: 7XXXXXXXX 0000YYYY
                // Description: 16bit if less than.
                // Simple: If the value at address 0XXXXXXX is less than the value YYYY.
                // Example: 723D6B28 00005400
            {
                u32 newSkip;
                u16 mask = (u16) ((arg1 >> 16) & 0xFFFF);
                u16 value = 0;
                if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                if ((value & (~mask)) < (arg1 & 0xFFFF))
                {
                    newSkip = 0;
                }
                else
                {
                    newSkip = 1;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                cheat_state.ifCount++;
            }
                break;
            case 0x8:
                // 8 Type
                // Format: 8XXXXXXXX 0000YYYY
                // Description: 16bit if greater than.
                // Simple: If the value at address 0XXXXXXX is greater than the value YYYY.
                // Example: 823D6B28 00005400
            {
                u32 newSkip;
                u16 mask = (u16) ((arg1 >> 16) & 0xFFFF);
                u16 value = 0;
                if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                if ((value & (~mask)) > (arg1 & 0xFFFF))
                {
                    newSkip = 0;
                }
                else
                {
                    newSkip = 1;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                cheat_state.ifCount++;
            }
                break;
            case 0x9:
                // 9 Type
                // Format: 9XXXXXXXX 0000YYYY
                // Description: 16bit if equal to.
                // Simple: If the value at address 0XXXXXXX is equal to the value YYYY.
                // Example: 923D6B28 00005400
            {
                u32 newSkip;
                u16 mask = (u16) ((arg1 >> 16) & 0xFFFF);
                u16 value = 0;
                if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                if ((value & (~mask)) == (arg1 & 0xFFFF))
                {
                    newSkip = 0;
                }
                else
                {
                    newSkip = 1;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                cheat_state.ifCount++;
            }
                break;
            case 0xA:
                // A Type
                // Format: AXXXXXXXX 0000YYYY
                // Description: 16bit if not equal to.
                // Simple: If the value at address 0XXXXXXX is not equal to the value YYYY.
                // Example: A23D6B28 00005400
            {
                u32 newSkip;
                u16 mask = (u16) ((arg1 >> 16) & 0xFFFF);
                u16 value = 0;
                if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                if ((value & (~mask)) != (arg1 & 0xFFFF))
                {
                    newSkip = 0;
                }
                else
                {
                    newSkip = 1;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                cheat_state.ifCount++;
            }
                break;

            case 0xB:
                // B Type
                // Format: BXXXXXXX 00000000
                // Description: Loads offset register.
                if (!skipExecution)
                {
                    cheat_state.offset = (arg0 & 0x0FFFFFFF);
                }
                break;
            case 0xC:
                // C Type
                // Format: C0000000 ZZZZZZZZ
                // Description: Repeat following lines at specified offset.
                // Simple: used to write a value to an address, and then continues to write that value Z number of times to all addresses at an offset determined by the (D6, D7, D8, or DC) type following it.
                // Note: used with the D6, D7, D8, and DC types. C types can not be nested.
                // Example:

                // C0000000 00000005
                // 023D6B28 0009896C
                // DC000000 00000010
                // D2000000 00000000

                cheat_state.loopLine = cheat_state.index;
                cheat_state.loopCount = arg1;
                cheat_state.storedStack = cheat_state.ifStack;
                cheat_state.storedIfCount = cheat_state.ifCount;
                break;
            case 0xD:
                switch (subcode)
                {
                    case 0x00:
                        // D0 Type
                        // Format: D0000000 00000000
                        // Description: ends most recent conditional.
                        // Simple: type 3 through A are all "conditionals," the conditional most recently executed before this line will be terminated by it.
                        // Example:

                        // 94000130 FFFB0000
                        // 74000100 FF00000C
                        // 023D6B28 0009896C
                        // D0000000 00000000

                        // The 7 type line would be terminated.
                        if (cheat_state.loopLine != -1)
                        {
                            if (cheat_state.ifCount > 0 && cheat_state.ifCount > cheat_state.storedIfCount)
                            {
                                cheat_state.ifStack >>= 1;
                                cheat_state.ifCount--;
                            }
                            else
                            {

                                if (cheat_state.loopCount > 0)
                                {
                                    cheat_state.loopCount--;
                                    if (cheat_state.loopCount == 0)
                                    {
                                        cheat_state.loopLine = -1;
                                    }
                                    else
                                    {
                                        if (cheat_state.loopLine != -1)
                                        {
                                            cheat_state.index = cheat_state.loopLine;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (cheat_state.ifCount > 0)
                            {
                                cheat_state.ifStack >>= 1;
                                cheat_state.ifCount--;
                            }
                        }
                        break;
                    case 0x01:
                        // D1 Type
                        // Format: D1000000 00000000
                        // Description: ends repeat block.
                        // Simple: will end all conditionals within a C type code, along with the C type itself.
                        // Example:

                        // 94000130 FFFB0000
                        // C0000000 00000010
                        // 8453DA0C 00000200
                        // 023D6B28 0009896C
                        // D6000000 00000005
                        // D1000000 00000000

                        // The C line, 8 line, 0 line, and D6 line would be terminated.
                        if (cheat_state.loopCount > 0)
                        {
                            cheat_state.ifStack = cheat_state.storedStack;
                            cheat_state.ifCount = cheat_state.storedIfCount;
                            cheat_state.loopCount--;
                            if (cheat_state.loopCount == 0)
                            {
                                cheat_state.loopLine = -1;
                            }
                            else
                            {
                                if (cheat_state.loopLine != -1)
                                {
                                    cheat_state.index = cheat_state.loopLine;
                                }
                            }
                        }
                        break;
                    case 0x02:
                        // D2 Type
                        // Format: D2000000 00000000
                        // Description: ends all conditionals/repeats before it and sets offset and stored to zero.
                        // Simple: ends all lines.
                        // Example:

                        // 94000130 FEEF0000
                        // C0000000 00000010
                        // 8453DA0C 00000200
                        // 023D6B28 0009896C
                        // D6000000 00000005
                        // D2000000 00000000

                        // All lines would terminate.
                        if (cheat_state.loopCount > 0)
                        {
                            cheat_state.loopCount--;
                            if (cheat_state.loopCount == 0)
                            {
                                cheat_state.data = 0;
                                cheat_state.offset = 0;
                                cheat_state.loopLine = -1;

                                cheat_state.ifStack = 0;
                                cheat_state.ifCount = 0;
                            }
                            else
                            {
                                if (cheat_state.loopLine != -1)
                                {
                                    cheat_state.index = cheat_state.loopLine;
                                }
                            }
                        }
                        else
                        {
                            cheat_state.data = 0;
                            cheat_state.offset = 0;
                            cheat_state.ifStack = 0;
                            cheat_state.ifCount = 0;
                        }
                        break;
                    case 0x03:
                        // D3 Type
                        // Format: D3000000 XXXXXXXX
                        // Description: sets offset.
                        // Simple: loads the address X so that lines after can modify the value at address X.
                        // Note: used with the D4, D5, D6, D7, D8, and DC types.
                        // Example: D3000000 023D6B28
                        if (!skipExecution)
                        {
                            cheat_state.offset = arg1;
                        }
                        break;
                    case 0x04:
                        // D4 Type
                        // Format: D4000000 YYYYYYYY
                        // Description: adds to the stored address' value.
                        // Simple: adds to the value at the address defined by lines D3, D9, DA, and DB.
                        // Note: used with the D3, D9, DA, DB, DC types.
                        // Example: D4000000 00000025
                        if (!skipExecution)
                        {
                            cheat_state.data += arg1;
                        }
                        break;
                    case 0x05:
                        // D5 Type
                        // Format: D5000000 YYYYYYYY
                        // Description: sets the stored address' value.
                        // Simple: makes the value at the address defined by lines D3, D9, DA, and DB to YYYYYYYY.
                        // Note: used with the D3, D9, DA, DB, and DC types.
                        // Example: D5000000 34540099
                        if (!skipExecution)
                        {
                            cheat_state.data = arg1;
                        }
                        break;
                    case 0x06:
                        // D6 Type
                        // Format: D6000000 XXXXXXXX
                        // Description: 32bit store and increment by 4.
                        // Simple: stores the value at address XXXXXXXX and to addresses in increments of 4.
                        // Note: used with the C, D3, and D9 types.
                        // Example: D3000000 023D6B28
                        if (!skipExecution)
                        {
                            if (!Cheat_Write32(processHandle, arg1, cheat_state.data)) return 0;
                            cheat_state.offset += 4;
                        }
                        break;
                    case 0x07:
                        // D7 Type
                        // Format: D7000000 XXXXXXXX
                        // Description: 16bit store and increment by 2.
                        // Simple: stores 2 bytes of the value at address XXXXXXXX and to addresses in increments of 2.
                        // Note: used with the C, D3, and DA types.
                        // Example: D7000000 023D6B28
                        if (!skipExecution)
                        {
                            if (!Cheat_Write16(processHandle, arg1, (u16) (cheat_state.data & 0xFFFF))) return 0;
                            cheat_state.offset += 2;
                        }
                        break;
                    case 0x08:
                        // D8 Type
                        // Format: D8000000 XXXXXXXX
                        // Description: 8bit store and increment by 1.
                        // Simple: stores 1 byte of the value at address XXXXXXXX and to addresses in increments of 1.
                        // Note: used with the C, D3, and DB types.
                        // Example: D8000000 023D6B28
                        if (!skipExecution)
                        {
                            if (!Cheat_Write8(processHandle, arg1, (u8) (cheat_state.data & 0xFF))) return 0;
                            cheat_state.offset += 1;
                        }
                        break;
                    case 0x09:
                        // D9 Type
                        // Format: D9000000 XXXXXXXX
                        // Description: 32bit load.
                        // Simple: loads the value from address X.
                        // Note: used with the D5 and D6 types.
                        // Example: D9000000 023D6B28
                        if (!skipExecution)
                        {
                            u32 value = 0;
                            if (!Cheat_Read32(processHandle, arg1, &value)) return 0;
                            cheat_state.data = value;
                        }
                        break;
                    case 0x0A:
                        // DA Type
                        // Format: DA000000 XXXXXXXX
                        // Description: 16bit load.
                        // Simple: loads 2 bytes from address X.
                        // Note: used with the D5 and D7 types.
                        // Example: DA000000 023D6B28
                        if (!skipExecution)
                        {
                            u16 value = 0;
                            if (!Cheat_Read16(processHandle, arg1, &value)) return 0;
                            cheat_state.data = value;
                        }
                        break;
                    case 0x0B:
                        // DB Type
                        // Format: DB000000 XXXXXXXX
                        // Description: 8bit load.
                        // Simple: loads 1 byte from address X.
                        // Note: used with the D5 and D8 types.
                        // Example: DB000000 023D6B28
                        if (!skipExecution)
                        {
                            u8 value = 0;
                            if (!Cheat_Read8(processHandle, arg1, &value)) return 0;
                            cheat_state.data = value;
                        }
                        break;
                    case 0x0C:
                        // DC Type
                        // Format: DC000000 VVVVVVVV
                        // Description: 32bit store and increment by V.
                        // Simple: stores the value at address(es) before it and to addresses in increments of V.
                        // Note: used with the C, D3, D5, D9, D8, DB types.
                        // Example: DC000000 00000100
                        if (!skipExecution)
                        {
                            cheat_state.offset += arg1;
                        }
                        break;
                    case 0x0D:
                        // DD Type
                    {
                        u32 newSkip;
                        if (arg1 == 0 || (HID_PAD & arg1) == arg1)
                        {
                            newSkip = 0;
                        }
                        else
                        {
                            newSkip = 1;
                        }

                        cheat_state.ifStack <<= 1;
                        cheat_state.ifStack |= ((newSkip | skipExecution) & 0x1);
                        cheat_state.ifCount++;
                    }
                        break;
                    default:
                        return 0;
                }
                break;
            case 0xE:
                // E Type
                // Format:
                // EXXXXXXX UUUUUUUU
                // YYYYYYYY YYYYYYYY

                // Description: writes Y to X for U bytes.

            {
                u32 beginOffset = (arg0 & 0x0FFFFFFF);
                u32 count = arg1;
                cheat_state.typeELine = cheat_state.index;
                cheat_state.typeEIdx = 7;
                for (u32 i = 0; i < count; i++)
                {
                    u8 byte = Cheat_GetNextTypeE(cheat);
                    if (!skipExecution)
                    {
                        if (!Cheat_Write8(processHandle, beginOffset + i, byte)) return 0;
                    }
                }
                cheat_state.index = cheat_state.typeELine;
            }
                break;
            default:
                return 0;
        }
        cheat_state.index++;
    }
    return 1;
}

static void Cheat_EatEvents(Handle debug)
{
    DebugEventInfo info;
    Result r;

    while(true)
    {
        if((r = svcGetProcessDebugEvent(&info, debug)) != 0)
        {
            if(r == (s32)(0xd8402009))
            {
                break;
            }
        }
        svcContinueDebugEvent(debug, 3);
    }
}

static Result Cheat_MapMemoryAndApplyCheat(u32 pid, CheatDescription* const cheat)
{
    Handle processHandle;
    Handle debugHandle;
    Result res;
    res = svcOpenProcess(&processHandle, pid);
    if (R_SUCCEEDED(res))
    {
        res = svcDebugActiveProcess(&debugHandle, pid);
        if (R_SUCCEEDED(res))
        {
            Cheat_EatEvents(debugHandle);
            cheat->valid = Cheat_ApplyCheat(debugHandle, cheat);

            svcCloseHandle(debugHandle);
            svcCloseHandle(processHandle);
            cheat->active = 1;
        }
        else
        {
            svcCloseHandle(processHandle);
        }
    }
    else
    {
        sprintf(failureReason, "Open process failed");
    }
    return res;
}

static CheatDescription* Cheat_AllocCheat()
{
    CheatDescription* cheat;
    if (cheatCount == 0)
    {
        cheat = (CheatDescription*) cheatBuffer;
    }
    else
    {
        CheatDescription* prev = cheats[cheatCount - 1];
        cheat = (CheatDescription *) ((u8*) (prev) + sizeof(CheatDescription) + sizeof(u64) * (prev->codesCount));
    }
    cheat->active = 0;
    cheat->valid = 1;
    cheat->codesCount = 0;
    cheat->keyActivated = 0;
    cheat->keyCombo = 0;
    cheat->name[0] = '\0';

    cheats[cheatCount] = cheat;
    cheatCount++;
    return cheat;
}

static void Cheat_AddCode(CheatDescription* cheat, u64 code)
{
    if (cheat)
    {
        cheat->codes[cheat->codesCount] = code;
        (cheat->codesCount)++;
    }
}

static Result Cheat_ReadLine(char* line)
{
    Result res = 0;

    char c = '\0';
    u32 idx = 0;
    while (R_SUCCEEDED(res))
    {
        c = cheatFileBuffer[cheatFilePos++];
        res = c ? 0 : -1;
        if (R_SUCCEEDED(res) && c != '\0')
        {
            if (c == '\n' || c == '\r' || idx >= 1023)
            {
                line[idx++] = '\0';
                return idx;
            }
            else
            {
                line[idx++] = c;
            }
        }
        else
        {
            if (idx > 0)
            {
                line[idx++] = '\0';
                return idx;
            }
        }
    }
    return res;
}

static bool Cheat_IsCodeLine(const char *line)
{
    s32 len = strnlen(line, 1023);
    if (len != 17)
    {
        return false;
    }
    if (line[8] != ' ')
    {
        return false;
    }

    int i;
    for (i = 0; i < 8; i++)
    {
        char c = line[i];
        if (!(('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f')))
        {
            return false;
        }
    }

    for (i = 9; i < 17; i++)
    {
        char c = line[i];
        if (!(('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f')))
        {
            return false;
        }
    }

    return true;
}

static u64 Cheat_GetCode(const char *line)
{
    u64 tmp = 0;
    int i;
    for (i = 0; i < 8; i++)
    {
        char c = line[i];
        u8 code = 0;
        if ('0' <= c && c <= '9')
        {
            code = c - '0';
        }
        if ('A' <= c && c <= 'F')
        {
            code = c - 'A' + 10;
        }
        if ('a' <= c && c <= 'f')
        {
            code = c - 'a' + 10;
        }
        tmp <<= 4;
        tmp |= (code & 0xF);
    }

    for (i = 9; i < 17; i++)
    {
        char c = line[i];
        u8 code = 0;
        if ('0' <= c && c <= '9')
        {
            code = c - '0';
        }
        if ('A' <= c && c <= 'F')
        {
            code = c - 'A' + 10;
        }
        if ('a' <= c && c <= 'f')
        {
            code = c - 'a' + 10;
        }
        tmp <<= 4;
        tmp |= (code & 0xF);
    }

    return tmp;
}

static void Cheat_LoadCheatsIntoMemory(u64 titleId)
{
    cheatCount = 0;
    cheatTitleInfo = titleId;
    hasKeyActivated = 0;

    char path[64] = { 0 };
    sprintf(path, "/luma/titles/%016llX/cheats.txt", titleId);

    IFile file;

    if (R_FAILED(IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ)))
    {
        // OK, let's try another source
        sprintf(path, "/cheats/%016llX.txt", titleId);
        if (R_FAILED(IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ))) return;
    };

    u64 fileLen = 0;
    IFile_GetSize(&file, &fileLen);
    if (fileLen > 16383)
    {
        fileLen = 16383;
    }

    u64 total;
    IFile_Read(&file, &total, cheatFileBuffer, fileLen);
    IFile_Close(&file);
    for (int i = fileLen; i < 16384; i++)
    {
        cheatFileBuffer[i] = 0;
    }

    char line[1024] = { 0 };
    Result res = 0;
    CheatDescription* cheat = 0;
    cheatFilePos = 0;
    do
    {
        res = Cheat_ReadLine(line);
        if (R_SUCCEEDED(res))
        {
            s32 lineLen = strnlen(line, 1023);
            if (!lineLen)
            {
                continue;
            }
            if (line[0] == '#')
            {
                continue;
            }
            if (Cheat_IsCodeLine(line))
            {
                if (cheat)
                {
                    u64 tmp = Cheat_GetCode(line);
                    Cheat_AddCode(cheat, tmp);
                    if (((tmp >> 32) & 0xFFFFFFFF) == 0xDD000000)
                    {
                        if (tmp & 0xFFFFFFFF)
                        {
                            // Not empty key code
                            cheat->keyCombo |= (tmp & 0xFFF);
                            cheat->keyActivated = 1;
                        }
                    }
                }
            }
            else
            {
                if (!cheat)
                {
                    cheat = Cheat_AllocCheat();
                }
                else
                {
                    if (cheat->codesCount > 0)
                    {
                        // Add new cheat only if previous has body. In other case just rewrite it's name
                        cheat = Cheat_AllocCheat();
                    }
                }
                strncpy(cheat->name, line, 39);
            }
        }
    } while (R_SUCCEEDED(res));

    if ((cheatCount > 0) && (cheats[cheatCount - 1]->codesCount == 0))
    {
        cheatCount--; // Remove last empty cheat
    }
}

static u32 Cheat_GetCurrentPID(u64* titleId)
{
    s32 processAmount = Cheats_FetchProcessInfo();

    s32 index = -1;

    for (s32 i = 0; i < processAmount; i++)
    {

        if (((u32) (cheatinfo[i].titleId >> 32)) == 0x00040010 || ((u32) (cheatinfo[i].titleId >> 32) == 0x00040000))
        {
            index = i;

            break;
        }
    }

    if (index != -1)
    {
        *titleId = cheatinfo[index].titleId;
        return cheatinfo[index].pid;
    }
    else
    {
        *titleId = 0;
        return 0xFFFFFFFF;
    }
}

void Cheat_ApplyKeyCheats(void)
{
    if (!cheatCount)
    {
        return;
    }
    if (!hasKeyActivated)
    {
        return;
    }

    u64 titleId = 0;
    u32 pid = Cheat_GetCurrentPID(&titleId);

    if (!titleId)
    {
        cheatCount = 0;
        hasKeyActivated = 0;
        return;
    }

    if (titleId != cheatTitleInfo)
    {
        cheatCount = 0;
        hasKeyActivated = 0;
        return;
    }

    u32 keys = HID_PAD & 0xFFF;
    for (int i = 0; i < cheatCount; i++)
    {
        if (cheats[i]->active && cheats[i]->keyActivated && (cheats[i]->keyCombo & keys) == keys)
        {
            Cheat_MapMemoryAndApplyCheat(pid, cheats[i]);
        }
    }
}

void RosalinaMenu_Cheats(void)
{
    u64 titleId = 0;
    u32 pid = Cheat_GetCurrentPID(&titleId);

    if (titleId != 0)
    {
        if (cheatTitleInfo != titleId || cheatCount == 0)
        {
            Cheat_LoadCheatsIntoMemory(titleId);
        }
    }

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    if (titleId == 0 || cheatCount == 0)
    {
        do
        {
            Draw_Lock();
            Draw_DrawString(10, 10, COLOR_TITLE, "Cheats");
            if (titleId == 0)
            {
                Draw_DrawString(10, 30, COLOR_WHITE, "No suitable title found");
            }
            else
            {
                Draw_DrawFormattedString(10, 30, COLOR_WHITE, "No cheats found for title %016llX", titleId);
            }

            Draw_FlushFramebuffer();
            Draw_Unlock();
        } while (!(waitInput() & BUTTON_B) && !terminationRequest);
    }
    else
    {
        s32 selected = 0, page = 0, pagePrev = 0;

        Result r = 0;
        do
        {
            Draw_Lock();
            if (page != pagePrev || R_FAILED(r))
            {
                Draw_ClearFramebuffer();
            }
            if (R_SUCCEEDED(r))
            {
                Draw_DrawFormattedString(10, 10, COLOR_TITLE, "Cheat list");

                for (s32 i = 0; i < CHEATS_PER_MENU_PAGE && page * CHEATS_PER_MENU_PAGE + i < cheatCount; i++)
                {
                    char buf[65] = { 0 };
                    s32 j = page * CHEATS_PER_MENU_PAGE + i;
                    const char * checkbox = (cheats[j]->active ? "(x) " : "( ) ");
                    const char * keyAct = (cheats[j]->keyActivated ? "*" : " ");
                    sprintf(buf, "%s%s%s", checkbox, keyAct, cheats[j]->name);

                    Draw_DrawString(30, 30 + i * SPACING_Y, cheats[j]->valid ? COLOR_WHITE : COLOR_RED, buf);
                    Draw_DrawCharacter(10, 30 + i * SPACING_Y, COLOR_TITLE, j == selected ? '>' : ' ');
                }
            }
            else
            {
                Draw_DrawFormattedString(10, 10, COLOR_TITLE, "ERROR: %08x", r);
                Draw_DrawFormattedString(10, 30, COLOR_RED, failureReason);
            }
            Draw_FlushFramebuffer();
            Draw_Unlock();

            if (terminationRequest) break;

            u32 pressed;
            do
            {
                pressed = waitInputWithTimeout(50);
                if (pressed != 0) break;
            } while (pressed == 0 && !terminationRequest);

            if (pressed & BUTTON_B)
                break;
            else if ((pressed & BUTTON_A) && R_SUCCEEDED(r))
            {
                if (cheats[selected]->active)
                {
                    cheats[selected]->active = 0;
                }
                else
                {
                    r = Cheat_MapMemoryAndApplyCheat(pid, cheats[selected]);
                }
                hasKeyActivated = 0;
                for (int i = 0; i < cheatCount; i++)
                {
                    if (cheats[i]->active && cheats[i]->keyActivated)
                    {
                        hasKeyActivated = 1;
                        break;
                    }
                }
            }
            else if (pressed & BUTTON_DOWN)
                selected++;
            else if (pressed & BUTTON_UP)
                selected--;
            else if (pressed & BUTTON_LEFT)
                selected -= CHEATS_PER_MENU_PAGE;
            else if (pressed & BUTTON_RIGHT)
            {
                if (selected + CHEATS_PER_MENU_PAGE < cheatCount)
                    selected += CHEATS_PER_MENU_PAGE;
                else if ((cheatCount - 1) / CHEATS_PER_MENU_PAGE == page)
                    selected %= CHEATS_PER_MENU_PAGE;
                else selected = cheatCount - 1;
            }

            if (selected < 0)
                selected = cheatCount - 1;
            else if (selected >= cheatCount) selected = 0;

            pagePrev = page;
            page = selected / CHEATS_PER_MENU_PAGE;
        } while (!terminationRequest);
    }

}
