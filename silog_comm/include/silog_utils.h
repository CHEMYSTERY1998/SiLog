#ifndef SILOG_UTILS_H
#define SILOG_UTILS_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 获取线程ID */
pid_t getTid(void);

#ifdef __cplusplus
}
#endif

#endif // SILOG_UTILS_H
