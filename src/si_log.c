#include "si_log_entry.h"
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
static int32_t g_sock_fd = -1;
static silog_level_t g_min_level = SILOG_DEBUG;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* 获取线程ID */
static pid_t get_tid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

static void silog_init_socket(void)
{
    if (__builtin_expect(g_sock_fd >= 0, 1))
        return;

    pthread_mutex_lock(&g_lock);
    if (g_sock_fd < 0) {
        g_sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (g_sock_fd < 0) {
            perror("socket");
            pthread_mutex_unlock(&g_lock);
            return;
        }
    }
    pthread_mutex_unlock(&g_lock);
}

/* 设置最小日志级别 */
void silog_set_level(silog_level_t level)
{
    g_min_level = level;
}

/* 预测分支判断 */
static inline uint8_t silog_check_level(silog_level_t level)
{
    return (level >= g_min_level) ? 1 : 0;
}

/* 构造 log_entry */
static void silog_build_entry(log_entry_t *entry, silog_level_t level, const char *tag, const char *file, uint32_t line,
                              const char *fmt, ...)
{
    memset(entry, 0, sizeof(*entry));
    entry->ts = (uint64_t)time(NULL) * 1000;
    entry->pid = getpid();
    entry->tid = get_tid();
    entry->level = level;
    strncpy(entry->tag, tag, SILOG_TAG_MAX_LEN - 1);
    strncpy(entry->file, file, SILOG_FILE_MAX_LEN - 1);
    entry->line = line;

    va_list args;
    va_start(args, fmt);
    int32_t n = vsnprintf(entry->msg, SILOG_MSG_MAX_LEN, fmt, args);
    va_end(args);
    entry->msg_len = (n > SILOG_MSG_MAX_LEN) ? SILOG_MSG_MAX_LEN : (uint16_t)n;
    entry->enabled = 1;
}

/* 发送 log_entry 到 logd */
static void silog_send(const log_entry_t *entry)
{
    if (!entry || g_sock_fd < 0)
        return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, LOGD_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    sendto(g_sock_fd, entry, sizeof(*entry), 0, (struct sockaddr *)&addr, sizeof(addr));
}

/* 核心日志打印接口 */
void silog_log(silog_level_t level, const char *tag, const char *file, uint32_t line, const char *fmt, ...)
{
    /* 日志级别过滤 */
    if (!silog_check_level(level)) {
        return;
    }

    log_entry_t entry;
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.msg, SILOG_MSG_MAX_LEN, fmt, args);
    va_end(args);

    silog_build_entry(&entry, level, tag, file, line, entry.msg);
    silog_send(&entry);
}

__attribute__((constructor)) static void silog_init_socket_ctor(void)
{
    silog_init_socket();
}