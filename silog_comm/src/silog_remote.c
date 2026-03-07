#include "silog_remote.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "silog_adapter.h"
#include "silog_error.h"
#include "silog_prelog.h"
#include "silog_securec.h"

/**
 * @brief 远程客户端结构
 */
typedef struct {
    int32_t fd;              ///< 客户端 socket fd
    struct sockaddr_in addr; ///< 客户端地址
    bool active;             ///< 是否活跃
} SilogRemoteClient;

/**
 * @brief 远程服务管理器
 */
typedef struct {
    bool isInit;                ///< 是否已初始化
    int32_t listenFd;           ///< 监听 socket fd
    uint16_t listenPort;        ///< 监听端口
    uint32_t maxClients;        ///< 最大客户端数
    SilogRemoteClient *clients; ///< 客户端数组
    uint32_t clientCount;       ///< 当前客户端数量
    pthread_mutex_t lock;       ///< 保护客户端数组的互斥锁
} SilogRemoteManager;

static SilogRemoteManager g_silogRemoteMgr = {
    .isInit = false,
    .listenFd = -1,
    .listenPort = SILOG_REMOTE_DEFAULT_PORT,
    .maxClients = SILOG_REMOTE_DEFAULT_MAX_CLIENTS,
    .clients = NULL,
    .clientCount = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

/**
 * @brief 设置 socket 为非阻塞模式
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
STATIC int32_t SilogRemoteSetNonblock(int32_t fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return SILOG_NET_FILE_ERROR;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return SILOG_NET_FILE_ERROR;
    }
    return SILOG_OK;
}

/**
 * @brief 查找空闲的客户端槽位
 */
STATIC int32_t SilogRemoteFindFreeSlot(void)
{
    for (uint32_t i = 0; i < g_silogRemoteMgr.maxClients; i++) {
        if (!g_silogRemoteMgr.clients[i].active) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief 关闭指定客户端连接（调用时必须持有锁）
 */
STATIC void SilogRemoteCloseClientInternal(uint32_t idx)
{
    if (idx >= g_silogRemoteMgr.maxClients) {
        return;
    }

    SilogRemoteClient *client = &g_silogRemoteMgr.clients[idx];
    if (client->active && client->fd >= 0) {
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client->addr.sin_addr, addrStr, sizeof(addrStr));
        close(client->fd);
        SILOG_PRELOG_I(SILOG_PRELOG_DAEMON, "Remote client disconnected: %s:%d", addrStr,
                       ntohs(client->addr.sin_port));
    }

    if (client->active) {
        client->active = false;
        if (g_silogRemoteMgr.clientCount > 0) {
            g_silogRemoteMgr.clientCount--;
        }
    }
    client->fd = -1;
    (void)memset_s(&client->addr, sizeof(client->addr), 0, sizeof(client->addr));
}

/**
 * @brief 关闭指定客户端连接（带锁版本，供外部调用）
 */
STATIC void SilogRemoteCloseClientLocked(uint32_t idx)
{
    pthread_mutex_lock(&g_silogRemoteMgr.lock);
    SilogRemoteCloseClientInternal(idx);
    pthread_mutex_unlock(&g_silogRemoteMgr.lock);
}

int32_t SilogRemoteInit(const SilogRemoteConfig_t *config)
{
    int32_t ret;

    if (g_silogRemoteMgr.isInit) {
        SILOG_PRELOG_W(SILOG_PRELOG_DAEMON, "Remote service already initialized");
        return SILOG_OK;
    }

    /* 应用配置 */
    if (config != NULL) {
        g_silogRemoteMgr.listenPort = (config->listenPort == 0) ? SILOG_REMOTE_DEFAULT_PORT : config->listenPort;
        g_silogRemoteMgr.maxClients = (config->maxClients == 0) ? SILOG_REMOTE_DEFAULT_MAX_CLIENTS : config->maxClients;
    } else {
        g_silogRemoteMgr.listenPort = SILOG_REMOTE_DEFAULT_PORT;
        g_silogRemoteMgr.maxClients = SILOG_REMOTE_DEFAULT_MAX_CLIENTS;
    }

    /* 分配客户端数组 */
    g_silogRemoteMgr.clients = (SilogRemoteClient *)SiMalloc(sizeof(SilogRemoteClient) * g_silogRemoteMgr.maxClients);
    if (g_silogRemoteMgr.clients == NULL) {
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "Failed to allocate client array");
        return SILOG_OUT_OF_MEMORY;
    }

    /* 初始化客户端数组 */
    for (uint32_t i = 0; i < g_silogRemoteMgr.maxClients; i++) {
        g_silogRemoteMgr.clients[i].fd = -1;
        g_silogRemoteMgr.clients[i].active = false;
        (void)memset_s(&g_silogRemoteMgr.clients[i].addr, sizeof(g_silogRemoteMgr.clients[i].addr), 0,
                       sizeof(g_silogRemoteMgr.clients[i].addr));
    }
    g_silogRemoteMgr.clientCount = 0;

    /* 创建 TCP socket */
    g_silogRemoteMgr.listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_silogRemoteMgr.listenFd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "Failed to create listen socket: %s", strerror(errno));
        SiFree(g_silogRemoteMgr.clients);
        g_silogRemoteMgr.clients = NULL;
        return SILOG_NET_FILE_CREATE;
    }

    /* 设置端口复用 */
    int opt = 1;
    if (setsockopt(g_silogRemoteMgr.listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        SILOG_PRELOG_W(SILOG_PRELOG_DAEMON, "Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    /* 绑定地址 */
    struct sockaddr_in addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_silogRemoteMgr.listenPort);

    if (bind(g_silogRemoteMgr.listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "Failed to bind port %d: %s", g_silogRemoteMgr.listenPort, strerror(errno));
        close(g_silogRemoteMgr.listenFd);
        g_silogRemoteMgr.listenFd = -1;
        SiFree(g_silogRemoteMgr.clients);
        g_silogRemoteMgr.clients = NULL;
        return SILOG_NET_FILE_OPEN;
    }

    /* 开始监听 */
    if (listen(g_silogRemoteMgr.listenFd, SILOG_REMOTE_BACKLOG) < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "Failed to listen: %s", strerror(errno));
        close(g_silogRemoteMgr.listenFd);
        g_silogRemoteMgr.listenFd = -1;
        SiFree(g_silogRemoteMgr.clients);
        g_silogRemoteMgr.clients = NULL;
        return SILOG_NET_FILE_ERROR;
    }

    /* 设置为非阻塞模式 */
    ret = SilogRemoteSetNonblock(g_silogRemoteMgr.listenFd);
    if (ret != SILOG_OK) {
        close(g_silogRemoteMgr.listenFd);
        g_silogRemoteMgr.listenFd = -1;
        SiFree(g_silogRemoteMgr.clients);
        g_silogRemoteMgr.clients = NULL;
        return ret;
    }

    g_silogRemoteMgr.isInit = true;
    SILOG_PRELOG_I(SILOG_PRELOG_DAEMON, "Remote log service initialized on port %d", g_silogRemoteMgr.listenPort);

    return SILOG_OK;
}

