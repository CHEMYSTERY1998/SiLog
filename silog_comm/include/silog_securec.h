/**
 * @file silog_securec.h
 * @brief SiLog 安全函数包装层
 *
 * 提供对不安全 C 函数的安全替代版本，防止缓冲区溢出等安全问题。
 * 这些函数遵循 C11 Annex K (Bounds-checking interfaces) 标准的命名约定。
 *
 * 安全函数命名规则：原函数名 + _s 后缀
 *
 * 内存操作函数：
 * - memset_s
 * - memcpy_s
 * - memmove_s
 *
 * 字符串复制函数：
 * - strcpy_s
 * - strncpy_s
 *
 * 字符串连接函数：
 * - strcat_s
 * - strncat_s
 *
 * 字符串比较/查找函数：
 * - strcmp_s
 * - strncmp_s
 * - strnlen_s
 * - strchr_s
 * - strrchr_s
 * - strstr_s
 *
 * 字符串标记函数：
 * - strtok_s
 *
 * 格式化输出函数：
 * - sprintf_s
 * - snprintf_s
 * - vsprintf_s
 * - vsnprintf_s
 *
 * 格式化输入函数：
 * - sscanf_s
 * - vsscanf_s
 * - fscanf_s
 * - vfscanf_s
 * - scanf_s
 * - vscanf_s
 */

#ifndef SILOG_SECURC_H
#define SILOG_SECURC_H

#include <errno.h>
#include <stddef.h>
#include <stdio.h>

/* 定义 errno_t 类型（C11 Annex K） */
#ifndef errno_t
typedef int errno_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码定义 */
#ifndef EOK
#define EOK 0
#endif

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef ERANGE
#define ERANGE 34
#endif

/* ==================== 内存操作函数 ==================== */

/**
 * @brief 安全的内存设置函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小
 * @param c 要设置的值
 * @param count 要设置的字节数
 * @return errno_t EOK 成功，EINVAL/ERANGE 失败
 *
 * 与标准 memset 不同，memset_s 会检查缓冲区边界，
 * 如果 destMax < count，则不会写入任何数据并返回错误。
 */
errno_t memset_s(void *dest, size_t destMax, int c, size_t count);

/**
 * @brief 安全的内存复制函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小
 * @param src 源缓冲区
 * @param count 要复制的字节数
 * @return errno_t EOK 成功，EINVAL/ERANGE 失败
 *
 * 与标准 memcpy 不同，memcpy_s 会检查：
 * 1. 目标缓冲区是否足够大 (destMax >= count)
 * 2. 源和目标是否重叠（如果重叠则使用 memmove）
 */
errno_t memcpy_s(void *dest, size_t destMax, const void *src, size_t count);

/**
 * @brief 安全的内存移动函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小
 * @param src 源缓冲区
 * @param count 要移动的字节数
 * @return errno_t EOK 成功，EINVAL/ERANGE 失败
 *
 * 与标准 memmove 不同，memmove_s 会检查缓冲区边界。
 * 正确处理源和目标内存区域重叠的情况。
 */
errno_t memmove_s(void *dest, size_t destMax, const void *src, size_t count);

/* ==================== 字符串复制函数 ==================== */

/**
 * @brief 安全的字符串复制函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小（包括空字符）
 * @param src 源字符串
 * @return errno_t EOK 成功，EINVAL/ERANGE 失败
 */
errno_t strcpy_s(char *dest, size_t destMax, const char *src);

/**
 * @brief 安全的字符串截断复制函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小（包括空字符）
 * @param src 源字符串
 * @param count 最大复制字符数
 * @return errno_t EOK 成功，EINVAL/ERANGE 失败
 */
errno_t strncpy_s(char *dest, size_t destMax, const char *src, size_t count);

/* ==================== 字符串连接函数 ==================== */

/**
 * @brief 安全的字符串连接函数
 *
 * @param dest 目标缓冲区（必须包含空结尾的字符串）
 * @param destMax 目标缓冲区最大大小（包括空字符）
 * @param src 要追加的源字符串
 * @return errno_t EOK 成功，EINVAL/ERANGE 失败
 */
errno_t strcat_s(char *dest, size_t destMax, const char *src);

