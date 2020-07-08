#include "strings.h"

void progIdToStr(char *strEnd, u64 progId)
{
    while(progId > 0)
    {
        static const char hexDigits[] = "0123456789ABCDEF";
        *strEnd-- = hexDigits[(u32)(progId & 0xF)];
        progId >>= 4;
    }
}
