#pragma once
#include "memory.h"
#include <stdarg.h>

u32 vsprintf(char *buf, const char *fmt, va_list args);
u32 sprintf(char *buf, const char *fmt, ...);
