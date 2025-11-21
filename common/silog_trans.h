#ifndef SILOG_TRANS_H
#define SILOG_TRANS_H

#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 服务端收到完整消息后的回调
 */
typedef void (*silogTransHandler_t)(int32_t fd, const void *buf, uint32_t len);

/**
 * 启动 Unix Domain Socket 服务端
 */
int32_t silogTransServerInit(const char *path);

/**
 * 服务端 accept
 */
int32_t silogTransServerAccept(int32_t serverFd);

/**
 * 服务端接收固定长度数据（处理粘包）
 */
int32_t silogTransServerRecv(int32_t clientFd, uint8_t *buf, uint32_t bufSize, uint32_t *remainLen,
                             silogTransHandler_t handler);

/**
 * 注册服务端消息处理回调
 */
void silogTransSetHandler(silogTransHandler_t handler);

/**
 * 客户端连接初始化
 */
int32_t silogTransClientInit(const char *path);

/**
 * 客户端发送（保证全部发送）
 */
int32_t silogTransClientSend(int32_t fd, const void *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif // SILOG_TRANS_H
