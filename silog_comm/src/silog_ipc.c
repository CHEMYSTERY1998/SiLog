#include "silog_ipc.h"

#include <errno.h>
#include <fcntl.h>
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
    bool isInit;
    int32_t sendFd;
    int32_t recvFd;
    int32_t (*clientInit)();
    int32_t (*clientSend)(const void *data, uint32_t len);
    void (*clientClose)();
    int32_t (*serverInit)();
    int32_t (*serverRecv)(void *data, uint32_t len);
    void (*serverClose)();
} SilogIpcAgent;

static SilogIpcAgent g_silogIpcAgent = {0};

// =========== Unix Domain DGRAM 实现 ===========
STATIC void setNonblock(int32_t fd)
{
    int32_t flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

STATIC int32_t SilogIpcDgramClientInit(void)
{
    g_silogIpcAgent.sendFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_silogIpcAgent.sendFd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to create client socket: %s", strerror(errno));
        return SILOG_NET_FILE_CREATE;
    }
    setNonblock(g_silogIpcAgent.sendFd);
    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    int32_t ret =
        snprintf_s(addr.sun_path, sizeof(addr.sun_path), sizeof(addr.sun_path) - 1, "%s", LOGD_SOCKET_PATH);
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to format socket path");
        close(g_silogIpcAgent.sendFd);
        return SILOG_STR_ERR;
    }

    ret = connect(g_silogIpcAgent.sendFd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to connect to socket %s: %s", LOGD_SOCKET_PATH, strerror(errno));
        close(g_silogIpcAgent.sendFd);
        return SILOG_NET_CONNECT;
    }

    return SILOG_OK;
}

STATIC int32_t SilogIpcDgramClientSend(const void *data, uint32_t len)
{
    if (g_silogIpcAgent.sendFd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Client send failed: socket not initialized");
        return SILOG_NET_FILE_ERROR;
    }

    int32_t n = send(g_silogIpcAgent.sendFd, data, len, 0);
    if (n < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Client send failed: %s", strerror(errno));
        return SILOG_NET_SEND;
    }

    return SILOG_OK;
}

STATIC void SilogIpcDgramClientClose(void)
{
    if (g_silogIpcAgent.sendFd >= 0) {
        close(g_silogIpcAgent.sendFd);
        g_silogIpcAgent.sendFd = -1;
    }
}

STATIC int32_t SilogIpcDgramServerInit(void)
{
    unlink(LOGD_SOCKET_PATH); /* 删除旧文件 */
    g_silogIpcAgent.recvFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_silogIpcAgent.recvFd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to create server socket: %s", strerror(errno));
        return SILOG_NET_FILE_CREATE;
    }
    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    int32_t ret =
        snprintf_s(addr.sun_path, sizeof(addr.sun_path), sizeof(addr.sun_path) - 1, "%s", LOGD_SOCKET_PATH);
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to format server socket path");
        close(g_silogIpcAgent.recvFd);
        return SILOG_STR_ERR;
    }

    ret = bind(g_silogIpcAgent.recvFd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Failed to bind socket %s: %s", LOGD_SOCKET_PATH, strerror(errno));
        close(g_silogIpcAgent.recvFd);
        return SILOG_NET_FILE_OPEN;
    }

    return SILOG_OK;
}

STATIC int32_t SilogIpcDgramServerRecv(void *data, uint32_t len)
{
    if (g_silogIpcAgent.recvFd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Server recv failed: socket not initialized");
        return 0;
    }

    int32_t n = recv(g_silogIpcAgent.recvFd, data, len, 0);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Server recv failed: %s", strerror(errno));
    }
    return n;
}

STATIC void SilogIpcDgramServerClose(void)
{
    if (g_silogIpcAgent.recvFd >= 0) {
        close(g_silogIpcAgent.recvFd);
        g_silogIpcAgent.recvFd = -1;
    }
}

STATIC void SilogIpcTypeSetStream(void)
{
    g_silogIpcAgent.clientInit = NULL;
    g_silogIpcAgent.clientSend = NULL;
    g_silogIpcAgent.clientClose = NULL;
}

STATIC void SilogIpcTypeSetDgram(void)
{
    g_silogIpcAgent.clientInit = SilogIpcDgramClientInit;
    g_silogIpcAgent.clientSend = SilogIpcDgramClientSend;
    g_silogIpcAgent.clientClose = SilogIpcDgramClientClose;
    g_silogIpcAgent.serverInit = SilogIpcDgramServerInit;
    g_silogIpcAgent.serverRecv = SilogIpcDgramServerRecv;
    g_silogIpcAgent.serverClose = SilogIpcDgramServerClose;
}

int32_t SilogIpcClientInit(void)
{
    if (g_silogIpcAgent.clientInit == NULL) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return g_silogIpcAgent.clientInit();
}

int32_t SilogIpcClientSend(const void *data, uint32_t len)
{
    if (g_silogIpcAgent.clientSend == NULL) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return g_silogIpcAgent.clientSend(data, len);
}

void SilogIpcClientClose(void)
{
    if (g_silogIpcAgent.clientClose == NULL) {
        return;
    }
    g_silogIpcAgent.clientClose();
}

int32_t SilogIpcServerInit(void)
{
    if (g_silogIpcAgent.serverInit == NULL) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return g_silogIpcAgent.serverInit();
}

int32_t SilogIpcServerRecv(void *data, uint32_t len)
{
    if (g_silogIpcAgent.serverRecv == NULL) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return g_silogIpcAgent.serverRecv(data, len);
}

void SilogIpcServerClose(void)
{
    if (g_silogIpcAgent.serverClose == NULL) {
        return;
    }
    g_silogIpcAgent.serverClose();
}

void SilogIpcInit(SilogIpcType_t type)
{
    if (g_silogIpcAgent.isInit) {
        return;
    }
    if (type == SILOG_IPC_TYPE_UNIX_DGRAM) {
        SilogIpcTypeSetDgram();
    } else {
        SilogIpcTypeSetStream();
    }
}
