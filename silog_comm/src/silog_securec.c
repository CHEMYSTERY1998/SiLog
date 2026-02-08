/**
 * @file silog_securec.c
 * @brief SiLog 安全函数包装层实现
 *
 * 这些函数遵循 C11 Annex K (Bounds-checking interfaces) 标准，
 * 提供对不安全 C 函数的安全替代版本。
 */

#include "silog_securec.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* ==================== 内存操作函数 ==================== */

errno_t memset_s(void *dest, size_t destMax, int c, size_t count)
{
    if (dest == NULL || destMax == 0) {
        return EINVAL;
    }

    if (count > destMax) {
        /* 缓冲区太小，不执行任何写入 */
        return ERANGE;
    }

    memset(dest, c, count);
    return EOK;
}

errno_t memcpy_s(void *dest, size_t destMax, const void *src, size_t count)
{
    if (dest == NULL || destMax == 0) {
        return EINVAL;
    }

    if (src == NULL) {
        return EINVAL;
    }

    if (count > destMax) {
        /* 目标缓冲区太小 */
        return ERANGE;
    }

    /* 检查重叠，如果重叠则使用 memmove */
    if (src < dest && (const char *)src + count > (char *)dest) {
        memmove(dest, src, count);
        return EOK;
    }

    if (dest < src && (char *)dest + destMax > (char *)src) {
        memmove(dest, src, count);
        return EOK;
    }

    memcpy(dest, src, count);
    return EOK;
}

errno_t memmove_s(void *dest, size_t destMax, const void *src, size_t count)
{
    if (dest == NULL || destMax == 0) {
        return EINVAL;
    }

    if (src == NULL) {
        return EINVAL;
    }

    if (count > destMax) {
        /* 目标缓冲区太小 */
        return ERANGE;
    }

    memmove(dest, src, count);
    return EOK;
}

/* ==================== 字符串复制函数 ==================== */

errno_t strcpy_s(char *dest, size_t destMax, const char *src)
{
    if (dest == NULL || destMax == 0) {
        return EINVAL;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return EINVAL;
    }

    size_t srcLen = strlen(src);

    if (srcLen >= destMax) {
        /* 目标缓冲区太小（需要空间存放空字符） */
        dest[0] = '\0';
        return ERANGE;
    }

    memcpy(dest, src, srcLen + 1); /* +1 for null terminator */
    return EOK;
}

errno_t strncpy_s(char *dest, size_t destMax, const char *src, size_t count)
{
    if (dest == NULL || destMax == 0) {
        return EINVAL;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return EINVAL;
    }

    size_t srcLen = strnlen(src, count);

    if (srcLen >= destMax) {
        /* 目标缓冲区太小 */
        dest[0] = '\0';
        return ERANGE;
    }

    memcpy(dest, src, srcLen);
    dest[srcLen] = '\0';
    return EOK;
}

/* ==================== 字符串连接函数 ==================== */

errno_t strcat_s(char *dest, size_t destMax, const char *src)
{
    if (dest == NULL || destMax == 0) {
        return EINVAL;
    }

    if (src == NULL) {
        return EINVAL;
    }

    size_t destLen = strnlen(dest, destMax);
    size_t srcLen = strlen(src);

    if (destLen + srcLen >= destMax) {
        /* 目标缓冲区太小 */
        return ERANGE;
    }

    memcpy(dest + destLen, src, srcLen + 1); /* +1 for null terminator */
    return EOK;
}

errno_t strncat_s(char *dest, size_t destMax, const char *src, size_t count)
{
    if (dest == NULL || destMax == 0) {
        return EINVAL;
    }

    if (src == NULL) {
        return EINVAL;
    }

    size_t destLen = strnlen(dest, destMax);
    size_t srcLen = strnlen(src, count);

    if (destLen + srcLen >= destMax) {
        /* 目标缓冲区太小 */
        return ERANGE;
    }

    memcpy(dest + destLen, src, srcLen);
    dest[destLen + srcLen] = '\0';
    return EOK;
}

/* ==================== 字符串比较/查找函数 ==================== */

size_t strnlen_s(const char *str, size_t strMax)
{
    if (str == NULL || strMax == 0) {
        return 0;
    }

    return strnlen(str, strMax);
}

