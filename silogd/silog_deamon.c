#include "silog_daemon.h"
#include "silog_entry.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef LOG_ENTRY_SIZE
#define LOG_ENTRY_SIZE (sizeof(logEntry_t))
#endif

// max clients we support concurrently (可根据需要调整)
#define MAX_CLIENTS 128

static int32_t gListenFd = -1;
static volatile sig_atomic_t gTerminate = 0;

static void HandleSignal(int signo)
{
    (void)signo;
    gTerminate = 1;
}

// 把 socket 设为非阻塞
static int32_t SetNonblocking(int32_t fd)
{
    int32_t flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 客户端上下文：用于处理半包/粘包
typedef struct {
    int32_t fd;
    uint8_t buf[LOG_ENTRY_SIZE];
    uint32_t used; // buf 中已用字节数
} client_ctx_t;

// 打印 / 处理一条 logEntry（根据需要替换为文件写入等）
static void HandleLogEntry(const logEntry_t *entry)
{
    if (!entry)
        return;
    // 简单打印（注意使用正确的整型格式化）
    printf("[silogd] level=%d tag=%s pid=%" PRId32 " tid=%" PRId32 " msg=%s\n", (int)entry->level, entry->tag,
           (int32_t)entry->pid, (int32_t)entry->tid, entry->msg);
    fflush(stdout);
}

// 清理客户端（关闭 fd 并标记空）
static void CloseClient(client_ctx_t *c)
{
    if (!c)
        return;
    if (c->fd >= 0)
        close(c->fd);
    c->fd = -1;
    c->used = 0;
}

// 接收并处理来自 client 的数据（保证不会丢包，能处理任意边界）
static int RecvAndProcess(client_ctx_t *c)
{
    if (!c || c->fd < 0)
        return -1;

    uint32_t space = LOG_ENTRY_SIZE - c->used;
    ssize_t n = read(c->fd, c->buf + c->used, space);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
    if (n == 0) {
        return -1;
    }

    c->used += (uint32_t)n;

    while (c->used >= LOG_ENTRY_SIZE) {
        logEntry_t entry;
        memcpy(&entry, c->buf, LOG_ENTRY_SIZE);

        HandleLogEntry(&entry);

        // 将剩余数据左移到 buf 头部
        uint32_t remain = c->used - LOG_ENTRY_SIZE;
        if (remain > 0) {
            memmove(c->buf, c->buf + LOG_ENTRY_SIZE, remain);
        }
        c->used = remain;
    }

    return 0;
}

// 初始化 listen socket (AF_UNIX SOCK_STREAM)
int32_t SilogDaemonInitSocket(void)
{
    int32_t fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    // unlink 旧 socket 文件（忽略错误）
    unlink(SILOGD_SOCKET_PATH);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SILOGD_SOCKET_PATH, sizeof(addr.sun_path) - 1);//TODO:修改

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    if (SetNonblocking(fd) < 0) {
        perror("set_nonblocking listen");
        close(fd);
        return -1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = HandleSignal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    gListenFd = fd;
    printf("[silogd] listening on %s (stream)\n", SILOGD_SOCKET_PATH);
    return fd;
}

// 主循环：使用 poll 管理连接，处理每个客户端的半包/粘包
void SilogDaemonStartLoop(int32_t serverFd)
{
    if (serverFd < 0)
        return;

    client_ctx_t clients[MAX_CLIENTS];
    struct pollfd pfds[MAX_CLIENTS + 1];
    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < MAX_CLIENTS; ++i)
        clients[i].fd = -1;

    pfds[0].fd = serverFd;
    pfds[0].events = POLLIN;

    for (int i = 1; i <= MAX_CLIENTS; ++i) {
        pfds[i].fd = -1;
        pfds[i].events = POLLIN;
    }

    while (!gTerminate) {
        // 填充 pollfd 列表（serverFd 在 pfds[0]）
        int idx = 1;
        for (int i = 0; i < MAX_CLIENTS && idx <= MAX_CLIENTS; ++i) {
            if (clients[i].fd >= 0) {
                pfds[idx].fd = clients[i].fd;
                pfds[idx].events = POLLIN;
            } else {
                pfds[idx].fd = -1;
                pfds[idx].events = 0;
            }
            ++idx;
        }

        int timeoutMs = 500; // 半秒，可调整
        int ready = poll(pfds, MAX_CLIENTS + 1, timeoutMs);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }
        if (ready == 0)
            continue;

        // 可读的新连接
        if (pfds[0].revents & POLLIN) {
            while (1) {
                int32_t cfd = accept(serverFd, NULL, NULL);
                if (cfd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    perror("accept");
                    break;
                }

                if (SetNonblocking(cfd) < 0) {
                    perror("set_nonblocking client");
                    close(cfd);
                    continue;
                }

                int placed = 0;
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (clients[i].fd < 0) {
                        clients[i].fd = cfd;
                        clients[i].used = 0;
                        placed = 1;
                        printf("[silogd] client connected (fd=%d) slot=%d\n", cfd, i);
                        break;
                    }
                }
                if (!placed) {
                    fprintf(stderr, "[silogd] too many clients, reject fd=%d\n", cfd);
                    close(cfd);
                }
            }
        }

        // 检查每个 client fd 是否有数据
        for (int p = 1; p <= MAX_CLIENTS; ++p) {
            if (pfds[p].fd < 0)
                continue;
            if (pfds[p].revents & (POLLIN | POLLERR | POLLHUP)) {
                int32_t cfd = pfds[p].fd;
                int idxClient = -1;
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (clients[i].fd == cfd) {
                        idxClient = i;
                        break;
                    }
                }
                if (idxClient < 0)
                    // 不认识的 fd（可能已经关闭），忽略
                    continue;

                int rc = RecvAndProcess(&clients[idxClient]);
                if (rc < 0) {
                    printf("[silogd] client fd=%d disconnected/err\n", clients[idxClient].fd);
                    CloseClient(&clients[idxClient]);
                }
            }
        }
    }

    // 退出前清理
    printf("[silogd] shutting down, closing clients...\n");
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].fd >= 0)
            CloseClient(&clients[i]);
    }
    if (serverFd >= 0)
        close(serverFd);
    unlink(SILOGD_SOCKET_PATH);
    gListenFd = -1;
    printf("[silogd] exited\n");
}
