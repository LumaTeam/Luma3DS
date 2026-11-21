#include <string.h>
#include "exefsredir.h"

extern const u8 exefsRedirPatch[];
extern const u32 exefsRedirPatchSize;

bool patchHomeMenuExefs(u8 *code, u32 size, u32 textSize, u32 reservedPadding)
{
    const u32 payloadSize = exefsRedirPatchSize;
    if(textSize < 8 || textSize > size || textSize > 0xFFFFF000 || (textSize & 3)) return false;

    const u32 roundedTextSize = (textSize + 0xFFF) & ~0xFFF;
    if(roundedTextSize > size || payloadSize > roundedTextSize - textSize ||
       reservedPadding > roundedTextSize - textSize - payloadSize) return false;

    const u32 payloadOffset = (roundedTextSize - payloadSize) & ~3;
    if(payloadOffset < textSize + reservedPadding) return false;
    for(u32 i = payloadOffset; i < roundedTextSize; i++)
        if(code[i] != 0) return false;

    // Find the ARM SDK OpenFileDirectly implementation by an actual literal
    // load of its IPC header, not by the presence of that constant alone.
    // Only redirect its call to the two-instruction svcSendSyncRequest stub.
    u32 call = 0xFFFFFFFF;
    const u32 *words = (const u32 *)code;
    for(u32 pos = 0; pos <= textSize - 8; pos += 4)
    {
        const u32 insn = words[pos / 4];
        if((insn & 0xFFFF0000) != 0xE59F0000) continue; // ldr rN, [pc, #imm]
        const u32 literal = pos + 8 + (insn & 0xFFF);
        if((literal & 3) || literal > textSize - 4 || words[literal / 4] != 0x08030204) continue;

        u32 start = pos;
        while(start >= 4 && (words[start / 4] & 0xFFFF4000) != 0xE92D4000) start -= 4;
        if((words[start / 4] & 0xFFFF4000) != 0xE92D4000) continue;

        u32 candidate = 0xFFFFFFFF;
        for(u32 at = start + 4; at < literal; at += 4)
        {
            const u32 branch = words[at / 4];
            if((branch & 0xFFFF0000) == 0xE92D0000) break;
            if((branch & 0xFF000000) != 0xEB000000) continue;
            const s32 displacement = ((s32)(branch << 8) >> 8) * 4;
            const s64 target = (s64)at + 8 + displacement;
            if(target < 0 || target > textSize - 8) continue;
            if(words[target / 4] != 0xEF000032 || words[target / 4 + 1] != 0xE12FFF1E) continue;
            if(candidate != 0xFFFFFFFF) return false;
            candidate = at;
        }
        if(candidate == 0xFFFFFFFF) continue;
        if(call != 0xFFFFFFFF && call != candidate) return false;
        call = candidate;
    }

    if(call == 0xFFFFFFFF) return false;
    const s64 displacement = (s64)payloadOffset - call - 8;
    if(displacement < -0x2000000 || displacement > 0x1FFFFFC) return false;

    memcpy(code + payloadOffset, exefsRedirPatch, payloadSize);
    *(u32 *)(code + call) = 0xEB000000 | (((u32)displacement >> 2) & 0xFFFFFF);
    return true;
}
