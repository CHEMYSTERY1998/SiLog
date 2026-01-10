#ifndef SILOG_TIME_H
#define SILOG_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 系统当前时间（wall clock），毫秒 */
uint64_t SilogGetNowMs(void);
/* 系统单调时间（monotonic），毫秒 */
uint64_t SilogGetMonoMs(void);

/* 将毫秒时间格式化为字符串，格式为 "YYYY-MM-DD HH:MM:SS" */
void SilogFormatWallClockMs(uint64_t inputMs, char *buffer, uint32_t bufferLen);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_TIME_H */