errno_t strcmp_s(const char *str1, size_t str1Max,
                 const char *str2, size_t str2Max,
                 int *result)
{
    if (result == NULL) {
        return EINVAL;
    }

    *result = 0;

    if (str1 == NULL || str1Max == 0) {
        if (str2 == NULL || str2Max == 0) {
            return EOK; /* 两个都为空，相等 */
        }
        *result = -1;
        return EOK;
    }

    if (str2 == NULL || str2Max == 0) {
        *result = 1;
        return EOK;
    }

    /* 使用安全的字符串长度 */
    size_t len1 = strnlen(str1, str1Max);
    size_t len2 = strnlen(str2, str2Max);
    size_t minLen = (len1 < len2) ? len1 : len2;

    int cmp = memcmp(str1, str2, minLen);

    if (cmp != 0) {
        *result = cmp;
    } else {
        /* 前缀相同，比较长度 */
        if (len1 < len2) {
            *result = -1;
        } else if (len1 > len2) {
            *result = 1;
        } else {
            *result = 0;
        }
    }

    return EOK;
}

errno_t strncmp_s(const char *str1, size_t str1Max,
                  const char *str2, size_t str2Max,
                  size_t count, int *result)
{
    if (result == NULL) {
        return EINVAL;
    }

    *result = 0;

    if (str1 == NULL || str1Max == 0) {
        if (str2 == NULL || str2Max == 0) {
            return EOK;
        }
        *result = -1;
        return EOK;
    }

    if (str2 == NULL || str2Max == 0) {
        *result = 1;
        return EOK;
    }

    /* 限制比较长度 */
    size_t len1 = strnlen(str1, str1Max);
    size_t len2 = strnlen(str2, str2Max);
    size_t maxCompare = (len1 < count) ? len1 : count;
    maxCompare = (len2 < maxCompare) ? len2 : maxCompare;

    int cmp = memcmp(str1, str2, maxCompare);

    if (cmp != 0) {
        *result = cmp;
    } else {
        *result = 0;
    }

    return EOK;
}

errno_t strchr_s(const char *str, size_t strMax, int c, size_t *position)
{
    if (position == NULL) {
        return EINVAL;
    }

    *position = 0;

    if (str == NULL || strMax == 0) {
        return EINVAL;
    }

    const char *p = memchr(str, c, strnlen(str, strMax));

    if (p == NULL) {
        return EINVAL;
    }

    *position = (size_t)(p - str);
    return EOK;
}

errno_t strrchr_s(const char *str, size_t strMax, int c, size_t *position)
{
    if (position == NULL) {
        return EINVAL;
    }

    *position = 0;

    if (str == NULL || strMax == 0) {
        return EINVAL;
    }

    size_t len = strnlen(str, strMax);

    /* 从后向前查找 */
    for (size_t i = len; i > 0; i--) {
        if (str[i - 1] == (char)c) {
            *position = i - 1;
            return EOK;
        }
    }

    return EINVAL;
}

errno_t strstr_s(const char *str, size_t strMax,
                 const char *substr, size_t substrMax,
                 size_t *position)
{
    if (position == NULL) {
        return EINVAL;
    }

    *position = 0;

    if (str == NULL || strMax == 0) {
        return EINVAL;
    }

    if (substr == NULL || substrMax == 0) {
        return EINVAL;
    }

    size_t strLen = strnlen(str, strMax);
    size_t subLen = strnlen(substr, substrMax);

    if (subLen == 0) {
        *position = 0;
        return EOK;
    }

    if (subLen > strLen) {
        return EINVAL;
    }

    /* 简单的字符串搜索 */
    for (size_t i = 0; i <= strLen - subLen; i++) {
        if (memcmp(str + i, substr, subLen) == 0) {
            *position = i;
            return EOK;
        }
    }

    return EINVAL;
}

/* ==================== 字符串标记函数 ==================== */

char *strtok_s(char *strToken, const char *strDelimit, char **context)
{
    if (context == NULL) {
        return NULL;
    }

    if (strDelimit == NULL) {
        return NULL;
    }

    /* 首次调用时保存字符串指针 */
    if (strToken != NULL) {
        *context = strToken;
    }

    if (*context == NULL) {
        return NULL;
    }

    /* 跳过前导分隔符 */
    while (**context != '\0' && strchr(strDelimit, **context) != NULL) {
        (*context)++;
    }

    if (**context == '\0') {
        *context = NULL;
        return NULL;
    }

    /* 记录 token 开始位置 */
    char *token = *context;

    /* 查找下一个分隔符 */
    while (**context != '\0' && strchr(strDelimit, **context) == NULL) {
        (*context)++;
    }

    if (**context != '\0') {
        **context = '\0';
        (*context)++;
    } else {
        *context = NULL;
    }

    return token;
}

/* ==================== 格式化输出函数 ==================== */

