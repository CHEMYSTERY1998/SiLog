/**
 * @file silog.h
 * @brief SiLog 主接口头文件
 */

#ifndef SILOG_H
#define SILOG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 日志级别枚举
 */
typedef enum {
    SILOG_DEBUG = 0, ///< 调试级别
    SILOG_INFO,      ///< 信息级别
    SILOG_WARN,      ///< 警告级别
    SILOG_ERROR,     ///< 错误级别
    SILOG_FATAL      ///< 致命级别
} silogLevel;

/**
 * @brief 核心日志打印接口
 * @param level 日志级别
 * @param tag 模块标签
 * @param file 源文件名
 * @param line 行号
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void silogLog(silogLevel level, const char *tag, const char *file, uint32_t line, const char *fmt, ...);

/**
 * @brief 日志宏定义，自动填充文件名和行号
 */
#define SILOG_D(tag, fmt, ...) silogLog(SILOG_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOG_I(tag, fmt, ...) silogLog(SILOG_INFO, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOG_W(tag, fmt, ...) silogLog(SILOG_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOG_E(tag, fmt, ...) silogLog(SILOG_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOG_F(tag, fmt, ...) silogLog(SILOG_FATAL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* SILOG_H */