#ifndef SILOG_H
#define SILOG_H
#include "silog_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    日志宏定义，自动填充文件名和行号
*/
#define SILOGD(tag, fmt, ...) silogLog(SILOG_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOGI(tag, fmt, ...) silogLog(SILOG_INFO, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOGW(tag, fmt, ...) silogLog(SILOG_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOGE(tag, fmt, ...) silogLog(SILOG_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOGF(tag, fmt, ...) silogLog(SILOG_FATAL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// TODO: 增加一个异步执行的注册函数，让用户可以注册自己的异步日志处理回调

#ifdef __cplusplus
}
#endif
#endif // SILOG_H