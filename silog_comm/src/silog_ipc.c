#include "silog_ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "silog_adapter.h"
#include "silog_error.h"
#include "silog_prelog.h"
#include "silog_securec.h"

#define LOGD_SOCKET_PATH "/tmp/logd.sock"

typedef struct {
    atomic_bool isInit;
    atomic_int_least32_t sendFd;
    atomic_int_least32_t recvFd;
} SilogIpcAgent;

static SilogIpcAgent g_silogIpcAgent = {0};

// =========== Unix Domain DGRAM 实现 ===========
STATIC int32_t setNonblock(int32_t fd)
{
    int32_t flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return SILOG_NET_FILE_ERROR;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return SILOG_NET_FILE_ERROR;
    }
    return SILOG_OK;
}

STATIC int32_t SilogIpcDgramClientInit(void)
{
    int32_t fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to create client socket: %s", strerror(errno));
        return SILOG_NET_FILE_CREATE;
    }
    int32_t ret = setNonblock(fd);
    if (ret != SILOG_OK) {
        close(fd);
        return ret;
    }
    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    ret = snprintf_s(addr.sun_path, sizeof(addr.sun_path), sizeof(addr.sun_path) - 1, "%s", LOGD_SOCKET_PATH);
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to format socket path");
        close(fd);
        return SILOG_STR_ERR;
    }

    ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to connect to socket %s: %s", LOGD_SOCKET_PATH, strerror(errno));
        close(fd);
        return SILOG_NET_CONNECT;
    }

    atomic_store(&g_silogIpcAgent.sendFd, fd);
    return SILOG_OK;
}

STATIC void SilogIpcDgramClientClose(void)
{
    int32_t fd = atomic_exchange(&g_silogIpcAgent.sendFd, -1);
    if (fd >= 0) {
        close(fd);
    }
}

STATIC int32_t SilogIpcDgramClientSend(const void *data, uint32_t len)
{
    int32_t fd = atomic_load(&g_silogIpcAgent.sendFd);
    if (fd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Client send failed: socket not initialized");
        return SILOG_NET_FILE_ERROR;
    }

    int32_t n = send(fd, data, len, 0);
    if (n < 0) {
        /* 如果发送失败（可能是服务器重启），尝试重新连接 */
        if (errno == ECONNREFUSED || errno == ENOENT) {
            SILOG_PRELOG_W(SILOG_PRELOG_COMM, "Server disconnected, attempting reconnect...");
            SilogIpcDgramClientClose();
            int32_t ret = SilogIpcDgramClientInit();
            if (ret == SILOG_OK) {
                fd = atomic_load(&g_silogIpcAgent.sendFd);
                /* 重试发送 */
                n = send(fd, data, len, 0);
                if (n >= 0) {
                    return SILOG_OK;
                }
            }
        }
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Client send failed: %s", strerror(errno));
        return SILOG_NET_SEND;
    }

    return SILOG_OK;
}

STATIC int32_t SilogIpcDgramServerInit(void)
{
    unlink(LOGD_SOCKET_PATH); /* 删除旧文件 */
    int32_t fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to create server socket: %s", strerror(errno));
        return SILOG_NET_FILE_CREATE;
    }
    /* 设置非阻塞模式 */
    int32_t ret = setNonblock(fd);
    if (ret != SILOG_OK) {
        close(fd);
        return ret;
    }
    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    ret = snprintf_s(addr.sun_path, sizeof(addr.sun_path), sizeof(addr.sun_path) - 1, "%s", LOGD_SOCKET_PATH);
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to format server socket path");
        close(fd);
        return SILOG_STR_ERR;
    }

    ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to bind socket %s: %s", LOGD_SOCKET_PATH, strerror(errno));
        close(fd);
        return SILOG_NET_FILE_OPEN;
    }
    atomic_store(&g_silogIpcAgent.recvFd, fd);

    return SILOG_OK;
}

STATIC int32_t SilogIpcDgramServerRecv(void *data, uint32_t len)
{
    int32_t fd = atomic_load(&g_silogIpcAgent.recvFd);
    if (fd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Server recv failed: socket not initialized");
        return 0;
    }

    int32_t n = recv(fd, data, len, 0);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Server recv failed: %s", strerror(errno));
    }
    return n;
}

STATIC void SilogIpcDgramServerClose(void)
{
    int32_t fd = atomic_exchange(&g_silogIpcAgent.recvFd, -1);
    if (fd >= 0) {
        close(fd);
    }
    /* 删除套接字文件 */
    (void)unlink(LOGD_SOCKET_PATH);
}

int32_t SilogIpcClientInit(void)
{
    if (!atomic_load(&g_silogIpcAgent.isInit)) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return SilogIpcDgramClientInit();
}

int32_t SilogIpcClientSend(const void *data, uint32_t len)
{
    if (!atomic_load(&g_silogIpcAgent.isInit)) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return SilogIpcDgramClientSend(data, len);
}

void SilogIpcClientClose(void)
{
    if (!atomic_load(&g_silogIpcAgent.isInit)) {
        return;
    }
    SilogIpcDgramClientClose();
}

int32_t SilogIpcServerInit(void)
{
    if (!atomic_load(&g_silogIpcAgent.isInit)) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return SilogIpcDgramServerInit();
}

int32_t SilogIpcServerRecv(void *data, uint32_t len)
{
    if (!atomic_load(&g_silogIpcAgent.isInit)) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return SilogIpcDgramServerRecv(data, len);
}

void SilogIpcServerClose(void)
{
    if (!atomic_load(&g_silogIpcAgent.isInit)) {
        return;
    }
    SilogIpcDgramServerClose();
    /* 重置状态，允许重新初始化（主要用于测试） */
    atomic_store(&g_silogIpcAgent.isInit, false);
}

bool SilogIpcIsInit(void)
{
    return atomic_load(&g_silogIpcAgent.isInit);
}

void SilogIpcInit(SilogIpcType_t type)
{
    if (atomic_load(&g_silogIpcAgent.isInit)) {
        return;
    }
    if (type == SILOG_IPC_TYPE_UNIX_DGRAM) {
        atomic_store(&g_silogIpcAgent.isInit, true);
        atomic_store(&g_silogIpcAgent.sendFd, -1);
        atomic_store(&g_silogIpcAgent.recvFd, -1);
    }
    /* STREAM 类型未实现，不设置 isInit */
}
