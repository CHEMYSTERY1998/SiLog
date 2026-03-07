/**
 * @file silog_daemon.h
 * @brief SiLog 守护进程模块
 */

#ifndef SILOG_DAEMON_H
#define SILOG_DAEMON_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Daemon 服务端初始化接口
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogDaemonInit(void);

/**
 * @brief Daemon 服务端反初始化接口
 */
void SilogDaemonDeinit(void);

/* ================ Daemon 服务端接口 ================ */

/**
 * @brief 远程服务配置结构
 */
typedef struct {
    uint16_t listenPort;    ///< 监听端口，0 表示使用默认端口 9090
    uint32_t maxClients;    ///< 最大客户端数，0 表示使用默认值 10
    bool enable;            ///< 是否启用远程服务
} SilogDaemonRemoteConfig;

/**
 * @brief Daemon 远程服务初始化
 * @param config 远程服务配置，为 NULL 时使用默认配置
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogDaemonRemoteInit(const SilogDaemonRemoteConfig *config);

/**
 * @brief Daemon 远程服务反初始化
 */
void SilogDaemonRemoteDeinit(void);

/**
 * @brief 获取当前远程服务状态
 * @return true 表示远程服务已启用，false 表示未启用
 */
bool SilogDaemonRemoteIsEnabled(void);

/**
 * @brief 获取当前连接的远程客户端数量
 * @return 客户端数量
 */
uint32_t SilogDaemonRemoteGetClientCount(void);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_DAEMON_H */
