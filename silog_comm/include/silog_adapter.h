#ifndef SILOG_CONFIG_H
#define SILOG_CONFIG_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DEBUG) || defined(TEST)
// Debug/Test 版本：不隐藏符号，方便调试
#define STATIC
#else
// Release 版本：隐藏符号
#if defined(_MSC_VER)
#define STATIC static
#else
#define STATIC static __attribute__((visibility("hidden")))
#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
#define SILOG_WINDOWS
#else
#define SILOG_LINUX
#endif

static inline void *SiMalloc(size_t size)
{
    return malloc(size);
}

static inline void SiFree(void *ptr)
{
    free(ptr);
}

static inline void *SiCalloc(size_t size)
{
    return calloc(1, size);
}

#ifdef __cplusplus
}
#endif

#endif // SILOG_CONFIG_H
