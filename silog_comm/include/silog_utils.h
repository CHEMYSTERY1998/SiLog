/**
 * @file silog_utils.h
 * @brief SiLog 通用工具模块
 */

#ifndef SILOG_UTILS_H
#define SILOG_UTILS_H

#include <sys/types.h>

#include "silog.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取当前线程 ID
 * @return pid_t 线程 ID
 */
pid_t SilogUtilsGetTid(void);

/**
 * @brief 将日志级别转换为名称字符串
 * @param level 日志级别
 * @return const char* 日志级别名称字符串
 */
const char *SilogUtilsLevelToName(silogLevel level);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_UTILS_H */
