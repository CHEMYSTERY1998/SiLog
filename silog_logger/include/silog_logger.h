/**
 * @file silog_logger.h
 * @brief SiLog 日志记录模块
 */

#ifndef SILOG_LOGGER_H
#define SILOG_LOGGER_H

#include <stdint.h>
#include <sys/types.h>

#include "silog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SILOG_TAG_MAX_LEN  32  ///< Tag 最大长度
#define SILOG_FILE_MAX_LEN 64  ///< 文件名最大长度
#define SILOG_MSG_MAX_LEN  256 ///< 消息最大长度

/**
 * @brief 日志条目结构
 */
typedef struct {
    uint64_t ts;                   ///< 时间戳（毫秒）
    pid_t pid;                     ///< 进程 ID
    pid_t tid;                     ///< 线程 ID
    silogLevel level;              ///< 日志级别
    char tag[SILOG_TAG_MAX_LEN];   ///< 模块名
    char file[SILOG_FILE_MAX_LEN]; ///< 源文件名
    uint32_t line;                 ///< 行号
    char msg[SILOG_MSG_MAX_LEN];   ///< 日志正文
    uint16_t msgLen;               ///< 日志正文长度
    uint8_t enabled;               ///< 预测分支快速判断标志
} logEntry_t;

/**
 * @brief 设置最小日志级别
 * @param level 日志级别
 */
void silogSetLevel(silogLevel level);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_LOGGER_H */