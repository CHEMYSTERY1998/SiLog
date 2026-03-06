/**
 * @file silog_prelog.h
 * @brief SiLog 预日志模块头文件
 *
 * 提供统一的预日志功能，用于在日志系统完全初始化前记录日志。
 * 支持文件写入、级别过滤、线程安全和可选的 stdout 输出。
 */

#ifndef SILOG_PRELOG_H
#define SILOG_PRELOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SILOG_PRELOG_PATH_MAX   256
#define SILOG_PRELOG_MSG_MAX    1024
#define SILOG_PRELOG_MAX 16

/**
 * @brief 预日志级别枚举
 */
typedef enum {
    SILOG_PRELOG_LEVEL_DEBUG = 0, ///< 调试级别
    SILOG_PRELOG_LEVEL_INFO,      ///< 信息级别
    SILOG_PRELOG_LEVEL_WARNING,   ///< 警告级别
    SILOG_PRELOG_LEVEL_ERROR      ///< 错误级别
} SilogPrelogLevel_t;

/**
 * @brief 预日志配置结构体
 */
typedef struct {
    char path[SILOG_PRELOG_PATH_MAX]; ///< 日志文件路径
    SilogPrelogLevel_t minLevel;      ///< 最小日志级别
    bool enableStdout;                ///< 是否同时输出到 stdout
} SilogPrelogConfig_t;

/**
 * @brief 初始化预日志模块
 *
 * @param config 配置指针，传 NULL 使用默认配置
 *               默认配置：
 *               - 路径: /tmp/silog_prelog.txt
 *               - 级别: SILOG_PRELOG_LEVEL_DEBUG
 *               - stdout: 如果文件打开失败则输出到 stdout
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogPrelogInit(const SilogPrelogConfig_t *config);

/**
 * @brief 反初始化预日志模块
 *
 * 关闭日志文件，释放资源。
 */
void SilogPrelogDeinit(void);

/**
 * @brief 写入预日志
 *
 * @param module 模块名称（如 "LOGGER", "DAEMON"）
 * @param level  日志级别
 * @param fmt    格式化字符串
 * @param ...    可变参数
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogPrelogWrite(const char *module, SilogPrelogLevel_t level, const char *fmt, ...);

/**
 * @brief 写入预日志（va_list 版本）
 *
 * @param module 模块名称（如 "LOGGER", "DAEMON"）
 * @param level  日志级别
 * @param fmt    格式化字符串
 * @param args   可变参数列表
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogPrelogWriteV(const char *module, SilogPrelogLevel_t level, const char *fmt, va_list args);

/**
 * @brief 检查预日志是否已初始化
 *
 * @return true 表示已初始化，false 表示未初始化
 */
bool SilogPrelogIsInitialized(void);

/**
 * @brief 获取预日志文件指针
 *
 * @return 文件指针，未初始化返回 NULL
 */
FILE *SilogPrelogGetFile(void);

/* 便捷宏定义 - 需要指定模块名称 */
#define SILOG_PRELOG_D(module, fmt, ...) SilogPrelogWrite(module, SILOG_PRELOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define SILOG_PRELOG_I(module, fmt, ...) SilogPrelogWrite(module, SILOG_PRELOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define SILOG_PRELOG_W(module, fmt, ...) SilogPrelogWrite(module, SILOG_PRELOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define SILOG_PRELOG_E(module, fmt, ...) SilogPrelogWrite(module, SILOG_PRELOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

/* 预定义模块名称宏 */
#define SILOG_PRELOG_LOGGER "logger"
#define SILOG_PRELOG_DAEMON "daemon"
#define SILOG_PRELOG_CAT    "cat"
#define SILOG_PRELOG_EXE    "exe"
#define SILOG_PRELOG_COMM   "comm"

#ifdef __cplusplus
}
#endif

#endif /* SILOG_PRELOG_H */