/**
 * @brief 安全的字符串截断连接函数
 *
 * @param dest 目标缓冲区（必须包含空结尾的字符串）
 * @param destMax 目标缓冲区最大大小（包括空字符）
 * @param src 要追加的源字符串
 * @param count 最大追加字符数
 * @return errno_t EOK 成功，EINVAL/ERANGE 失败
 */
errno_t strncat_s(char *dest, size_t destMax, const char *src, size_t count);

/* ==================== 字符串比较/查找函数 ==================== */

/**
 * @brief 安全的字符串长度函数
 *
 * @param str 字符串
 * @param strMax 字符串最大长度（防止读取越界）
 * @return size_t 字符串长度（不包括空字符），如果字符串无效则返回 0
 *
 * 与标准 strlen 不同，strnlen_s 会限制最大扫描长度，
 * 防止读取到缓冲区末尾之后。
 */
size_t strnlen_s(const char *str, size_t strMax);

/**
 * @brief 安全的字符串比较函数
 *
 * @param str1 第一个字符串
 * @param str1Max 第一个字符串最大长度
 * @param str2 第二个字符串
 * @param str2Max 第二个字符串最大长度
 * @param result 比较结果输出：负数表示 str1 < str2，0 表示相等，正数表示 str1 > str2
 * @return errno_t EOK 成功，EINVAL 失败
 *
 * 与标准 strcmp 不同，strcmp_s 提供边界检查，
 * 并通过输出参数返回结果（更符合安全函数设计模式）。
 */
errno_t strcmp_s(const char *str1, size_t str1Max, const char *str2, size_t str2Max, int *result);

/**
 * @brief 安全的字符串截断比较函数
 *
 * @param str1 第一个字符串
 * @param str1Max 第一个字符串最大长度
 * @param str2 第二个字符串
 * @param str2Max 第二个字符串最大长度
 * @param count 最大比较字符数
 * @param result 比较结果输出
 * @return errno_t EOK 成功，EINVAL 失败
 */
errno_t strncmp_s(const char *str1, size_t str1Max, const char *str2, size_t str2Max, size_t count, int *result);

/**
 * @brief 安全的字符查找函数
 *
 * @param str 字符串
 * @param strMax 字符串最大长度
 * @param c 要查找的字符
 * @param position 查找结果输出：字符在字符串中的位置（从 0 开始）
 * @return errno_t EOK 找到，EINVAL 未找到或参数无效
 *
 * 与标准 strchr 不同，strchr_s 通过输出参数返回位置，
 * 并提供边界检查。
 */
errno_t strchr_s(const char *str, size_t strMax, int c, size_t *position);

/**
 * @brief 安全的反向字符查找函数
 *
 * @param str 字符串
 * @param strMax 字符串最大长度
 * @param c 要查找的字符
 * @param position 查找结果输出：字符在字符串中的位置（从 0 开始）
 * @return errno_t EOK 找到，EINVAL 未找到或参数无效
 *
 * 与标准 strrchr 不同，strrchr_s 通过输出参数返回位置。
 */
errno_t strrchr_s(const char *str, size_t strMax, int c, size_t *position);

/**
 * @brief 安全的子字符串查找函数
 *
 * @param str 字符串
 * @param strMax 字符串最大长度
 * @param substr 要查找的子字符串
 * @param substrMax 子字符串最大长度
 * @param position 查找结果输出：子字符串在字符串中的起始位置
 * @return errno_t EOK 找到，EINVAL 未找到或参数无效
 *
 * 与标准 strstr 不同，strstr_s 通过输出参数返回位置。
 */
errno_t strstr_s(const char *str, size_t strMax, const char *substr, size_t substrMax, size_t *position);

/* ==================== 字符串标记函数 ==================== */

/**
 * @brief 安全的字符串分割函数
 *
 * @param strToken 要分割的字符串（首次调用时传入，后续调用传入 NULL）
 * @param strDelimit 分隔符字符串
 * @param context 上下文指针（用于保存分割状态）
 * @return char* 分割后的子字符串，NULL 表示没有更多 token
 *
 * 与标准 strtok 不同，strtok_s 使用上下文参数而非静态变量，
 * 是线程安全的，并且提供边界检查。
 *
 * 使用示例：
 *   char *context;
 *   char *token = strtok_s(str, " \t\n", &context);
 *   while (token != NULL) {
 *       // 处理 token
 *       token = strtok_s(NULL, " \t\n", &context);
 *   }
 */
