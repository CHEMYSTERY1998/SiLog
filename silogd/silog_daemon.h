#ifndef SILOG_DAEMON_H
#define SILOG_DAEMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SILOGD_SOCKET_PATH "/tmp/silogd.sock"

int32_t SilogDaemonInitSocket(void);          // 创建并绑定 socket（Unix Domain）
int32_t SilogDaemonStartLoop(int32_t sockFd); // 主循环

#ifdef __cplusplus
}
#endif
#endif // SILOG_DAEMON_H