void SilogRemoteDeinit(void)
{
    pthread_mutex_lock(&g_silogRemoteMgr.lock);

    if (!g_silogRemoteMgr.isInit) {
        pthread_mutex_unlock(&g_silogRemoteMgr.lock);
        return;
    }

    /* 关闭所有客户端连接 */
    for (uint32_t i = 0; i < g_silogRemoteMgr.maxClients; i++) {
        SilogRemoteCloseClientInternal(i);
    }

    /* 关闭监听 socket */
    if (g_silogRemoteMgr.listenFd >= 0) {
        close(g_silogRemoteMgr.listenFd);
        g_silogRemoteMgr.listenFd = -1;
    }

    /* 释放客户端数组 */
    if (g_silogRemoteMgr.clients != NULL) {
        SiFree(g_silogRemoteMgr.clients);
        g_silogRemoteMgr.clients = NULL;
    }

    g_silogRemoteMgr.clientCount = 0;
    g_silogRemoteMgr.isInit = false;

    pthread_mutex_unlock(&g_silogRemoteMgr.lock);

    SILOG_PRELOG_I(SILOG_PRELOG_DAEMON, "Remote log service deinitialized");
}

int32_t SilogRemoteAccept(void)
{
    if (!g_silogRemoteMgr.isInit) {
        return SILOG_NET_FILE_ERROR;
    }

    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    (void)memset_s(&clientAddr, sizeof(clientAddr), 0, sizeof(clientAddr));

    int32_t clientFd = accept(g_silogRemoteMgr.listenFd, (struct sockaddr *)&clientAddr, &addrLen);
    if (clientFd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SILOG_NET_TIMEOUT; /* 无新连接 */
        }
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "Failed to accept: %s", strerror(errno));
        return SILOG_NET_FILE_ERROR;
    }

    pthread_mutex_lock(&g_silogRemoteMgr.lock);

    /* 查找空闲槽位 */
    if (g_silogRemoteMgr.clientCount >= g_silogRemoteMgr.maxClients) {
        pthread_mutex_unlock(&g_silogRemoteMgr.lock);
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        SILOG_PRELOG_W(SILOG_PRELOG_DAEMON, "Max clients reached, rejecting connection from %s:%d",
                       addrStr, ntohs(clientAddr.sin_port));
        close(clientFd);
        return SILOG_NET_FILE_ERROR;
    }

    int32_t slot = SilogRemoteFindFreeSlot();
    if (slot < 0) {
        pthread_mutex_unlock(&g_silogRemoteMgr.lock);
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        SILOG_PRELOG_W(SILOG_PRELOG_DAEMON, "No free slot for connection from %s:%d",
                       addrStr, ntohs(clientAddr.sin_port));
        close(clientFd);
        return SILOG_NET_FILE_ERROR;
    }

    /* 设置为非阻塞模式 */
    int32_t ret = SilogRemoteSetNonblock(clientFd);
    if (ret != SILOG_OK) {
        pthread_mutex_unlock(&g_silogRemoteMgr.lock);
        close(clientFd);
        return ret;
    }

    /* 保存客户端信息 */
    SilogRemoteClient *client = &g_silogRemoteMgr.clients[slot];
    client->fd = clientFd;
    client->addr = clientAddr;
    client->active = true;
    g_silogRemoteMgr.clientCount++;

    pthread_mutex_unlock(&g_silogRemoteMgr.lock);

    char addrStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
    SILOG_PRELOG_I(SILOG_PRELOG_DAEMON, "Remote client connected: %s:%d (slot %d)", addrStr,
                   ntohs(clientAddr.sin_port), slot);

    return SILOG_OK;
}