char *strtok_s(char *strToken, const char *strDelimit, char **context);

/* ==================== 格式化输出函数 ==================== */

/**
 * @brief 安全的格式化字符串输出函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小（包括空字符）
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return int 成功时返回写入的字符数（不包括空字符），失败时返回 -1
 */
int sprintf_s(char *dest, size_t destMax, const char *format, ...);

/**
 * @brief 安全的截断格式化字符串输出函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小（包括空字符）
 * @param count 最大输出字符数（不包括空字符）
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return int 成功时返回写入的字符数（不包括空字符），失败时返回 -1
 */
int snprintf_s(char *dest, size_t destMax, size_t count, const char *format, ...);

/**
 * @brief 安全的可变参数格式化字符串输出函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小（包括空字符）
 * @param format 格式化字符串
 * @param argList 可变参数列表
 * @return int 成功时返回写入的字符数（不包括空字符），失败时返回 -1
 */
int vsprintf_s(char *dest, size_t destMax, const char *format, va_list argList);

/**
 * @brief 安全的可变参数截断格式化字符串输出函数
 *
 * @param dest 目标缓冲区
 * @param destMax 目标缓冲区最大大小（包括空字符）
 * @param count 最大输出字符数（不包括空字符）
 * @param format 格式化字符串
 * @param argList 可变参数列表
 * @return int 成功时返回写入的字符数（不包括空字符），失败时返回 -1
 */
int vsnprintf_s(char *dest, size_t destMax, size_t count, const char *format, va_list argList);

/* ==================== 格式化输入函数 ==================== */

/**
 * @brief 安全的字符串格式化输入函数
 *
 * @param buffer 输入缓冲区
 * @param bufferSize 缓冲区大小
 * @param format 格式化字符串
 * @param ... 可变参数（指针）
 * @return int 成功时返回成功匹配的项目数，失败时返回 EOF
 *
 * 与标准 sscanf 不同，sscanf_s 对字符串格式（如 %s、%c）需要指定缓冲区大小。
 *
 * 使用示例：
 *   sscanf_s(buffer, "%s %d", name, (unsigned int)sizeof(name), &value);
 */
int sscanf_s(const char *buffer, size_t bufferSize, const char *format, ...);

/**
 * @brief 安全的可变参数字符串格式化输入函数
 *
 * @param buffer 输入缓冲区
 * @param bufferSize 缓冲区大小
 * @param format 格式化字符串
 * @param argList 可变参数列表
 * @return int 成功时返回成功匹配的项目数，失败时返回 EOF
 */
int vsscanf_s(const char *buffer, size_t bufferSize, const char *format, va_list argList);

/**
 * @brief 安全的流格式化输入函数
 *
 * @param stream 输入流
 * @param format 格式化字符串
 * @param ... 可变参数（指针）
 * @return int 成功时返回成功匹配的项目数，失败时返回 EOF
 *
 * 与标准 fscanf 不同，fscanf_s 对字符串格式（如 %s、%c）需要指定缓冲区大小。
 */
int fscanf_s(FILE *stream, const char *format, ...);

/**
 * @brief 安全的可变参数流格式化输入函数
 *
 * @param stream 输入流
 * @param format 格式化字符串
 * @param argList 可变参数列表
 * @return int 成功时返回成功匹配的项目数，失败时返回 EOF
 */
int vfscanf_s(FILE *stream, const char *format, va_list argList);

/**
 * @brief 安全的标准输入格式化函数
 *
 * @param format 格式化字符串
 * @param ... 可变参数（指针）
 * @return int 成功时返回成功匹配的项目数，失败时返回 EOF
 *
 * 与标准 scanf 不同，scanf_s 对字符串格式（如 %s、%c）需要指定缓冲区大小。
 */
int scanf_s(const char *format, ...);

/**
 * @brief 安全的可变参数标准输入格式化函数
 *
 * @param format 格式化字符串
 * @param argList 可变参数列表
 * @return int 成功时返回成功匹配的项目数，失败时返回 EOF
 */
int vscanf_s(const char *format, va_list argList);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_SECURC_H */
