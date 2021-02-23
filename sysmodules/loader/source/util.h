#pragma once

#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/os.h>
#include <3ds/srv.h>

#define REG32(reg)              (*(vu32 *)reg)
#define REG64(reg)              (*(vu64 *)reg)

#define TRY(expr)               if(R_FAILED(res = (expr))) return res;
#define TRYG(expr, label)       if(R_FAILED(res = (expr))) goto label;

#ifdef XDS
static void hexItoa(u64 number, char *out, u32 digits, bool uppercase)
{
    const char hexDigits[] = "0123456789ABCDEF";
    const char hexDigitsLowercase[] = "0123456789abcdef";
    u32 i = 0;

    while(number > 0)
    {
        out[digits - 1 - i++] = uppercase ? hexDigits[number & 0xF] : hexDigitsLowercase[number & 0xF];
        number >>= 4;
    }

    while(i < digits) out[digits - 1 - i++] = '0';
}

static inline void debugOutputHex(u64 number, u32 digits)
{
    char buf[16+2];
    hexItoa(number, buf, digits, false);
    buf[digits] = '\n';
    buf[digits + 1] = '\0';

    svcOutputDebugString(buf, digits + 1);
}

#endif

static void __attribute__((noinline)) panic(Result res)
{
#ifndef XDS
    (void)res;
    __builtin_trap();
#else
    debugOutputHex(res, 8);
    svcBreak(USERBREAK_PANIC);
#endif
}

static inline Result assertSuccess(Result res)
{
    if(R_FAILED(res)) {
        panic(res);
    }

    return res;
}