/**
 * @brief 序列化日志条目为网络格式
 */
static void SilogRemoteSerializeEntry(const logEntry_t *entry, uint8_t *buf, size_t bufSize, size_t *outLen)
{
    if (buf == NULL || outLen == NULL || bufSize < SILOG_REMOTE_ENTRY_SIZE) {
        return;
    }

    size_t offset = 0;

    /* ts: uint64_t, big-endian */
    buf[offset++] = (uint8_t)((entry->ts >> 56) & 0xFF);
    buf[offset++] = (uint8_t)((entry->ts >> 48) & 0xFF);
    buf[offset++] = (uint8_t)((entry->ts >> 40) & 0xFF);
    buf[offset++] = (uint8_t)((entry->ts >> 32) & 0xFF);
    buf[offset++] = (uint8_t)((entry->ts >> 24) & 0xFF);
    buf[offset++] = (uint8_t)((entry->ts >> 16) & 0xFF);
    buf[offset++] = (uint8_t)((entry->ts >> 8) & 0xFF);
    buf[offset++] = (uint8_t)(entry->ts & 0xFF);

    /* pid: pid_t -> uint64_t, big-endian */
    uint64_t pid = (uint64_t)entry->pid;
    buf[offset++] = (uint8_t)((pid >> 56) & 0xFF);
    buf[offset++] = (uint8_t)((pid >> 48) & 0xFF);
    buf[offset++] = (uint8_t)((pid >> 40) & 0xFF);
    buf[offset++] = (uint8_t)((pid >> 32) & 0xFF);
    buf[offset++] = (uint8_t)((pid >> 24) & 0xFF);
    buf[offset++] = (uint8_t)((pid >> 16) & 0xFF);
    buf[offset++] = (uint8_t)((pid >> 8) & 0xFF);
    buf[offset++] = (uint8_t)(pid & 0xFF);

    /* tid: pid_t -> uint32_t, big-endian */
    uint32_t tid = (uint32_t)entry->tid;
    buf[offset++] = (uint8_t)((tid >> 24) & 0xFF);
    buf[offset++] = (uint8_t)((tid >> 16) & 0xFF);
    buf[offset++] = (uint8_t)((tid >> 8) & 0xFF);
    buf[offset++] = (uint8_t)(tid & 0xFF);

    /* level: uint8_t */
    buf[offset++] = (uint8_t)entry->level;

    /* line: int32_t, big-endian */
    buf[offset++] = (uint8_t)((entry->line >> 24) & 0xFF);
    buf[offset++] = (uint8_t)((entry->line >> 16) & 0xFF);
    buf[offset++] = (uint8_t)((entry->line >> 8) & 0xFF);
    buf[offset++] = (uint8_t)(entry->line & 0xFF);

    /* tag: fixed length */
    (void)memcpy_s(&buf[offset], bufSize - offset, entry->tag, SILOG_TAG_MAX_LEN);
    offset += SILOG_TAG_MAX_LEN;

    /* file: fixed length */
    (void)memcpy_s(&buf[offset], bufSize - offset, entry->file, SILOG_FILE_MAX_LEN);
    offset += SILOG_FILE_MAX_LEN;

    /* msg: fixed length */
    (void)memcpy_s(&buf[offset], bufSize - offset, entry->msg, SILOG_MSG_MAX_LEN);
    offset += SILOG_MSG_MAX_LEN;

    *outLen = offset;
}

