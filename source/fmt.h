#pragma once
#include "memory.h"
#include <stdarg.h>

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
