#ifndef DEBUG_H
#define DEBUG_H

#ifdef ENABLE_DEBUG
#include <stdio.h>
#define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#endif // DEBUG_H