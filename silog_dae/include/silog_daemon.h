#ifndef SILOG_DAEMON_H
#define SILOG_DAEMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ================ Daemon 服务端接口 ================

// Daemon 服务端初始化接口
int32_t SilogDaemonInit();
void SilogDaemonDeinit();

// Daemon 服务端初始化接口

// Daemon 服务端启动接口

// Daemon 服务端配置接口：配置缓冲区大小、日志文件路径、日志切割策略、等参数

// Daemon 服务端停止接口

// Daemon 服务端状态查询接口: 查询当前日志文件大小、已用缓冲区大小等状态信息、外部连接客户端信息


#ifdef __cplusplus
}
#endif
#endif // SILOG_DAEMON_H
