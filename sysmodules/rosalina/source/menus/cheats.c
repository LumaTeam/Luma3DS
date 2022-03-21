/*
 *   This file is part of Luma3DS
 *   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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

#include <3ds.h>
#include <stdlib.h>
#include "menus/cheats.h"
#include "memory.h"
#include "draw.h"
#include "menu.h"
#include "utils.h"
#include "fmt.h"
#include "ifile.h"
#include "pmdbgext.h"

#define MAKE_QWORD(hi,low) \
    ((u64) ((((u64)(hi)) << 32) | (low)))

typedef struct CheatDescription
{
    struct {
        u8 active : 1;
        u8 valid : 1;
        u8 hasKeyCode : 1;
        u8 activeStorage : 1;
    };
    char name[39];
    u32 codesCount;
    u32 storage1;
    u32 storage2;
    u64 codes[0];
} CheatDescription;

typedef struct BufferedFile
{
    IFile file;
    u64 curPos;
    u64 maxPos;
    char buffer[512];
} BufferedFile;

CheatDescription* cheats[1024] = { 0 };
u8 cheatBuffer[32768] = { 0 };
u8 cheatPage[0x1000] = { 0 };

typedef struct CheatState
{
    u32 index;
    u32 offset1;
    u32 offset2;
    u32 data1;
    u32 data2;
    struct {
        u8 activeOffset : 1;
        u8 activeData : 1;
        u8 conditionalMode : 3;
        u8 data1Mode : 1;
        u8 data2Mode : 1;
        u8 floatMode : 1;
    };
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
u64 cheatTitleInfo = -1ULL;
u64 cheatRngState = 0;

static inline u32* activeOffset()
{
    return cheat_state.activeOffset ? &cheat_state.offset2 : &cheat_state.offset1;
}

static inline u32* activeData()
{
    return cheat_state.activeData ? &cheat_state.data2 : &cheat_state.data1;
}

static inline u32* activeStorage(CheatDescription* desc)
{
    return desc->activeStorage ? &desc->storage2 : &desc->storage1;
}

char failureReason[64];

static u32 Cheat_GetRandomNumber(void)
{
    cheatRngState = 0x5D588B656C078965ULL * cheatRngState + 0x0000000000269EC3ULL;
    return (u32)(cheatRngState >> 32);
}

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
    u32 addr = *activeOffset() + offset;
    if (addr >= 0x01E81000 && addr < 0x01E82000)
    {
        cheatPage[addr - 0x01E81000] = value;
        return true;
    }
    if (Cheat_IsValidAddress(processHandle, addr, 1))
    {
        *((u8*) (&ReadWriteBuffer8)) = value;
        return R_SUCCEEDED(svcWriteProcessMemory(processHandle, &ReadWriteBuffer8, addr, 1));
    }
    return false;
}

static bool Cheat_Write16(const Handle processHandle, u32 offset, u16 value)
{
    u32 addr = *activeOffset() + offset;
    if (addr >= 0x01E81000 && addr + 1 < 0x01E82000)
    {
        *(u16*)(cheatPage + addr - 0x01E81000) = value;
        return true;
    }
    if (Cheat_IsValidAddress(processHandle, addr, 2))
    {
        *((u16*) (&ReadWriteBuffer16)) = value;
        return R_SUCCEEDED(svcWriteProcessMemory(processHandle, &ReadWriteBuffer16, addr, 2));
    }
    return false;
}

static bool Cheat_Write32(const Handle processHandle, u32 offset, u32 value)
{
    u32 addr = *activeOffset() + offset;
    if (addr >= 0x01E81000 && addr + 3 < 0x01E82000)
    {
        *(u32*)(cheatPage + addr - 0x01E81000) = value;
        return true;
    }
    if (Cheat_IsValidAddress(processHandle, addr, 4))
    {
        *((u32*) (&ReadWriteBuffer32)) = value;
        return R_SUCCEEDED(svcWriteProcessMemory(processHandle, &ReadWriteBuffer32, addr, 4));
    }
    return false;
}

static bool Cheat_Read8(const Handle processHandle, u32 offset, u8* retValue)
{
    u32 addr = *activeOffset() + offset;
    if (addr >= 0x01E81000 && addr < 0x01E82000)
    {
        *retValue = cheatPage[addr - 0x01E81000];
        return true;
    }
    if (Cheat_IsValidAddress(processHandle, addr, 1))
    {
        Result res = svcReadProcessMemory(&ReadWriteBuffer8, processHandle, addr, 1);
        *retValue = *((u8*) (&ReadWriteBuffer8));
        return R_SUCCEEDED(res);
    }
    return false;
}

static bool Cheat_Read16(const Handle processHandle, u32 offset, u16* retValue)
{
    u32 addr = *activeOffset() + offset;
    if (addr >= 0x01E81000 && addr + 1 < 0x01E82000)
    {
        *retValue = *(u16*)(cheatPage + addr - 0x01E81000);
        return true;
    }
    if (Cheat_IsValidAddress(processHandle, addr, 2))
    {
        Result res = svcReadProcessMemory(&ReadWriteBuffer16, processHandle, addr, 2);
        *retValue = *((u16*) (&ReadWriteBuffer16));
        return R_SUCCEEDED(res);
    }
    return false;
}

static bool Cheat_Read32(const Handle processHandle, u32 offset, u32* retValue)
{
    u32 addr = *activeOffset() + offset;
    if (addr >= 0x01E81000 && addr + 3 < 0x01E82000)
    {
        *retValue = *(u32*)(cheatPage + addr - 0x01E81000);
        return true;
    }
    if (Cheat_IsValidAddress(processHandle, addr, 4))
    {
        Result res = svcReadProcessMemory(&ReadWriteBuffer32, processHandle, addr, 4);
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

static u32 Cheat_ApplyCheat(const Handle processHandle, CheatDescription* const cheat)
{
    cheat_state.index = 0;
    cheat_state.offset1 = 0;
    cheat_state.offset2 = 0;
    cheat_state.data1 = 0;
    cheat_state.data2 = 0;
    cheat_state.activeOffset = 0;
    cheat_state.activeData = 0;
    cheat_state.conditionalMode = 0;
    cheat_state.data1Mode = 0;
    cheat_state.data2Mode = 0;
    cheat_state.floatMode = 0;
    cheat_state.loopCount = 0;
    cheat_state.loopLine = -1;
    cheat_state.ifStack = 0;
    cheat_state.storedStack = 0;
    cheat_state.ifCount = 0;
    cheat_state.storedIfCount = 0;

    while (cheat_state.index < cheat->codesCount)
    {
        bool skipExecution = (cheat_state.ifStack & 0x00000001) != 0;
        u32 arg0 = (u32) ((cheat->codes[cheat_state.index] >> 32) & 0x00000000FFFFFFFFULL);
        u32 arg1 = (u32) ((cheat->codes[cheat_state.index]) & 0x00000000FFFFFFFFULL);
        if (arg0 == 0 && arg1 == 0)
        {
            return 0;
        }
        u32 code = ((arg0 >> 28) & 0x0F);
        u32 subcode = ((arg0 >> 24) & 0x0F);
        u32 codeArg = arg0 & 0x0F;

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
                bool newSkip;
                u32 value = 0;
                switch (cheat_state.conditionalMode)
                {
                    case 0x0:
                        if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !(value < arg1);
                        break;
                    case 0x1:
                        if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !(value < *activeData());
                        break;
                    case 0x2:
                        newSkip = !(*activeData() < arg1);
                        break;
                    case 0x3:
                        newSkip = !(*activeStorage(cheat) < arg1);
                        break;
                    case 0x4:
                        newSkip = !(*activeData() < *activeStorage(cheat));
                        break;
                    default:
                        return 0;
                }
                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
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
                bool newSkip;
                u32 value = 0;
                switch (cheat_state.conditionalMode)
                {
                    case 0x0:
                        if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !(value > arg1);
                        break;
                    case 0x1:
                        if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !(value > *activeData());
                        break;
                    case 0x2:
                        newSkip = !(*activeData() > arg1);
                        break;
                    case 0x3:
                        newSkip = !(*activeStorage(cheat) > arg1);
                        break;
                    case 0x4:
                        newSkip = !(*activeData() > *activeStorage(cheat));
                        break;
                    default:
                        return 0;
                }
                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
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
                bool newSkip;
                u32 value = 0;
                switch (cheat_state.conditionalMode)
                {
                    case 0x0:
                        if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !(value == arg1);
                        break;
                    case 0x1:
                        if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !(value == *activeData());
                        break;
                    case 0x2:
                        newSkip = !(*activeData() == arg1);
                        break;
                    case 0x3:
                        newSkip = !(*activeStorage(cheat) == arg1);
                        break;
                    case 0x4:
                        newSkip = !(*activeData() == *activeStorage(cheat));
                        break;
                    default:
                        return 0;
                }
                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
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
                bool newSkip;
                u32 value = 0;
                switch (cheat_state.conditionalMode)
                {
                    case 0x0:
                        if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !(value != arg1);
                        break;
                    case 0x1:
                        if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !(value != *activeData());
                        break;
                    case 0x2:
                        newSkip = !(*activeData() != arg1);
                        break;
                    case 0x3:
                        newSkip = !(*activeStorage(cheat) != arg1);
                        break;
                    case 0x4:
                        newSkip = !(*activeData() != *activeStorage(cheat));
                        break;
                    default:
                        return 0;
                }
                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
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
                bool newSkip;
                u16 mask = (u16) ((arg1 >> 16) & 0xFFFF);
                u16 value = 0;
                switch (cheat_state.conditionalMode)
                {
                    case 0x0:
                        if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !((value & (~mask)) < (arg1 & 0xFFFF));
                        break;
                    case 0x1:
                        if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !((value & (~mask)) < (*activeData() & (~mask)));
                        break;
                    case 0x2:
                        newSkip = !((*activeData() & (~mask)) < (arg1 & 0xFFFF));
                        break;
                    case 0x3:
                        newSkip = !((*activeStorage(cheat) & (~mask)) < (arg1 & 0xFFFF));
                        break;
                    case 0x4:
                        newSkip = !((*activeData() & (~mask)) < (*activeStorage(cheat) & (~mask)));
                        break;
                    default:
                        return 0;
                }
                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
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
                bool newSkip;
                u16 mask = (u16) ((arg1 >> 16) & 0xFFFF);
                u16 value = 0;
                switch (cheat_state.conditionalMode)
                {
                    case 0x0:
                        if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !((value & (~mask)) > (arg1 & 0xFFFF));
                        break;
                    case 0x1:
                        if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !((value & (~mask)) > (*activeData() & (~mask)));
                        break;
                    case 0x2:
                        newSkip = !((*activeData() & (~mask)) > (arg1 & 0xFFFF));
                        break;
                    case 0x3:
                        newSkip = !((*activeStorage(cheat) & (~mask)) > (arg1 & 0xFFFF));
                        break;
                    case 0x4:
                        newSkip = !((*activeData() & (~mask)) > (*activeStorage(cheat) & (~mask)));
                        break;
                    default:
                        return 0;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
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
                bool newSkip;
                u16 mask = (u16) ((arg1 >> 16) & 0xFFFF);
                u16 value = 0;
                switch (cheat_state.conditionalMode)
                {
                    case 0x0:
                        if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !((value & (~mask)) == (arg1 & 0xFFFF));
                        break;
                    case 0x1:
                        if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !((value & (~mask)) == (*activeData() & (~mask)));
                        break;
                    case 0x2:
                        newSkip = !((*activeData() & (~mask)) == (arg1 & 0xFFFF));
                        break;
                    case 0x3:
                        newSkip = !((*activeStorage(cheat) & (~mask)) == (arg1 & 0xFFFF));
                        break;
                    case 0x4:
                        newSkip = !((*activeData() & (~mask)) == (*activeStorage(cheat) & (~mask)));
                        break;
                    default:
                        return 0;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
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
                bool newSkip;
                u16 mask = (u16) ((arg1 >> 16) & 0xFFFF);
                u16 value = 0;
                switch (cheat_state.conditionalMode)
                {
                    case 0x0:
                        if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !((value & (~mask)) != (arg1 & 0xFFFF));
                        break;
                    case 0x1:
                        if (!Cheat_Read16(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                        newSkip = !((value & (~mask)) != (*activeData() & (~mask)));
                        break;
                    case 0x2:
                        newSkip = !((*activeData() & (~mask)) != (arg1 & 0xFFFF));
                        break;
                    case 0x3:
                        newSkip = !((*activeStorage(cheat) & (~mask)) != (arg1 & 0xFFFF));
                        break;
                    case 0x4:
                        newSkip = !((*activeData() & (~mask)) != (*activeStorage(cheat) & (~mask)));
                        break;
                    default:
                        return 0;
                }

                cheat_state.ifStack <<= 1;
                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
                cheat_state.ifCount++;
            }
                break;

            case 0xB:
                // B Type
                // Format: BXXXXXXX 00000000
                // Description: Loads offset register with value at given XXXXXXX
                if (!skipExecution)
                {
                    u32 value;
                    if (!Cheat_Read32(processHandle, arg0 & 0x0FFFFFFF, &value)) return 0;
                    *activeOffset() = value;
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
                switch (subcode)
                {
                    case 0x00:
                        cheat_state.loopLine = cheat_state.index;
                        cheat_state.loopCount = arg1;
                        cheat_state.storedStack = cheat_state.ifStack;
                        cheat_state.storedIfCount = cheat_state.ifCount;
                        break;
                    case 0x01:
                        cheat_state.loopLine = cheat_state.index;
                        cheat_state.loopCount = cheat_state.data1;
                        cheat_state.storedStack = cheat_state.ifStack;
                        cheat_state.storedIfCount = cheat_state.ifCount;
                        break;
                    case 0x02:
                        cheat_state.loopLine = cheat_state.index;
                        cheat_state.loopCount = cheat_state.data2;
                        cheat_state.storedStack = cheat_state.ifStack;
                        cheat_state.storedIfCount = cheat_state.ifCount;
                        break;
                }
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
                        if (arg1 == 0)
                        {
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
                                        else if (cheat_state.loopLine != -1)
                                        {
                                            cheat_state.index = cheat_state.loopLine;
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
                        }
                        // D0000000 00000001
                        // Loop break
                        else if (!skipExecution && arg1 == 1)
                        {
                            cheat_state.loopCount = 0;
                            cheat_state.loopLine = -1;
                            cheat_state.index++;
                            while (cheat_state.index < cheat->codesCount)
                            {
                                u64 code = cheat->codes[cheat_state.index++];
                                if (code == 0xD100000000000000ull || code == 0xD200000000000000ull)
                                {
                                    break;
                                }
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
                        if (arg1 == 0)
                        {
                            if (cheat_state.loopCount > 0)
                            {
                                cheat_state.loopCount--;
                                if (cheat_state.loopCount == 0)
                                {
                                    *activeData() = 0;
                                    *activeOffset() = 0;
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
                                *activeData() = 0;
                                *activeOffset() = 0;
                                cheat_state.ifStack = 0;
                                cheat_state.ifCount = 0;
                            }
                        }
                        // D2000000 00000001
                        // Return
                        else if (!skipExecution && arg1 == 1)
                        {
                            cheat_state.index = cheat->codesCount;
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
                            if (codeArg == 0)
                            {
                                cheat_state.offset1 = arg1;
                            }
                            else if (codeArg == 1)
                            {
                                cheat_state.offset2 = arg1;
                            }
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
                            if (codeArg == 0)
                            {
                                *activeData() += arg1;
                            }
                            else if (codeArg == 1)
                            {
                                cheat_state.data1 += arg1 + cheat_state.data2;
                            }
                            else if (codeArg == 2)
                            {
                                cheat_state.data2 += arg1 + cheat_state.data1;
                            }
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
                            if (codeArg == 0)
                            {
                                *activeData() = arg1;
                            }
                            else if (codeArg == 1)
                            {
                                cheat_state.data1 = arg1;
                            }
                            else if (codeArg == 2)
                            {
                                cheat_state.data2 = arg1;
                            }
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
                            if (codeArg == 0)
                            {
                                if (!Cheat_Write32(processHandle, arg1, *activeData())) return 0;
                                *activeOffset() += 4;
                            }
                            else if (codeArg == 1)
                            {
                                if (!Cheat_Write32(processHandle, arg1, cheat_state.data1)) return 0;
                                *activeOffset() += 4;
                            }
                            else if (codeArg == 2)
                            {
                                if (!Cheat_Write32(processHandle, arg1, cheat_state.data2)) return 0;
                                *activeOffset() += 4;
                            }
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
                            if (codeArg == 0)
                            {
                                if (!Cheat_Write16(processHandle, arg1, (u16) (*activeData() & 0xFFFF))) return 0;
                                *activeOffset() += 2;
                            }
                            else if (codeArg == 1)
                            {
                                if (!Cheat_Write16(processHandle, arg1, (u16) (cheat_state.data1 & 0xFFFF))) return 0;
                                *activeOffset() += 2;
                            }
                            else if (codeArg == 2)
                            {
                                if (!Cheat_Write16(processHandle, arg1, (u16) (cheat_state.data2 & 0xFFFF))) return 0;
                                *activeOffset() += 2;
                            }
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
                            if (codeArg == 0)
                            {
                                if (!Cheat_Write8(processHandle, arg1, (u8) (*activeData() & 0xFF))) return 0;
                                *activeOffset() += 1;
                            }
                            else if (codeArg == 1)
                            {
                                if (!Cheat_Write8(processHandle, arg1, (u8) (cheat_state.data1 & 0xFF))) return 0;
                                *activeOffset() += 1;
                            }
                            else if (codeArg == 2)
                            {
                                if (!Cheat_Write8(processHandle, arg1, (u8) (cheat_state.data2 & 0xFF))) return 0;
                                *activeOffset() += 1;
                            }
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
                            if (codeArg == 0)
                            {
                                u32 value = 0;
                                if (!Cheat_Read32(processHandle, arg1, &value)) return 0;
                                *activeData() = value;
                            }
                            else if (codeArg == 1)
                            {
                                u32 value = 0;
                                if (!Cheat_Read32(processHandle, arg1, &value)) return 0;
                                cheat_state.data1 = value;
                            }
                            else if (codeArg == 2)
                            {
                                u32 value = 0;
                                if (!Cheat_Read32(processHandle, arg1, &value)) return 0;
                                cheat_state.data2 = value;
                            }
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
                            if (codeArg == 0)
                            {
                                u16 value = 0;
                                if (!Cheat_Read16(processHandle, arg1, &value)) return 0;
                                *activeData() = value;
                            }
                            else if (codeArg == 1)
                            {
                                u16 value = 0;
                                if (!Cheat_Read16(processHandle, arg1, &value)) return 0;
                                cheat_state.data1 = value;
                            }
                            else if (codeArg == 2)
                            {
                                u16 value = 0;
                                if (!Cheat_Read16(processHandle, arg1, &value)) return 0;
                                cheat_state.data2 = value;
                            }
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
                            if (codeArg == 0)
                            {
                                u8 value = 0;
                                if (!Cheat_Read8(processHandle, arg1, &value)) return 0;
                                *activeData() = value;
                            }
                            else if (codeArg == 1)
                            {
                                u8 value = 0;
                                if (!Cheat_Read8(processHandle, arg1, &value)) return 0;
                                cheat_state.data1 = value;
                            }
                            else if (codeArg == 2)
                            {
                                u8 value = 0;
                                if (!Cheat_Read8(processHandle, arg1, &value)) return 0;
                                cheat_state.data2 = value;
                            }
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
                            *activeOffset() += arg1;
                        }
                        break;
                    case 0x0D:
                        // DD Type
                    {
                        bool newSkip = !(arg1 == 0 || (HID_PAD & arg1) == arg1);

                        cheat_state.ifStack <<= 1;
                        cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;;
                        cheat_state.ifCount++;
                    }
                        break;
                    case 0x0E:
                        // Touchpad conditional
                        // DE000000 AAAABBBB: AAAA >= X position >= BBBB
                        // DE000001 AAAABBBB: AAAA >= Y position >= BBBB
                    {
                        bool newSkip;
                        u32 highBound = arg1 >> 16;
                        u32 lowBound = arg1 & 0xFFFF;
                        touchPosition touch;
                        hidTouchRead(&touch);
                        if (codeArg == 0)
                        {
                            newSkip = !(lowBound <= touch.px && highBound >= touch.px);
                        }
                        else if (codeArg == 1)
                        {
                            newSkip = !(lowBound <= touch.py && highBound >= touch.py);
                        }
                        else
                        {
                            return 0;
                        }

                        cheat_state.ifStack <<= 1;
                        cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
                        cheat_state.ifCount++;
                    }
                        break;
                    case 0x0F:
                    {
                        switch (codeArg)
                        {
                            case 0x00:
                            {
                                if (arg1 & 0x00010000)
                                {
                                    if (arg1 & 0x1)
                                    {
                                        cheat_state.offset2 = cheat_state.offset1;
                                    }
                                    else
                                    {
                                        cheat_state.offset1 = cheat_state.offset2;
                                    }
                                }
                                else if (arg1 & 0x00020000)
                                {
                                    if (arg1 & 0x1)
                                    {
                                        cheat_state.data2 = cheat_state.offset2;
                                    }
                                    else
                                    {
                                        cheat_state.data1 = cheat_state.offset1;
                                    }
                                }
                                else
                                {
                                    cheat_state.activeOffset = arg1 & 0x1;
                                }
                            }
                                break;
                            case 0x01:
                            {
                                if (arg1 & 0x00010000)
                                {
                                    if (arg1 & 0x1)
                                    {
                                        cheat_state.data2 = cheat_state.data1;
                                    }
                                    else
                                    {
                                        cheat_state.data1 = cheat_state.data2;
                                    }
                                }
                                else if (arg1 & 0x00020000)
                                {
                                    if (arg1 & 0x1)
                                    {
                                        cheat_state.offset2 = cheat_state.data2;
                                    }
                                    else
                                    {
                                        cheat_state.offset1 = cheat_state.data1;
                                    }
                                }
                                else
                                {
                                    cheat_state.activeData = arg1 & 0x1;
                                }
                            }
                                break;
                            case 0x02:
                            {
                                if (arg1 & 0x00010000)
                                {
                                    if (arg1 & 0x1)
                                    {
                                        cheat_state.data2 = cheat->storage2;
                                    }
                                    else
                                    {
                                        cheat_state.data1 = cheat->storage1;
                                    }
                                }
                                else if (arg1 & 0x00020000)
                                {
                                    if (arg1 & 0x1)
                                    {
                                        cheat->storage2 = cheat_state.data2;
                                    }
                                    else
                                    {
                                        cheat->storage1 = cheat_state.data1;
                                    }
                                }
                                else
                                {
                                    cheat->activeStorage = arg1 & 0x1;
                                }
                            }
                                break;
                            case 0x0E:
                            {
                                if (cheat_state.activeData)
                                {
                                    switch (arg1)
                                    {
                                        case 0x0:
                                        {
                                            cheat_state.data2Mode = 0;
                                        }
                                            break;
                                        case 0x1:
                                        {
                                            cheat_state.data2Mode = 1;
                                        }
                                            break;
                                        case 0x10:
                                        {
                                            cheat_state.data2Mode = 0;
                                            float val;
                                            memcpy(&val, &cheat_state.data2, sizeof(float));
                                            cheat_state.data2 = val;
                                        }
                                            break;
                                        case 0x11:
                                        {
                                            cheat_state.data2Mode = 1;
                                            float val = cheat_state.data2;
                                            memcpy(&cheat_state.data2, &val, sizeof(float));
                                        }
                                            break;
                                        default:
                                            return 0;
                                    }
                                }
                                else
                                {
                                    switch (arg1)
                                    {
                                        case 0x0:
                                        {
                                            cheat_state.data1Mode = 0;
                                        }
                                            break;
                                        case 0x1:
                                        {
                                            cheat_state.data1Mode = 1;
                                        }
                                            break;
                                        case 0x10:
                                        {
                                            cheat_state.data1Mode = 0;
                                            float val;
                                            memcpy(&val, &cheat_state.data1, sizeof(float));
                                            cheat_state.data1 = val;
                                        }
                                            break;
                                        case 0x11:
                                        {
                                            cheat_state.data1Mode = 1;
                                            float val = cheat_state.data1;
                                            memcpy(&cheat_state.data1, &val, sizeof(float));
                                        }
                                            break;
                                        default:
                                            return 0;
                                    }
                                }
                            }
                                break;
                            case 0x0F:
                            {
                                if (arg1 < 5)
                                {
                                    cheat_state.conditionalMode = (u8)arg1;
                                }
                                else
                                {
                                    return 0;
                                }
                            }
                                break;
                            default:
                                return 0;
                        }
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
            case 0xF:
            {
                if (arg0 == 0xF0F00000)
                {
                    // I have no clue how to implement this, or if it's even possible. Needs research.
                    return 0;
                }
                else
                {
                    switch (subcode)
                    {
                        case 0x0:
                        {
                            if(!skipExecution)
                            {
                                cheat_state.floatMode = arg1 & 0x1;
                            }
                        }
                            break;
                        case 0x1:
                        {
                            if (!skipExecution)
                            {
                                if (cheat_state.floatMode)
                                {
                                    float flarg1;
                                    memcpy(&flarg1, &arg1, sizeof(float));
                                    u32 tmp;
                                    if (!Cheat_Read32(processHandle, arg0 & 0x00FFFFFF, &tmp))
                                    {
                                        return 0;
                                    }
                                    float value;
                                    memcpy(&value, &tmp, sizeof(float));
                                    value += flarg1;
                                    memcpy(&tmp, &value, sizeof(u32));
                                    if (!Cheat_Write32(processHandle, arg0 & 0x00FFFFFF, tmp))
                                    {
                                        return 0;
                                    }
                                }
                                else
                                {
                                    u32 tmp;
                                    if (!Cheat_Read32(processHandle, arg0 & 0x00FFFFFF, &tmp))
                                    {
                                        return 0;
                                    }
                                    tmp += arg1;
                                    if (!Cheat_Write32(processHandle, arg0 & 0x00FFFFFF, tmp))
                                    {
                                        return 0;
                                    }
                                }
                            }
                        }
                            break;
                        case 0x2:
                        {
                            if (!skipExecution)
                            {
                                if (cheat_state.floatMode)
                                {
                                    float flarg1;
                                    memcpy(&flarg1, &arg1, sizeof(float));
                                    u32 tmp;
                                    if (!Cheat_Read32(processHandle, arg0 & 0x00FFFFFF, &tmp))
                                    {
                                        return 0;
                                    }
                                    float value;
                                    memcpy(&value, &tmp, sizeof(float));
                                    value *= flarg1;
                                    memcpy(&tmp, &value, sizeof(u32));
                                    if (!Cheat_Write32(processHandle, arg0 & 0x00FFFFFF, tmp))
                                    {
                                        return 0;
                                    }
                                }
                                else
                                {
                                    u32 tmp;
                                    if (!Cheat_Read32(processHandle, arg0 & 0x00FFFFFF, &tmp))
                                    {
                                        return 0;
                                    }
                                    tmp *= arg1;
                                    if (!Cheat_Write32(processHandle, arg0 & 0x00FFFFFF, tmp))
                                    {
                                        return 0;
                                    }
                                }
                            }
                        }
                            break;
                        case 0x3:
                        {
                            if (!skipExecution)
                            {
                                if (cheat_state.floatMode)
                                {
                                    float flarg1;
                                    memcpy(&flarg1, &arg1, sizeof(float));
                                    u32 tmp;
                                    if (!Cheat_Read32(processHandle, arg0 & 0x00FFFFFF, &tmp))
                                    {
                                        return 0;
                                    }
                                    float value;
                                    memcpy(&value, &tmp, sizeof(float));
                                    value /= flarg1;
                                    memcpy(&tmp, &value, sizeof(u32));
                                    if (!Cheat_Write32(processHandle, arg0 & 0x00FFFFFF, tmp))
                                    {
                                        return 0;
                                    }
                                }
                                else
                                {
                                    u32 tmp;
                                    if (!Cheat_Read32(processHandle, arg0 & 0x00FFFFFF, &tmp))
                                    {
                                        return 0;
                                    }
                                    tmp /= arg1;
                                    if (!Cheat_Write32(processHandle, arg0 & 0x00FFFFFF, tmp))
                                    {
                                        return 0;
                                    }
                                }
                            }
                        }
                            break;
                        case 0x4:
                        {
                            if (!skipExecution)
                            {
                                if (cheat_state.data1Mode)
                                {
                                    float flarg1;
                                    memcpy(&flarg1, &arg1, sizeof(float));
                                    float value;
                                    memcpy(&value, activeData(), sizeof(float));
                                    value *= flarg1;
                                    memcpy(activeData(), &value, sizeof(float));
                                }
                                else
                                {
                                    *activeData() *= arg1;
                                }
                            }
                        }
                            break;
                        case 0x5:
                        {
                            if (!skipExecution)
                            {
                                if (cheat_state.data1Mode)
                                {
                                    float flarg1;
                                    memcpy(&flarg1, &arg1, sizeof(float));
                                    float value;
                                    memcpy(&value, activeData(), sizeof(float));
                                    value /= flarg1;
                                    memcpy(activeData(), &value, sizeof(float));
                                }
                                else
                                {
                                    *activeData() /= arg1;
                                }
                            }
                        }
                            break;
                        case 0x6:
                        {
                            if (!skipExecution)
                            {
                                *activeData() &= arg1;
                            }
                        }
                            break;
                        case 0x7:
                        {
                            if (!skipExecution)
                            {
                                *activeData() |= arg1;
                            }
                        }
                            break;
                        case 0x8:
                        {
                            if (!skipExecution)
                            {
                                *activeData() ^= arg1;
                            }
                        }
                            break;
                        case 0x9:
                        {
                            if (!skipExecution)
                            {
                                *activeData() = ~*activeData();
                            }
                        }
                            break;
                        case 0xA:
                        {
                            if (!skipExecution)
                            {
                                *activeData() <<= arg1;
                            }
                        }
                            break;
                        case 0xB:
                        {
                            if (!skipExecution)
                            {
                                *activeData() >>= arg1;
                            }
                        }
                            break;
                        case 0xC:
                        {
                            if (!skipExecution)
                            {
                                u8 origActiveOffset = cheat_state.activeOffset;
                                for (size_t i = 0; i < arg1; i++)
                                {
                                    u8 data;
                                    cheat_state.activeOffset = 1;
                                    if (!Cheat_Read8(processHandle, 0, &data))
                                    {
                                        return 0;
                                    }
                                    cheat_state.activeOffset = 0;
                                    if (!Cheat_Write8(processHandle, 0, data))
                                    {
                                        return 0;
                                    }
                                }
                                cheat_state.activeOffset = origActiveOffset;
                            }
                        }
                            break;
                        // Search for pattern
                        case 0xE:
                        {
                            u32 searchSize = arg0 & 0xFFFF;
                            if (searchSize <= arg1 && searchSize + cheat_state.index < cheat->codesCount)
                            {
                                bool newSkip = true;
                                if (!skipExecution) // Don't do an expensive operation if we don't have to
                                {
                                    u8* searchData = (u8*)(cheat->codes + cheat_state.index + 1);
                                    cheat_state.index += searchSize / 8;
                                    if (searchSize & 0x7)
                                    {
                                        cheat_state.index++;
                                    }
                                    for (size_t i = 0; i < arg1 - searchSize; i++)
                                    {
                                        u8 curVal;
                                        newSkip = false;
                                        for (size_t j = 0; j < searchSize; j++)
                                        {
                                            if (!Cheat_Read8(processHandle, i + j, &curVal))
                                            {
                                                return 0;
                                            }
                                            if (curVal != searchData[j])
                                            {
                                                newSkip = 1;
                                                break;
                                            }
                                        }
                                        if (!newSkip)
                                        {
                                            break;
                                        }
                                    }
                                }

                                cheat_state.ifStack <<= 1;
                                cheat_state.ifStack |= (newSkip || skipExecution) ? 1 : 0;
                                cheat_state.ifCount++;
                            }
                            else
                            {
                                return 0;
                            }
                        }
                            break;
                        case 0xF:
                        {
                            if (!skipExecution)
                            {
                                u32 range = arg1 - (arg0 & 0xFFFFFF);
                                u32 number = Cheat_GetRandomNumber() % range;
                                *activeData() = (arg0 & 0xFFFFFF) + number;
                            }
                        }
                            break;
                        default:
                            return 0;
                    }
                }
            }
                break;
            // This should now not be possible
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
            sprintf(failureReason, "Debug process failed");
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
    cheat->hasKeyCode = 0;
    cheat->storage1 = 0;
    cheat->storage2 = 0;
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

static Result BufferedFile_Open(BufferedFile* file, FS_ArchiveID archiveId, FS_Path archivePath, FS_Path filePath, u32 flags)
{
    Result res = 0;
    memset(file->buffer, '\0', sizeof(file->buffer));
    res = IFile_Open(&file->file, archiveId, archivePath, filePath, flags);
    if (R_SUCCEEDED(res))
    {
        file->curPos = 0;
        res = IFile_Read(&file->file, &file->maxPos, file->buffer, sizeof(file->buffer));
    }
    return res;
}

static Result BufferedFile_Read(BufferedFile* file, u64* totalRead, void* buffer, u32 len)
{
    Result res = 0;
    if (len == 0)
    {
        *totalRead = 0;
        return 0;
    }
    else if (file->curPos + len < file->maxPos)
    {
        memcpy(buffer, file->buffer + file->curPos, len);
        file->curPos += len;
        *totalRead = len;
    }
    else
    {
        *totalRead = 0;
        while(R_SUCCEEDED(res) && file->maxPos != 0 && *totalRead < len)
        {
            u32 toRead = file->maxPos - file->curPos < len - *totalRead ? file->maxPos - file->curPos : len - *totalRead;
            memcpy(buffer + *totalRead, file->buffer + file->curPos, toRead);
            *totalRead += toRead;
            file->curPos += toRead;
            if (file->curPos >= file->maxPos)
            {
                res = IFile_Read(&file->file, &file->maxPos, file->buffer, sizeof(file->buffer));
                file->curPos = 0;
            }
        }
    }
    return res;
}

static Result Cheat_ReadLine(BufferedFile* file, char* line, u32 lineSize)
{
    Result res = 0;

    u32 idx = 0;
    u64 total = 0;
    bool lastWasCarriageReturn = false;
    while (R_SUCCEEDED(res) && idx < lineSize)
    {
        res = BufferedFile_Read(file, &total, line + idx, 1);
        if (total == 0)
        {
            line[idx] = '\0';
            return -1;
        }
        if (R_SUCCEEDED(res))
        {
            if (line[idx] == '\r')
            {
                lastWasCarriageReturn = true;
            }
            else if (line[idx] == '\n')
            {
                if (lastWasCarriageReturn)
                {
                    line[--idx] = '\0';
                    return idx;
                }
                else
                {
                    line[idx] = '\0';
                    return idx;
                }
            }
            else if (line[idx] == '\0')
            {
                return -1;
            }
            else
            {
                lastWasCarriageReturn = false;
            }
            idx++;
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

static char* stripWhitespace(char* in)
{
    char* ret = in;
    while (*ret == ' ' || *ret == '\t')
    {
        ret++;
    }
    int back = strlen(ret) - 1;
    while (back > 0 && (ret[back] == ' ' || ret[back] == '\t'))
    {
        back--;
    }
    ret[back+1] = '\0';
    return ret;
}

static void Cheat_LoadCheatsIntoMemory(u64 titleId)
{
    cheatCount = 0;
    cheatTitleInfo = titleId;

    char path[64] = { 0 };
    sprintf(path, "/luma/titles/%016llX/cheats.txt", titleId);

    BufferedFile file;

    if (R_FAILED(BufferedFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ)))
    {
        // OK, let's try another source
        sprintf(path, "/cheats/%016llX.txt", titleId);
        if (R_FAILED(BufferedFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ))) return;
    };

    char line[1024] = { 0 };
    Result res = 0;
    CheatDescription* cheat = 0;
    u32 cheatSize = 0;
    do
    {
        res = Cheat_ReadLine(&file, line, 1024);
        // -1 is special; it can't be a normal result because of how results are constructed
        // So let's just use it as a signal that this is the final line of a file
        if (R_SUCCEEDED(res) || res == -1)
        {
            char* strippedLine = stripWhitespace(line);
            s32 lineLen = strnlen(strippedLine, 1023);
            if (!lineLen)
            {
                continue;
            }
            if (strippedLine[0] == '#')
            {
                continue;
            }
            if (Cheat_IsCodeLine(strippedLine))
            {
                if (cheatSize + sizeof(u64) >= sizeof(cheatBuffer))
                {
                    cheatCount--;
                    break;
                }
                if (cheat)
                {
                    u64 tmp = Cheat_GetCode(strippedLine);
                    Cheat_AddCode(cheat, tmp);
                    cheatSize += sizeof(u64);
                    if (((tmp >> 32) & 0xFFFFFFFF) == 0xDD000000)
                    {
                        cheat->hasKeyCode = 1;
                    }
                }
            }
            else
            {
                if (!cheat || cheat->codesCount > 0)
                {
                    if (cheatSize + sizeof(CheatDescription) >= sizeof(cheatBuffer))
                    {
                        break;
                    }
                    cheat = Cheat_AllocCheat();
                    cheatSize += sizeof(CheatDescription);
                }
                strncpy(cheat->name, line, 38);
                cheat->name[38] = '\0';
            }
        }
    } while (R_SUCCEEDED(res) && cheatSize < sizeof(cheatBuffer));

    IFile_Close(&file.file);

    if ((cheatCount > 0) && (cheats[cheatCount - 1]->codesCount == 0))
    {
        cheatCount--; // Remove last empty cheat
    }

    memset(cheatPage, 0, 0x1000);
}

static u32 Cheat_GetCurrentProcessAndTitleId(u64* titleId)
{
    FS_ProgramInfo programInfo;
    u32 pid;
    u32 launchFlags;
    Result res = PMDBG_GetCurrentAppInfo(&programInfo, &pid, &launchFlags);
    if (R_FAILED(res)) {
        *titleId = 0;
        return 0xFFFFFFFF;
    }

    *titleId = programInfo.programId;
    return pid;
}

void Cheat_SeedRng(u64 seed)
{
    cheatRngState = seed;
}

void Cheat_ApplyCheats(void)
{
    if (!cheatCount)
    {
        return;
    }

    u64 titleId = 0;
    u32 pid = Cheat_GetCurrentProcessAndTitleId(&titleId);

    if (!titleId)
    {
        cheatCount = 0;
        return;
    }

    if (titleId != cheatTitleInfo)
    {
        cheatCount = 0;
        return;
    }

    for (int i = 0; i < cheatCount; i++)
    {
        if (cheats[i]->active)
        {
            Cheat_MapMemoryAndApplyCheat(pid, cheats[i]);
        } 
    }
}

void RosalinaMenu_Cheats(void)
{
    u64 titleId = 0;
    u32 pid = Cheat_GetCurrentProcessAndTitleId(&titleId);

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
        } while (!(waitInput() & KEY_B) && !menuShouldExit);
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
                    const char * keyAct = (cheats[j]->hasKeyCode ? "*" : " ");
                    sprintf(buf, "%s%s%s", checkbox, keyAct, cheats[j]->name);

                    Draw_DrawString(30, 30 + i * SPACING_Y, cheats[j]->valid ? COLOR_WHITE : COLOR_RED, buf);
                    Draw_DrawCharacter(10, 30 + i * SPACING_Y, COLOR_TITLE, j == selected ? '>' : ' ');
                }
            }
            else
            {
                Draw_DrawFormattedString(10, 10, COLOR_TITLE, "ERROR: %08lx", r);
                Draw_DrawFormattedString(10, 30, COLOR_RED, failureReason);
            }
            Draw_FlushFramebuffer();
            Draw_Unlock();

            if (menuShouldExit) break;

            u32 pressed;
            do
            {
                pressed = waitInputWithTimeout(50);
                if (pressed != 0) break;
            } while (pressed == 0 && !menuShouldExit);

            if (pressed & KEY_B)
                break;
            else if ((pressed & KEY_A) && R_SUCCEEDED(r))
            {
                if (cheats[selected]->active)
                {
                    cheats[selected]->active = 0;
                }
                else
                {
                    r = Cheat_MapMemoryAndApplyCheat(pid, cheats[selected]);
                }
            }
            else if (pressed & KEY_DOWN)
                selected++;
            else if (pressed & KEY_UP)
                selected--;
            else if (pressed & KEY_LEFT)
                selected -= CHEATS_PER_MENU_PAGE;
            else if (pressed & KEY_RIGHT)
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
        } while (!menuShouldExit);
    }

}
