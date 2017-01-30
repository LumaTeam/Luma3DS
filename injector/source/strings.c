#include "strings.h"

size_t strnlen(const char *string, size_t maxlen)
{
    size_t size;

    for(size = 0; *string && size < maxlen; string++, size++);

    return size;
}

void progIdToStr(char *strEnd, u64 progId)
{
    while(progId > 0)
    {
        static const char hexDigits[] = "0123456789ABCDEF";
        *strEnd-- = hexDigits[(u32)(progId & 0xF)];
        progId >>= 4;
    }
}