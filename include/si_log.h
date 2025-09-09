#ifndef SI_LOG_H
#define SI_LOG_H

#include "si_log_entry.h"

#define SILOGD(tag, fmt, ...) silog_log(SILOG_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOGI(tag, fmt, ...) silog_log(SILOG_INFO, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOGW(tag, fmt, ...) silog_log(SILOG_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOGE(tag, fmt, ...) silog_log(SILOG_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SILOGF(tag, fmt, ...) silog_log(SILOG_FATAL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // SI_LOG_H