/**
 * @brief 完全发送数据到客户端
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
static int32_t SilogRemoteSendFull(int32_t fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* 非阻塞模式下无法立即发送，返回特殊错误码 */
                return SILOG_NET_TIMEOUT;
            }
            return SILOG_NET_FILE_ERROR;
        }
        if (n == 0) {
            /* 连接已关闭 */
            return SILOG_NET_FILE_ERROR;
        }
        sent += (size_t)n;
    }

    return SILOG_OK;
}

int32_t SilogRemoteBroadcast(const logEntry_t *entry, int32_t *sentCount)
{
    if (sentCount != NULL) {
        *sentCount = 0;
    }

    if (!g_silogRemoteMgr.isInit) {
        return SILOG_TRANS_NOT_INIT;
    }

    if (entry == NULL) {
        return SILOG_NULL_PTR;
    }

    pthread_mutex_lock(&g_silogRemoteMgr.lock);

    if (g_silogRemoteMgr.clientCount == 0) {
        pthread_mutex_unlock(&g_silogRemoteMgr.lock);
        return SILOG_OK;
    }

    /* 序列化日志条目 */
    uint8_t buf[SILOG_REMOTE_ENTRY_SIZE];
    size_t bufLen = 0;
    SilogRemoteSerializeEntry(entry, buf, sizeof(buf), &bufLen);

    if (bufLen == 0) {
        pthread_mutex_unlock(&g_silogRemoteMgr.lock);
        return SILOG_INVALID_ARG;
    }

    int32_t actualSentCount = 0;

    for (uint32_t i = 0; i < g_silogRemoteMgr.maxClients; i++) {
        SilogRemoteClient *client = &g_silogRemoteMgr.clients[i];
        if (!client->active || client->fd < 0) {
            continue;
        }

        /* 先解锁再发送，避免长时间持有锁 */
        int32_t fd = client->fd;
        pthread_mutex_unlock(&g_silogRemoteMgr.lock);

        int32_t ret = SilogRemoteSendFull(fd, buf, bufLen);

        pthread_mutex_lock(&g_silogRemoteMgr.lock);

        if (ret != SILOG_OK) {
            if (ret == SILOG_NET_TIMEOUT) {
                /* 发送缓冲区满，跳过此客户端 */
                continue;
            }
            /* 发送失败，关闭连接 */
            SILOG_PRELOG_W(SILOG_PRELOG_DAEMON, "Failed to send to client %u: %s", i, strerror(errno));
            SilogRemoteCloseClientInternal(i);
        } else {
            actualSentCount++;
        }
    }

    pthread_mutex_unlock(&g_silogRemoteMgr.lock);

    if (sentCount != NULL) {
        *sentCount = actualSentCount;
    }

    return SILOG_OK;
}

uint32_t SilogRemoteGetClientCount(void)
{
    pthread_mutex_lock(&g_silogRemoteMgr.lock);
    uint32_t count = g_silogRemoteMgr.isInit ? g_silogRemoteMgr.clientCount : 0;
    pthread_mutex_unlock(&g_silogRemoteMgr.lock);
    return count;
}

bool SilogRemoteIsInit(void)
{
    return g_silogRemoteMgr.isInit;
}

/* ==================== 客户端实现 ==================== */

/**
 * @brief 远程客户端上下文
 */