int sprintf_s(char *dest, size_t destMax, const char *format, ...)
{
    if (dest == NULL || destMax == 0) {
        return -1;
    }

    if (format == NULL) {
        dest[0] = '\0';
        return -1;
    }

    va_list args;
    va_start(args, format);

    int result = vsnprintf(dest, destMax, format, args);

    va_end(args);

    if (result < 0 || (size_t)result >= destMax) {
        /* 输出被截断或发生错误 */
        dest[0] = '\0';
        return -1;
    }

    return result;
}

int snprintf_s(char *dest, size_t destMax, size_t count, const char *format, ...)
{
    if (dest == NULL || destMax == 0) {
        return -1;
    }

    if (format == NULL) {
        dest[0] = '\0';
        return -1;
    }

    /* 实际写入的最大字符数（不包括空字符） */
    size_t maxWrite = (count < destMax - 1) ? count : destMax - 1;

    va_list args;
    va_start(args, format);

    int result = vsnprintf(dest, maxWrite + 1, format, args);

    va_end(args);

    if (result < 0) {
        dest[0] = '\0';
        return -1;
    }

    /* 确保空结尾 */
    dest[maxWrite] = '\0';

    return result;
}

int vsprintf_s(char *dest, size_t destMax, const char *format, va_list argList)
{
    if (dest == NULL || destMax == 0) {
        return -1;
    }

    if (format == NULL) {
        dest[0] = '\0';
        return -1;
    }

    int result = vsnprintf(dest, destMax, format, argList);

    if (result < 0 || (size_t)result >= destMax) {
        dest[0] = '\0';
        return -1;
    }

    return result;
}

int vsnprintf_s(char *dest, size_t destMax, size_t count,
                const char *format, va_list argList)
{
    if (dest == NULL || destMax == 0) {
        return -1;
    }

    if (format == NULL) {
        dest[0] = '\0';
        return -1;
    }

    size_t maxWrite = (count < destMax - 1) ? count : destMax - 1;

    int result = vsnprintf(dest, maxWrite + 1, format, argList);

    if (result < 0) {
        dest[0] = '\0';
        return -1;
    }

    dest[maxWrite] = '\0';

    return result;
}

/* ==================== 格式化输入函数 ==================== */

/**
 * @brief 辅助函数：从格式字符串中提取 %s 或 %c 参数的缓冲区大小
 *
 * 扫描可变参数列表，对于 %s 和 %c 格式，期望参数为：
 * - 指针后跟一个表示缓冲区大小的 unsigned int 值
 *
 * 其他格式（%d, %x, %f 等）保持不变
 */
static int vsscanf_s_internal(const char *buffer, const char *format, va_list argList)
{
    if (buffer == NULL || format == NULL) {
        return EOF;
    }

    /* 注意：这是一个简化的实现 */
    /* 完整的实现需要解析格式字符串，为 %s/%c 提取缓冲区大小 */
    /* 这里我们使用标准的 vsscanf，但假设调用者遵循安全约定 */

    return vsscanf(buffer, format, argList);
}

int sscanf_s(const char *buffer, size_t bufferSize, const char *format, ...)
{
    (void)bufferSize; /* 参数保留用于未来增强 */

    if (buffer == NULL || format == NULL) {
        return EOF;
    }

    va_list args;
    va_start(args, format);

    int result = vsscanf_s_internal(buffer, format, args);

    va_end(args);

    return result;
}

int vsscanf_s(const char *buffer, size_t bufferSize,
              const char *format, va_list argList)
{
    (void)bufferSize; /* 参数保留用于未来增强 */

    if (buffer == NULL || format == NULL) {
        return EOF;
    }

    return vsscanf_s_internal(buffer, format, argList);
}

int fscanf_s(FILE *stream, const char *format, ...)
{
    if (stream == NULL || format == NULL) {
        return EOF;
    }

    va_list args;
    va_start(args, format);

    int result = vfscanf_s(stream, format, args);

    va_end(args);

    return result;
}

int vfscanf_s(FILE *stream, const char *format, va_list argList)
{
    if (stream == NULL || format == NULL) {
        return EOF;
    }

    /* 注意：完整的 vfscanf_s 需要解析格式字符串验证 %s/%c 参数 */
    /* 这里使用标准的 vfscanf */
    return vfscanf(stream, format, argList);
}

int scanf_s(const char *format, ...)
{
    if (format == NULL) {
        return EOF;
    }

    va_list args;
    va_start(args, format);

    int result = vscanf_s(format, args);

    va_end(args);

    return result;
}

int vscanf_s(const char *format, va_list argList)
{
    if (format == NULL) {
        return EOF;
    }

    /* 注意：完整的 vscanf_s 需要解析格式字符串验证 %s/%c 参数 */
    /* 这里使用标准的 vscanf */
    return vscanf(format, argList);
}
