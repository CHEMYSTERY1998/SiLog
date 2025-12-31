#ifndef SILOG_DAEMON_H
#define SILOG_DAEMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ================ Daemon 客户端接口 ================
// Daemon 客户端初始化接口
int32_t SilogDaemonClientInit();
// Daemon 客户端配置接口-->调用服务端配置接口

// Daemon 客户端注册日志信息回调接口

#ifdef __cplusplus
}
#endif
#endif // SILOG_DAEMON_H