typedef struct {
    bool isInit;                       ///< 是否已初始化
    bool isConnected;                  ///< 是否已连接
    int32_t socketFd;                  ///< 客户端 socket fd
    SilogRemoteClientConfig config;    ///< 客户端配置
    uint8_t recvBuf[SILOG_REMOTE_ENTRY_SIZE]; ///< 接收缓冲区
    size_t recvOffset;                 ///< 当前接收偏移
} SilogRemoteClientCtx;

static SilogRemoteClientCtx g_silogRemoteClient = {
    .isInit = false,
    .isConnected = false,
    .socketFd = -1,
    .recvOffset = 0,
};

/**
 * @brief 设置 socket 为非阻塞模式
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
static int32_t SilogRemoteClientSetNonblock(int32_t fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return SILOG_NET_FILE_ERROR;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return SILOG_NET_FILE_ERROR;
    }
    return SILOG_OK;
}

int32_t SilogRemoteClientInit(const SilogRemoteClientConfig *config)
{
    if (g_silogRemoteClient.isInit) {
        SILOG_PRELOG_W(SILOG_PRELOG_CAT, "Remote client already initialized");
        return SILOG_OK;
    }

    /* 应用配置 */
    if (config != NULL) {
        (void)memcpy_s(&g_silogRemoteClient.config, sizeof(g_silogRemoteClient.config),
                       config, sizeof(SilogRemoteClientConfig));
        if (g_silogRemoteClient.config.serverAddr[0] == '\0') {
            (void)strncpy_s(g_silogRemoteClient.config.serverAddr,
                            sizeof(g_silogRemoteClient.config.serverAddr),
                            SILOG_REMOTE_DEFAULT_SERVER_ADDR,
                            sizeof(SILOG_REMOTE_DEFAULT_SERVER_ADDR) - 1);
        }
        if (g_silogRemoteClient.config.serverPort == 0) {
            g_silogRemoteClient.config.serverPort = SILOG_REMOTE_DEFAULT_PORT;
        }
        if (g_silogRemoteClient.config.reconnectMs == 0) {
            g_silogRemoteClient.config.reconnectMs = SILOG_REMOTE_DEFAULT_RECONNECT_MS;
        }
    } else {
        (void)strncpy_s(g_silogRemoteClient.config.serverAddr,
                        sizeof(g_silogRemoteClient.config.serverAddr),
                        SILOG_REMOTE_DEFAULT_SERVER_ADDR,
                        sizeof(SILOG_REMOTE_DEFAULT_SERVER_ADDR) - 1);
        g_silogRemoteClient.config.serverPort = SILOG_REMOTE_DEFAULT_PORT;
        g_silogRemoteClient.config.useReconnect = true;
        g_silogRemoteClient.config.reconnectMs = SILOG_REMOTE_DEFAULT_RECONNECT_MS;
    }

    g_silogRemoteClient.socketFd = -1;
    g_silogRemoteClient.isConnected = false;
    g_silogRemoteClient.recvOffset = 0;
    g_silogRemoteClient.isInit = true;

    SILOG_PRELOG_I(SILOG_PRELOG_CAT, "Remote client initialized, server=%s:%d",
                   g_silogRemoteClient.config.serverAddr, g_silogRemoteClient.config.serverPort);

    return SILOG_OK;
}

void SilogRemoteClientDeinit(void)
{
    if (!g_silogRemoteClient.isInit) {
        return;
    }

    SilogRemoteClientDisconnect();

    g_silogRemoteClient.isInit = false;
    g_silogRemoteClient.recvOffset = 0;

    SILOG_PRELOG_I(SILOG_PRELOG_CAT, "Remote client deinitialized");
}

