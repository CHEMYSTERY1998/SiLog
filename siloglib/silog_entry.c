#include "silog.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define LOGD_SOCKET_PATH "/tmp/logd.sock"

/* 全局配置 */
static int32_t g_sockFd = -1;
static silogLevel_t g_minLevel = SILOG_DEBUG;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* 获取线程ID */
static pid_t getTid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

static void silogInitSocket(void)
{
    if (__builtin_expect(g_sockFd >= 0, 1)) {
        return;
    }

    pthread_mutex_lock(&g_lock);
    if (g_sockFd < 0) {
        g_sockFd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (g_sockFd < 0) {
            perror("socket");
            pthread_mutex_unlock(&g_lock);
            return;
        }
    }
    pthread_mutex_unlock(&g_lock);
}

/* 设置最小日志级别 */
void silogSetLevel(silogLevel_t level)
{
    g_minLevel = level;
}

static inline uint8_t silogCheckLevel(silogLevel_t level)
{
    return (level >= g_minLevel) ? 1 : 0;
}

/* 构造 log_entry */
static void silogBuildEntry(logEntry_t *entry, silogLevel_t level, const char *tag, const char *file, uint32_t line,
                            const char *fmt, ...)
{
    memset(entry, 0, sizeof(*entry));
    entry->ts = (uint64_t)time(NULL) * 1000;
    entry->pid = getpid();
    entry->tid = getTid();
    entry->level = level;
    strncpy(entry->tag, tag, SILOG_TAG_MAX_LEN - 1);
    strncpy(entry->file, file, SILOG_FILE_MAX_LEN - 1);
    entry->line = line;

    va_list args;
    va_start(args, fmt);
    int32_t n = vsnprintf(entry->msg, SILOG_MSG_MAX_LEN, fmt, args);
    va_end(args);
    entry->msgLen = (n > SILOG_MSG_MAX_LEN) ? SILOG_MSG_MAX_LEN : (uint16_t)n;
    entry->enabled = 1;
}

/* 发送 log_entry 到 logd */
static void silogSend(const logEntry_t *entry)
{
    if (!entry || g_sockFd < 0) {
        return;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, LOGD_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    sendto(g_sockFd, entry, sizeof(*entry), 0, (struct sockaddr *)&addr, sizeof(addr));
}

/* 核心日志打印接口 */
void silogLog(silogLevel_t level, const char *tag, const char *file, uint32_t line, const char *fmt, ...)
{
    /* 日志级别过滤 */
    if (!silogCheckLevel(level)) {
        return;
    }

    logEntry_t entry;
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.msg, SILOG_MSG_MAX_LEN, fmt, args);
    va_end(args);

    silogBuildEntry(&entry, level, tag, file, line, entry.msg);
    silogSend(&entry);
}

__attribute__((constructor)) static void silogInitSocketCtor(void)
{
    silogInitSocket();
}