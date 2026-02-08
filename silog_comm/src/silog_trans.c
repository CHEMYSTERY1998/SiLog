#include "silog_trans.h"

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
} SilogTranAgent;

static SilogTranAgent g_silogTranAgent = {0};

// =========== UDP 传输实现 ===========
STATIC void setNonblock(int32_t fd)
{
    int32_t flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

STATIC int32_t SilogTransUdpClientInit(void)
{
    g_silogTranAgent.sendFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_silogTranAgent.sendFd < 0) {
        return SILOG_NET_FILE_CREATE;
    }
    setNonblock(g_silogTranAgent.sendFd); // TODO: 是否需要非阻塞？
    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    int32_t ret = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", LOGD_SOCKET_PATH);
    if (ret < 0) {
        close(g_silogTranAgent.recvFd);
        return SILOG_STR_ERR;
    }

    ret = connect(g_silogTranAgent.sendFd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        close(g_silogTranAgent.sendFd);
        return SILOG_NET_CONNECT;
    }

    return SILOG_OK;
}

STATIC int32_t SilogTransUdpClientSend(const void *data, uint32_t len)
{
    if (g_silogTranAgent.sendFd < 0) {
        return SILOG_NET_FILE_ERROR;
    }

    int32_t n = send(g_silogTranAgent.sendFd, data, len, 0);
    if (n < 0) {
        return SILOG_NET_SEND;
    }

    return SILOG_OK;
}

STATIC void SilogTransUdpClientClose(void)
{
    if (g_silogTranAgent.sendFd >= 0) {
        close(g_silogTranAgent.sendFd);
        g_silogTranAgent.sendFd = -1;
    }
}

STATIC int32_t SilogTransUdpServerInit(void)
{
    unlink(LOGD_SOCKET_PATH); /* 删除旧文件 */
    g_silogTranAgent.recvFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_silogTranAgent.recvFd < 0) {
        return SILOG_NET_FILE_CREATE;
    }
    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    int32_t ret = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", LOGD_SOCKET_PATH);
    if (ret < 0) {
        close(g_silogTranAgent.recvFd);
        return SILOG_STR_ERR;
    }

    ret = bind(g_silogTranAgent.recvFd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        close(g_silogTranAgent.recvFd);
        return SILOG_NET_FILE_OPEN;
    }

    return SILOG_OK;
}

STATIC int32_t SilogTransUdpServerRecv(void *data, uint32_t len)
{
    if (g_silogTranAgent.recvFd < 0) {
        return 0;
    }

    return recv(g_silogTranAgent.recvFd, data, len, 0);
}

STATIC void SilogTransUdpServerClose(void)
{
    if (g_silogTranAgent.recvFd >= 0) {
        close(g_silogTranAgent.recvFd);
        g_silogTranAgent.recvFd = -1;
    }
}

// =========== TCP 传输实现 ===========
// TODO: 后续实现 TCP 传输相关函数

STATIC void SilogTransTypeSetTcp(void)
{
    g_silogTranAgent.clientInit = NULL;
    g_silogTranAgent.clientSend = NULL;
    g_silogTranAgent.clientClose = NULL;
}

STATIC void SilogTransTypeSetUdp(void)
{
    g_silogTranAgent.clientInit = SilogTransUdpClientInit;
    g_silogTranAgent.clientSend = SilogTransUdpClientSend;
    g_silogTranAgent.clientClose = SilogTransUdpClientClose;
    g_silogTranAgent.serverInit = SilogTransUdpServerInit;
    g_silogTranAgent.serverRecv = SilogTransUdpServerRecv;
    g_silogTranAgent.serverClose = SilogTransUdpServerClose;
}

int32_t SilogTransClientInit(void)
{
    if (g_silogTranAgent.clientInit == NULL) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return g_silogTranAgent.clientInit();
}

int32_t SilogTransClientSend(const void *data, uint32_t len)
{
    if (g_silogTranAgent.clientSend == NULL) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return g_silogTranAgent.clientSend(data, len);
}

void SilogTransClientClose(void)
{
    if (g_silogTranAgent.clientClose == NULL) {
        return;
    }
    g_silogTranAgent.clientClose();
}

int32_t SilogTransServerInit(void)
{
    if (g_silogTranAgent.serverInit == NULL) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return g_silogTranAgent.serverInit();
}

int32_t SilogTransServerRecv(void *data, uint32_t len)
{
    if (g_silogTranAgent.serverRecv == NULL) {
        return SILOG_NOT_IMPLEMENTED;
    }
    return g_silogTranAgent.serverRecv(data, len);
}

void SilogTransServerClose(void)
{
    if (g_silogTranAgent.serverClose == NULL) {
        return;
    }
    g_silogTranAgent.serverClose();
}

void SilogTransInit(SilogTransType_t type)
{
    if (g_silogTranAgent.isInit) {
        return;
    }
    if (type == SILOG_TRAN_TYPE_UDP) {
        SilogTransTypeSetUdp();
    } else {
        SilogTransTypeSetTcp();
    }
}