int32_t SilogRemoteClientConnect(void)
{
    if (!g_silogRemoteClient.isInit) {
        SILOG_PRELOG_E(SILOG_PRELOG_CAT, "Remote client not initialized");
        return SILOG_TRANS_NOT_INIT;
    }

    if (g_silogRemoteClient.isConnected) {
        return SILOG_OK;
    }

    /* 创建 TCP socket */
    g_silogRemoteClient.socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_silogRemoteClient.socketFd < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_CAT, "Failed to create socket: %s", strerror(errno));
        return SILOG_NET_FILE_CREATE;
    }

    /* 设置服务器地址 */
    struct sockaddr_in serverAddr;
    (void)memset_s(&serverAddr, sizeof(serverAddr), 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(g_silogRemoteClient.config.serverPort);

    if (inet_pton(AF_INET, g_silogRemoteClient.config.serverAddr, &serverAddr.sin_addr) <= 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_CAT, "Invalid server address: %s", g_silogRemoteClient.config.serverAddr);
        close(g_silogRemoteClient.socketFd);
        g_silogRemoteClient.socketFd = -1;
        return SILOG_INVALID_ARG;
    }

    /* 连接到服务器 */
    if (connect(g_silogRemoteClient.socketFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_CAT, "Failed to connect to %s:%d: %s",
                       g_silogRemoteClient.config.serverAddr, g_silogRemoteClient.config.serverPort, strerror(errno));
        close(g_silogRemoteClient.socketFd);
        g_silogRemoteClient.socketFd = -1;
        return SILOG_NET_CONNECT;
    }

    /* 设置为非阻塞模式 */
    (void)SilogRemoteClientSetNonblock(g_silogRemoteClient.socketFd);

    g_silogRemoteClient.isConnected = true;
    g_silogRemoteClient.recvOffset = 0;

    SILOG_PRELOG_I(SILOG_PRELOG_CAT, "Connected to remote server %s:%d",
                   g_silogRemoteClient.config.serverAddr, g_silogRemoteClient.config.serverPort);

    return SILOG_OK;
}

void SilogRemoteClientDisconnect(void)
{
    if (!g_silogRemoteClient.isInit) {
        return;
    }

    if (g_silogRemoteClient.socketFd >= 0) {
        close(g_silogRemoteClient.socketFd);
        g_silogRemoteClient.socketFd = -1;
    }

    g_silogRemoteClient.isConnected = false;
    g_silogRemoteClient.recvOffset = 0;

    SILOG_PRELOG_I(SILOG_PRELOG_CAT, "Disconnected from remote server");
}

bool SilogRemoteClientIsConnected(void)
{
    return g_silogRemoteClient.isInit && g_silogRemoteClient.isConnected;
}

int32_t SilogRemoteDeserializeEntry(const uint8_t *buf, size_t bufSize, logEntry_t *entry)
{
    if (buf == NULL || entry == NULL) {
        return SILOG_NULL_PTR;
    }

    if (bufSize < SILOG_REMOTE_ENTRY_SIZE) {
        return SILOG_INVALID_ARG;
    }

    size_t offset = 0;

    /* ts: uint64_t, big-endian */
    entry->ts = ((uint64_t)buf[offset] << 56) | ((uint64_t)buf[offset + 1] << 56 >> 8) |
                ((uint64_t)buf[offset + 2] << 48) | ((uint64_t)buf[offset + 3] << 48 >> 8) |
                ((uint64_t)buf[offset + 4] << 40) | ((uint64_t)buf[offset + 5] << 40 >> 8) |
                ((uint64_t)buf[offset + 6] << 32) | ((uint64_t)buf[offset + 7]);
    offset += SILOG_REMOTE_TS_SIZE;

    /* pid: uint64_t, big-endian */
    uint64_t pid = ((uint64_t)buf[offset] << 56) | ((uint64_t)buf[offset + 1] << 48) |
                   ((uint64_t)buf[offset + 2] << 40) | ((uint64_t)buf[offset + 3] << 32) |
                   ((uint64_t)buf[offset + 4] << 24) | ((uint64_t)buf[offset + 5] << 16) |
                   ((uint64_t)buf[offset + 6] << 8) | (uint64_t)buf[offset + 7];
    entry->pid = (pid_t)pid;
    offset += SILOG_REMOTE_PID_SIZE;

    /* tid: uint32_t, big-endian */
    uint32_t tid = ((uint32_t)buf[offset] << 24) | ((uint32_t)buf[offset + 1] << 16) |
                   ((uint32_t)buf[offset + 2] << 8) | (uint32_t)buf[offset + 3];
    entry->tid = (pid_t)tid;
    offset += SILOG_REMOTE_TID_SIZE;

    /* level: uint8_t */
    entry->level = (silogLevel)buf[offset];
    offset += SILOG_REMOTE_LEVEL_SIZE;

    /* line: int32_t, big-endian */
    int32_t line = ((int32_t)buf[offset] << 24) | ((int32_t)buf[offset + 1] << 16) |
                   ((int32_t)buf[offset + 2] << 8) | (int32_t)buf[offset + 3];
    entry->line = (uint32_t)line;
    offset += SILOG_REMOTE_LINE_SIZE;

    /* tag: fixed length */
    (void)memcpy_s(entry->tag, sizeof(entry->tag), &buf[offset], SILOG_TAG_MAX_LEN);
    entry->tag[SILOG_TAG_MAX_LEN - 1] = '\0';
    offset += SILOG_TAG_MAX_LEN;

    /* file: fixed length */
    (void)memcpy_s(entry->file, sizeof(entry->file), &buf[offset], SILOG_FILE_MAX_LEN);
    entry->file[SILOG_FILE_MAX_LEN - 1] = '\0';
    offset += SILOG_FILE_MAX_LEN;

    /* msg: fixed length */
    (void)memcpy_s(entry->msg, sizeof(entry->msg), &buf[offset], SILOG_MSG_MAX_LEN);
    entry->msg[SILOG_MSG_MAX_LEN - 1] = '\0';
    offset += SILOG_MSG_MAX_LEN;

    entry->msgLen = (uint16_t)strlen(entry->msg);
    entry->enabled = 1;

    return SILOG_OK;
}

