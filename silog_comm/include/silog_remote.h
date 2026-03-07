/**
 * @file silog_remote.h
 * @brief SiLog 远程日志服务模块 - 跨主机 TCP 通信
 *
 * 本模块提供跨主机的日志广播功能，daemon 作为 TCP 服务器，
 * 允许远程 silogcat 客户端连接并实时接收日志。
 */

#ifndef SILOG_REMOTE_H
#define SILOG_REMOTE_H

#include <stdbool.h>
#include <stdint.h>

#include "silog_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SILOG_REMOTE_DEFAULT_PORT        9090 ///< 默认监听端口
#define SILOG_REMOTE_DEFAULT_MAX_CLIENTS 10   ///< 默认最大客户端数
#define SILOG_REMOTE_BACKLOG             10   ///< listen 队列长度

#define SILOG_REMOTE_DEFAULT_SERVER_ADDR "127.0.0.1" ///< 默认服务器地址
#define SILOG_REMOTE_DEFAULT_RECONNECT_MS 5000       ///< 默认重连间隔（毫秒）

///< 日志条目序列化字段大小（字节）
#define SILOG_REMOTE_TS_SIZE     8 ///< 时间戳 uint64_t
#define SILOG_REMOTE_PID_SIZE    8 ///< 进程 ID uint64_t
#define SILOG_REMOTE_TID_SIZE    4 ///< 线程 ID uint32_t
#define SILOG_REMOTE_LEVEL_SIZE  1 ///< 日志级别 uint8_t
#define SILOG_REMOTE_LINE_SIZE   4 ///< 行号 int32_t

///< 序列化后日志条目总大小
#define SILOG_REMOTE_ENTRY_SIZE (SILOG_REMOTE_TS_SIZE + SILOG_REMOTE_PID_SIZE + SILOG_REMOTE_TID_SIZE + \
                                 SILOG_REMOTE_LEVEL_SIZE + SILOG_REMOTE_LINE_SIZE + \
                                 SILOG_TAG_MAX_LEN + SILOG_FILE_MAX_LEN + SILOG_MSG_MAX_LEN)

/**
 * @brief 远程服务配置结构
 */
typedef struct {
    uint16_t listenPort; ///< 监听端口
    uint32_t maxClients; ///< 最大客户端数
    bool enableAuth;     ///< 是否启用认证（后续扩展）
} SilogRemoteConfig_t;

/**
 * @brief 远程客户端配置结构
 */
typedef struct {
    char serverAddr[64];  ///< 服务器地址
    uint16_t serverPort;  ///< 服务器端口
    bool useReconnect;    ///< 是否自动重连
    uint32_t reconnectMs; ///< 重连间隔毫秒
} SilogRemoteClientConfig;

/**
 * @brief 日志条目回调函数类型
 * @param entry 日志条目指针
 * @param userData 用户数据指针
 */
typedef void (*SilogRemoteLogCallback)(const logEntry_t *entry, void *userData);

/**
 * @brief 初始化远程日志服务
 * @param config 配置参数，为 NULL 时使用默认配置
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogRemoteInit(const SilogRemoteConfig_t *config);

/**
 * @brief 反初始化远程日志服务
 */
void SilogRemoteDeinit(void);

/**
 * @brief 接受新的客户端连接（应在 daemon 主循环中调用）
 * @return 成功返回 SILOG_OK，无新连接返回 SILOG_NET_TIMEOUT，失败返回错误码
 */
int32_t SilogRemoteAccept(void);

/**
 * @brief 广播日志给所有连接的远程客户端
 * @param entry 日志条目指针
 * @param sentCount 输出参数，成功广播的客户端数量，可为 NULL
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogRemoteBroadcast(const logEntry_t *entry, int32_t *sentCount);

/**
 * @brief 获取当前连接的客户端数量
 * @return 客户端数量
 */
uint32_t SilogRemoteGetClientCount(void);

/**
 * @brief 检查远程服务是否已初始化
 * @return true 表示已初始化，false 表示未初始化
 */
bool SilogRemoteIsInit(void);

/* ==================== 客户端 API ==================== */

/**
 * @brief 初始化远程日志客户端
 * @param config 配置参数，为 NULL 时使用默认配置
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogRemoteClientInit(const SilogRemoteClientConfig *config);

/**
 * @brief 反初始化远程日志客户端
 */
void SilogRemoteClientDeinit(void);

/**
 * @brief 连接到远程日志服务器
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogRemoteClientConnect(void);

/**
 * @brief 断开与远程日志服务器的连接
 */
void SilogRemoteClientDisconnect(void);

/**
 * @brief 检查客户端是否已连接
 * @return true 表示已连接，false 表示未连接
 */
bool SilogRemoteClientIsConnected(void);

/**
 * @brief 接收并处理日志条目（阻塞模式）
 * @param callback 日志条目回调函数
 * @param userData 用户数据指针，会传递给回调函数
 * @param timeoutMs 超时时间（毫秒），0 表示不超时
 * @return 成功返回 SILOG_OK，超时返回 SILOG_NET_TIMEOUT，失败返回错误码
 */
int32_t SilogRemoteClientReceive(SilogRemoteLogCallback callback, void *userData, uint32_t timeoutMs);

/**
 * @brief 将网络缓冲区反序列化为 logEntry_t
 * @param buf 网络缓冲区指针
 * @param bufSize 缓冲区大小
 * @param entry 输出的日志条目结构
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogRemoteDeserializeEntry(const uint8_t *buf, size_t bufSize, logEntry_t *entry);

/**
 * @brief 检查远程客户端是否已初始化
 * @return true 表示已初始化，false 表示未初始化
 */
bool SilogRemoteClientIsInit(void);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_REMOTE_H */
