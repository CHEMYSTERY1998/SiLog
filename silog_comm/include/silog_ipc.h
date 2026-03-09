/**
 * @file silog_ipc.h
 * @brief SiLog IPC (Inter-Process Communication) module for local communication
 *
 * 本模块提供本地进程间通信功能，使用 Unix Domain Socket 实现。
 * 注意：此模块专用于本机通信，不涉及跨主机网络传输。
 */

#ifndef SILOG_IPC_H
#define SILOG_IPC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IPC 类型枚举
 */
typedef enum {
    SILOG_IPC_TYPE_UNIX_DGRAM = 0, ///< Unix Domain Socket - Datagram 模式
    SILOG_IPC_TYPE_UNIX_STREAM     ///< Unix Domain Socket - Stream 模式（未实现）
} SilogIpcType_t;

/**
 * @brief 初始化 IPC 层
 * @param type IPC 类型
 */
void SilogIpcInit(SilogIpcType_t type);

/**
 * @brief 客户端初始化
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogIpcClientInit(void);

/**
 * @brief 客户端发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogIpcClientSend(const void *data, uint32_t len);

/**
 * @brief 关闭客户端连接
 */
void SilogIpcClientClose(void);

/**
 * @brief 服务端初始化
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogIpcServerInit(void);

/**
 * @brief 服务端接收数据
 * @param data 接收缓冲区
 * @param len 缓冲区大小
 * @return 成功返回接收字节数，失败返回负值
 */
int32_t SilogIpcServerRecv(void *data, uint32_t len);

/**
 * @brief 关闭服务端
 */
void SilogIpcServerClose(void);

/**
 * @brief 检查 IPC 是否已初始化
 * @return true 如果已初始化，false 否则
 */
bool SilogIpcIsInit(void);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_IPC_H */