int32_t SilogRemoteClientReceive(SilogRemoteLogCallback callback, void *userData, uint32_t timeoutMs)
{
    if (!g_silogRemoteClient.isInit) {
        return SILOG_TRANS_NOT_INIT;
    }

    if (!g_silogRemoteClient.isConnected) {
        return SILOG_NET_CLOSED;
    }

    if (callback == NULL) {
        return SILOG_NULL_PTR;
    }

    /* 使用 select 等待数据 */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(g_silogRemoteClient.socketFd, &readfds);

    struct timeval tv;
    struct timeval *tvPtr = NULL;
    if (timeoutMs > 0) {
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        tvPtr = &tv;
    }

    int ret = select(g_silogRemoteClient.socketFd + 1, &readfds, NULL, NULL, tvPtr);
    if (ret < 0) {
        if (errno == EINTR) {
            return SILOG_NET_TIMEOUT;
        }
        SILOG_PRELOG_E(SILOG_PRELOG_CAT, "select failed: %s", strerror(errno));
        SilogRemoteClientDisconnect();
        return SILOG_NET_FILE_ERROR;
    }

    if (ret == 0) {
        return SILOG_NET_TIMEOUT; /* 超时 */
    }

    /* 接收数据到缓冲区 */
    size_t needed = SILOG_REMOTE_ENTRY_SIZE - g_silogRemoteClient.recvOffset;
    ssize_t n = recv(g_silogRemoteClient.socketFd,
                     g_silogRemoteClient.recvBuf + g_silogRemoteClient.recvOffset,
                     needed, 0);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SILOG_NET_TIMEOUT;
        }
        SILOG_PRELOG_E(SILOG_PRELOG_CAT, "recv failed: %s", strerror(errno));
        SilogRemoteClientDisconnect();
        return SILOG_NET_RECV;
    }

    if (n == 0) {
        /* 服务器关闭连接 */
        SILOG_PRELOG_I(SILOG_PRELOG_CAT, "Server closed connection");
        SilogRemoteClientDisconnect();
        return SILOG_NET_CLOSED;
    }

    g_silogRemoteClient.recvOffset += (size_t)n;

    /* 处理完整的数据包 */
    while (g_silogRemoteClient.recvOffset >= SILOG_REMOTE_ENTRY_SIZE) {
        logEntry_t entry;
        (void)memset_s(&entry, sizeof(entry), 0, sizeof(entry));

        int32_t deserRet = SilogRemoteDeserializeEntry(g_silogRemoteClient.recvBuf, SILOG_REMOTE_ENTRY_SIZE, &entry);
        if (deserRet == SILOG_OK) {
            callback(&entry, userData);
        }

        /* 移动剩余数据 */
        size_t remaining = g_silogRemoteClient.recvOffset - SILOG_REMOTE_ENTRY_SIZE;
        if (remaining > 0) {
            (void)memmove_s(g_silogRemoteClient.recvBuf, sizeof(g_silogRemoteClient.recvBuf),
                            g_silogRemoteClient.recvBuf + SILOG_REMOTE_ENTRY_SIZE, remaining);
        }
        g_silogRemoteClient.recvOffset = remaining;
    }

    return SILOG_OK;
}

bool SilogRemoteClientIsInit(void)
{
    return g_silogRemoteClient.isInit;
}
