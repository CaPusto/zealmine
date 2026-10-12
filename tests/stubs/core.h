#ifndef TEST_CORE_H
#define TEST_CORE_H

/* Host equivalents of the target coreutils memory routines. */
#include <stdint.h>
#include <string.h>

static inline void* mem_set(void* ptr, uint8_t value, size_t size)
{
    return memset(ptr, value, size);
}

static inline void* mem_cpy(void* dst, const void* src, size_t size)
{
    return memcpy(dst, src, size);
}

#endif
