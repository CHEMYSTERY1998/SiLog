#include "silog_daemon.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "silog_adapter.h"
#include "silog_error.h"
#include "silog_logger.h"
#include "silog_mpsc.h"
#include "silog_time.h"
#include "silog_trans.h"
#include "silog_utils.h"

#define LOG_DAEMON_FILE_PATH "/tmp/silog_daemon.txt"

#define SILOG_DAEMON(...) silog_daemon_log(__VA_ARGS__)
#define SILOG_DAEMON_E(fmt, ...) SILOG_DAEMON("[ERROR]" fmt, ##__VA_ARGS__)
#define SILOG_DAEMON_W(fmt, ...) SILOG_DAEMON("[WARNING]" fmt, ##__VA_ARGS__)
#define SILOG_DAEMON_I(fmt, ...) SILOG_DAEMON("[INFO]" fmt, ##__VA_ARGS__)
#define SILOG_DAEMON_D(fmt, ...) SILOG_DAEMON("[DEBUG]" fmt, ##__VA_ARGS__)

typedef struct {
    FILE *prelogFd;
    pthread_mutex_t lock;
    SiLogMpscQueue logQueue;
} SilogDaemonManager;

SilogDaemonManager g_silogDaemonMgr = {
    .prelogFd = NULL,
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

static inline void silog_daemon_log(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    int err = errno;
    /* 1. 生成完整日志 */
    int n = snprintf(buf, sizeof(buf), "[%s] ", strerror(err));
    va_start(ap, fmt);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    va_end(ap);
    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    pthread_mutex_lock(&g_silogDaemonMgr.lock);
    /* 2. 输出目标集合 */
#ifdef SILOG_EXE
    FILE *fds[2] = {stdout, g_silogDaemonMgr.prelogFd};
    int fdCount = 2;
#else
    FILE *fds[1] = {g_silogDaemonMgr.prelogFd ? g_silogDaemonMgr.prelogFd : stdout};
    int fdCount = 1;
#endif
    for (int i = 0; i < fdCount; i++) {
        if (!fds[i])
            continue;
        fputs(buf, fds[i]);
        fflush(fds[i]);
    }
    pthread_mutex_unlock(&g_silogDaemonMgr.lock);
}

/*
    至少需要三个线程，
    1、日志接收线程：负责接收应用进程发送过来的日志数据，并存入缓冲区
    2、日志写入线程：负责从缓冲区读取日志数据，并写入文件
    3、日志传输线程：负责将日志数据通过网络传输到指定客户端
*/
STATIC void *SilogDaemonRecvThreadFunc(void *arg)
{
    SilogTransInit(TRAN_TYPE_UDP);
    (void)arg;
    int32_t ret = SilogTransServerInit();
    if (ret != SILOG_OK) {
        SILOG_DAEMON_E("SilogTransServerInit failed: %d", ret);
        return NULL;
    }
    ret = SilogMpscQueueInit(&g_silogDaemonMgr.logQueue, sizeof(logEntry_t), 4096);
    logEntry_t entry;
    while (1) {
        int32_t n = SilogTransServerRecv(&entry, sizeof(logEntry_t));
        if (n > 0) {
            ret = SilogMpscQueuePush(&g_silogDaemonMgr.logQueue, &entry);
            if (ret != SILOG_OK) {
                SILOG_DAEMON_E("SilogMpscQueuePush failed: %d", ret);
            }
        } else {
            usleep(1000);
        }
    }

    return NULL;
}

STATIC int32_t SilogDaemonRecvThreadInit()
{
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, SilogDaemonRecvThreadFunc, NULL) != 0) {
        return SILOG_THREAD_CREATE;
    }
    if (pthread_detach(recv_thread) != 0) {
        return SILOG_THREAD_CREATE;
    }

    return SILOG_OK;
}

STATIC void *SilogDaemonWriteThreadFunc(void *arg)
{
    (void)arg;
    logEntry_t entry;
    while (1) {
        int32_t ret = SilogMpscQueuePop(&g_silogDaemonMgr.logQueue, &entry);
        if (ret == SILOG_OK) {
            // TODO:写入日志文件
            char timebuf[32];
            SilogFormatWallClockMs(entry.ts, timebuf, sizeof(timebuf));
            printf("[%s][%s][pid:%d tid:%d][%s][%s:%u] %s\n", timebuf, SilogLevelToName(entry.level), entry.pid,
                   entry.tid, entry.tag, entry.file, entry.line, entry.msg);
            pthread_mutex_unlock(&g_silogDaemonMgr.lock);
        } else {
            usleep(1000);
        }
    }

    return NULL;
}

STATIC int32_t SilogDaemonWriteThreadInit()
{
    pthread_t write_thread;
    if (pthread_create(&write_thread, NULL, SilogDaemonWriteThreadFunc, NULL) != 0) {
        return SILOG_THREAD_CREATE;
    }
    if (pthread_detach(write_thread) != 0) {
        SILOG_DAEMON_E("pthread_detach failed");
        return SILOG_THREAD_CREATE;
    }

    return SILOG_OK;
}

int32_t SilogDaemonInit()
{
    g_silogDaemonMgr.prelogFd = fopen(LOG_DAEMON_FILE_PATH, "w");
    if (g_silogDaemonMgr.prelogFd == NULL) {
        perror("fopen " LOG_DAEMON_FILE_PATH " failed");
        return SILOG_FILE_OPEN;
    }

    // 初始化日志接收线程
    int32_t ret = SilogDaemonRecvThreadInit();
    if (ret != SILOG_OK) {
        return ret;
    }

    // 初始化日志写入线程
    ret = SilogDaemonWriteThreadInit();
    if (ret != SILOG_OK) {
        SILOG_DAEMON_E("SilogDaemonWriteThreadInit failed: %d", ret);
        return ret;
    }

    // 初始化日志传输线程
    // SilogDaemonTransThreadInit();

    return SILOG_OK;
}

void SilogDaemonDeinit()
{
    if (g_silogDaemonMgr.prelogFd != NULL) {
        pthread_mutex_lock(&g_silogDaemonMgr.lock);
        fflush(g_silogDaemonMgr.prelogFd);
        fclose(g_silogDaemonMgr.prelogFd);
        g_silogDaemonMgr.prelogFd = NULL;
        pthread_mutex_unlock(&g_silogDaemonMgr.lock);
    }
    pthread_mutex_destroy(&g_silogDaemonMgr.lock);
}
