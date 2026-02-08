/**
 * @file silog_time.h
 * @brief SiLog 时间工具模块
 */

#ifndef SILOG_TIME_H
#define SILOG_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 获取系统当前时间（wall clock），毫秒
 * @return uint64_t 当前时间距 1970-01-01 的毫秒数
 */
uint64_t SilogGetNowMs(void);

/**
 * @brief 获取系统单调时间（monotonic），毫秒
 * @return uint64_t 单调时间毫秒数（不受系统时间调整影响）
 */
uint64_t SilogGetMonoMs(void);

/**
 * @brief 将毫秒时间格式化为字符串
 * @param inputMs 输入的毫秒时间戳
 * @param buffer 输出缓冲区
 * @param bufferLen 缓冲区大小
 * @note 格式为 "YYYY-MM-DD HH:MM:SS.mmm"
 */
void SilogFormatWallClockMs(uint64_t inputMs, char *buffer, uint32_t bufferLen);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_TIME_H */
