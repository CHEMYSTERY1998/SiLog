#ifndef SILOG_H
#define SILOG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SILOG_DEBUG = 0,
    SILOG_INFO,
    SILOG_WARN,
    SILOG_ERROR,
    SILOG_FATAL
} silogLevel;

void silogLog(silogLevel level, const char *tag, const char *file, uint32_t line, const char *fmt, ...);

/*
    日志宏定义，自动填充文件名和行号
*/
#define SILOG_D(tag, fmt, ...) silogLog(SILOG_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOG_I(tag, fmt, ...) silogLog(SILOG_INFO, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOG_W(tag, fmt, ...) silogLog(SILOG_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOG_E(tag, fmt, ...) silogLog(SILOG_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOG_F(tag, fmt, ...) silogLog(SILOG_FATAL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif // SILOG_H