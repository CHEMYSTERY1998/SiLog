#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "silog_trans.h"

// 你的日志结构体（示例）
typedef struct {
    int32_t level;
    char tag[32];
    char msg[128];
} logEntry_t;

// 每个连接的缓存（示例：记录粘包残余）
typedef struct {
    uint32_t remain;
    uint8_t buf[sizeof(logEntry_t)];
} clientBuf_t;

#define MAX_EVENTS 32
#define SOCK_PATH "/tmp/silog_demo.sock"

// =====================
// 回调：收到完整包时执行
// =====================
static void logEntryHandler(int32_t fd, const void *data, uint32_t len)
{
    (void)fd;
    const logEntry_t *entry = (const logEntry_t *)data;
    printf("[SERVER] level=%d tag=%s msg=%s\n", entry->level, entry->tag, entry->msg);
}

// =====================
// 主程序
// =====================
int main()
{
    silogTransSetHandler(logEntryHandler);

    int32_t serverFd = silogTransServerInit(SOCK_PATH);
    if (serverFd < 0) {
        perror("silogTransServerInit failed");
        return 1;
    }
    printf("[SERVER] listening on %s\n", SOCK_PATH);

    // ---- 创建 epoll ----
    int32_t epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        return 1;
    }

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = serverFd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serverFd, &ev);

    // 每个 client fd 对应一个 buffer（简单示例用数组）
    clientBuf_t clientBuf[1024];
    memset(clientBuf, 0, sizeof(clientBuf));

    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n < 0)
            continue;

        for (int i = 0; i < n; i++) {

            // 新连接
            if (events[i].data.fd == serverFd) {
                int32_t clientFd = silogTransServerAccept(serverFd);
                if (clientFd < 0) {
                    continue;
                }

                printf("[SERVER] new client %d\n", clientFd);

                ev.events = EPOLLIN | EPOLLET; // 边缘触发
                ev.data.fd = clientFd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientFd, &ev);
            } else {
                int32_t fd = events[i].data.fd;

                clientBuf_t *cb = &clientBuf[fd];

                int32_t ret = silogTransServerRecv(fd, cb->buf, sizeof(logEntry_t), &cb->remain, logEntryHandler);

                if (ret == 0) {
                    printf("[SERVER] client %d disconnected\n", fd);
                    close(fd);
                } else if (ret < 0) {
                    printf("[SERVER] recv error on %d\n", fd);
                    close(fd);
                }
            }
        }
    }

    close(serverFd);
    return 0;
}
