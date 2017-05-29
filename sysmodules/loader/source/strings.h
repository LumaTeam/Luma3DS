#pragma once

#include <3ds/types.h>

size_t strnlen(const char *string, size_t maxlen);
void progIdToStr(char *strEnd, u64 progId);