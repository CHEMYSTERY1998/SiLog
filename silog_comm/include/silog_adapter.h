/**
 * @file silog_adapter.h
 * @brief SiLog 平台适配层和内存管理宏
 */

#ifndef SILOG_ADAPTER_H
#define SILOG_ADAPTER_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def STATIC
 * @brief 符号可见性控制宏
 * @note Debug/Test 版本：不隐藏符号，方便调试；Release 版本：隐藏符号
 */
#if defined(DEBUG) || defined(TEST)
#define STATIC
#else
#if defined(_MSC_VER)
#define STATIC static
#else
#define STATIC static __attribute__((visibility("hidden")))
#endif
#endif

/**
 * @def SILOG_WINDOWS
 * @brief Windows 平台定义
 */
/**
 * @def SILOG_LINUX
 * @brief Linux 平台定义
 */
#if defined(_WIN32) || defined(_WIN64)
#define SILOG_WINDOWS
#else
#define SILOG_LINUX
#endif

/**
 * @brief 分配内存
 * @param size 分配大小
 * @return void* 分配的内存指针
 */
static inline void *SiMalloc(size_t size)
{
    return malloc(size);
}

/**
 * @brief 释放内存
 * @param ptr 内存指针
 */
static inline void SiFree(void *ptr)
{
    free(ptr);
}

/**
 * @brief 分配并清零内存
 * @param size 分配大小
 * @return void* 分配的内存指针
 */
static inline void *SiCalloc(size_t size)
{
    return calloc(1, size);
}

#ifdef __cplusplus
}
#endif

#endif /* SILOG_ADAPTER_H */
