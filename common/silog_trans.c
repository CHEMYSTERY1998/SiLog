#include "silog_trans.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static silogTransHandler_t g_handler = NULL;

void silogTransSetHandler(silogTransHandler_t handler)
{
    g_handler = handler;
}

/* ============================
 *           客户端
 * ============================ */

int32_t silogTransClientInit(const char *path)
{
    int32_t fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int32_t silogTransClientSend(int32_t fd, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        sent += n;
    }
    return 0;
}

/* ============================
 *           服务端
 * ============================ */

int32_t silogTransServerInit(const char *path)
{
    unlink(path);

    int32_t fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int32_t silogTransServerAccept(int32_t serverFd)
{
    struct sockaddr_un clientAddr;
    socklen_t len = sizeof(clientAddr);

    int32_t clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &len);
    if (clientFd < 0)
        return -1;

    return clientFd;
}

int32_t silogTransServerRecv(int32_t clientFd, uint8_t *buf, uint32_t bufSize, uint32_t *remainLen,
                             silogTransHandler_t handler)
{
    ssize_t n = recv(clientFd, buf + (*remainLen), bufSize - (*remainLen), 0);

    if (n == 0)
        return 0; // 客户端断开
    if (n < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            return 1;
        return -1;
    }

    *remainLen += (uint32_t)n;

    if (*remainLen < bufSize)
        return 1;

    // 收到一个完整包
    if (handler)
        handler(clientFd, buf, bufSize);

    *remainLen = 0;
    return 1;
}
