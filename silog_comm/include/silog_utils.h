#ifndef SILOG_UTILS_H
#define SILOG_UTILS_H

#include "silog_utils.h"

#include "silog_adapter.h"
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t silogGetCurTimeMs(void);

/* 获取线程ID */
pid_t getTid(void);

#ifdef __cplusplus
}
#endif

#endif // SILOG_UTILS_H
