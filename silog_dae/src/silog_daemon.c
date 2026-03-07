#include "silog_daemon.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "silog_adapter.h"
#include "silog_error.h"
#include "silog_file_manager.h"
#include "silog_logger.h"
#include "silog_mpsc.h"
#include "silog_prelog.h"
#include "silog_securec.h"
#include "silog_time.h"
#include "silog_ipc.h"
#include "silog_utils.h"

#define US_PER_MS                 1000 // 微秒每毫秒
#define TIME_BUF_SIZE             32   // 时间字符串缓冲区大小
#define LOG_MSG_BUF_SIZE          512  // 日志消息缓冲区大小
#define DAEMON_LOG_QUEUE_CAPACITY 4096 // 守护进程日志队列容量

typedef struct {
    SiLogMpscQueue logQueue;
} SilogDaemonManager;

SilogDaemonManager g_silogDaemonMgr = {0};

/*
    至少需要三个线程，
    1、日志接收线程：负责接收应用进程发送过来的日志数据，并存入缓冲区
    2、日志写入线程：负责从缓冲区读取日志数据，并写入文件
    3、日志传输线程：负责将日志数据通过网络传输到指定客户端
*/
STATIC void *SilogDaemonRecvThreadFunc(void *arg)
{
    SilogIpcInit(SILOG_IPC_TYPE_UNIX_DGRAM);
    (void)arg;
    int32_t ret = SilogIpcServerInit();
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "SilogIpcServerInit failed: %d", ret);
        return NULL;
    }
    ret = SilogMpscQueueInit(&g_silogDaemonMgr.logQueue, sizeof(logEntry_t), DAEMON_LOG_QUEUE_CAPACITY);
    logEntry_t entry;
    while (1) {
        int32_t n = SilogIpcServerRecv(&entry, sizeof(logEntry_t));
        if (n > 0) {
            ret = SilogMpscQueuePush(&g_silogDaemonMgr.logQueue, &entry);
            if (ret != SILOG_OK) {
                SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "SilogMpscQueuePush failed: %d", ret);
            }
        } else {
            usleep(US_PER_MS);
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
    char timebuf[TIME_BUF_SIZE];
    char logbuf[LOG_MSG_BUF_SIZE];

    while (1) {
        int32_t ret = SilogMpscQueuePop(&g_silogDaemonMgr.logQueue, &entry);
        if (ret == SILOG_OK) {
            // 格式化日志
            SilogFormatWallClockMs(entry.ts, timebuf, sizeof(timebuf));
            int len = snprintf_s(logbuf, sizeof(logbuf), sizeof(logbuf) - 1, "[%s][%s][pid:%d tid:%d][%s][%s:%u] %s\n",
                                 timebuf, SilogLevelToName(entry.level), entry.pid, entry.tid, entry.tag, entry.file,
                                 entry.line, entry.msg);
            if (len < 0) {
                SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "snprintf_s failed");
                continue;
            }
            if (len > LOG_MSG_BUF_SIZE) {
                len = LOG_MSG_BUF_SIZE;
            }
            // 写入日志文件（自动处理轮转和刷盘）
            SilogFileManagerWriteRaw((const uint8_t *)logbuf, len);

#ifdef SILOG_EXE
            // 同时输出到控制台
            fputs(logbuf, stdout);
#endif
        } else {
            usleep(US_PER_MS);
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
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "pthread_detach failed");
        return SILOG_THREAD_CREATE;
    }

    return SILOG_OK;
}

int32_t SilogDaemonInit(void)
{
    /* 初始化预日志模块 */
    SilogPrelogConfig_t prelogConfig = {
        .path = "/tmp/silog_daemon.txt",
        .minLevel = SILOG_PRELOG_LEVEL_DEBUG,
        .enableStdout = false,
    };
    (void)SilogPrelogInit(&prelogConfig);

    /* 初始化日志文件管理器 */
    int32_t ret = SilogFileManagerInit(NULL);
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "SilogFileManagerInit failed: %d", ret);
        return ret;
    }

    /* 初始化日志接收线程 */
    ret = SilogDaemonRecvThreadInit();
    if (ret != SILOG_OK) {
        return ret;
    }

    /* 初始化日志写入线程 */
    ret = SilogDaemonWriteThreadInit();
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "SilogDaemonWriteThreadInit failed: %d", ret);
        return ret;
    }

    /* 初始化日志传输线程 */
    /* SilogDaemonTransThreadInit(); */

    return SILOG_OK;
}

void SilogDaemonDeinit(void)
{
    /* 反初始化预日志模块 */
    SilogPrelogDeinit();

    /* 反初始化日志文件管理器 */
    SilogFileManagerDeinit();
}
