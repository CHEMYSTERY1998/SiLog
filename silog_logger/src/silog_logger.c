#include "silog_logger.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "silog_adapter.h"
#include "silog_error.h"
#include "silog_mpsc.h"
#include "silog_prelog.h"
#include "silog_securec.h"
#include "silog_time.h"
#include "silog_trans.h"
#include "silog_utils.h"

#define LOG_ENTRY_QUEUE_CAPACITY 1024

// 时间单位转换
#define US_PER_MS 1000 // 微秒每毫秒

typedef struct {
    silogLevel minLevel;
    pthread_once_t initOnce;
    SiLogMpscQueue logQueue;
    bool initSuccess;
} logEntryManager_t;

STATIC logEntryManager_t g_logEntryMgr = {
    .minLevel = SILOG_DEBUG,
    .initOnce = PTHREAD_ONCE_INIT,
    .initSuccess = false,
};

/* 设置最小日志级别 */
void silogSetLevel(silogLevel level)
{
    g_logEntryMgr.minLevel = level;
}

static inline bool silogCheckLevel(silogLevel level)
{
    return (level >= g_logEntryMgr.minLevel);
}

/* 构造 log_entry */
STATIC int32_t silogBuildEntry(logEntry_t *entry, silogLevel level, const char *tag, const char *file, uint32_t line,
                               const char *fmt, ...)
{
    (void)memset_s(entry, sizeof(*entry), 0, sizeof(*entry));

    va_list args;
    va_start(args, fmt);
    int32_t n = vsnprintf(entry->msg, SILOG_MSG_MAX_LEN, fmt, args);
    if (n < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "vsnprintf failed");
        va_end(args);
        return SILOG_STR_ERR;
    }
    va_end(args);

    entry->ts = SilogGetNowMs();
    entry->pid = getpid();
    entry->tid = getTid();
    entry->level = level;
    entry->line = line;

    int ret = snprintf_s(entry->tag, SILOG_TAG_MAX_LEN, SILOG_TAG_MAX_LEN - 1, "%s", tag);
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "snprintf_s failed");
        return SILOG_STR_ERR;
    }
    ret = snprintf_s(entry->file, SILOG_FILE_MAX_LEN, SILOG_FILE_MAX_LEN - 1, "%s", file);
    if (ret < 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "snprintf_s failed");
        return SILOG_STR_ERR;
    }

    entry->msgLen = n;
    entry->enabled = 1;

    return SILOG_OK;
}

STATIC void *silogEntrySendHandle(void *arg)
{
    (void)arg;
    while (1) {
        logEntry_t entry;
        int32_t ret = SilogMpscQueuePop(&g_logEntryMgr.logQueue, &entry);
        if (ret != SILOG_OK) {
            usleep(US_PER_MS);
            continue;
        }
        ret = SilogTransClientSend(&entry, sizeof(logEntry_t));
        if (ret != SILOG_OK) {
            SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "SilogTransClientSend failed, ret=%u", ret);
        }
    }
}

STATIC int32_t silogEntrySendTaskInit(void)
{
    pthread_t tid;
    SilogTransInit(SILOG_TRAN_TYPE_UDP);
    int32_t ret = SilogTransClientInit();
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "trans init failed, ret=%u", ret);
        return ret;
    }

    ret = pthread_create(&tid, NULL, silogEntrySendHandle, NULL);
    if (ret != 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "pthread_create failed: %d", ret);
        return SILOG_THREAD_CREATE;
    }

    pthread_detach(tid);
    return SILOG_OK;
}

STATIC void silogEntryMngInit(void)
{
    /* 初始化预日志模块 */
    SilogPrelogConfig_t prelogConfig = {
        .path = "/tmp/silog_logger.txt",
        .minLevel = SILOG_PRELOG_LEVEL_DEBUG,
        .enableStdout = false,
    };
    (void)SilogPrelogInit(&prelogConfig);

    int32_t ret = SilogMpscQueueInit(&g_logEntryMgr.logQueue, sizeof(logEntry_t), LOG_ENTRY_QUEUE_CAPACITY);
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "MPSC Queue init failed, ret=%u", ret);
        return;
    }

    ret = silogEntrySendTaskInit();
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "MPSC Send Task init failed, ret=%u", ret);
        return;
    }
    g_logEntryMgr.initSuccess = true;
    SILOG_PRELOG_I(SILOG_PRELOG_LOGGER, "SiLog socket initialized in constructor");
}

/* 核心日志打印接口 */
void silogLog(silogLevel level, const char *tag, const char *file, uint32_t line, const char *fmt, ...)
{
    pthread_once(&g_logEntryMgr.initOnce, silogEntryMngInit);
    if (!g_logEntryMgr.initSuccess) {
        return;
    }

    /* 日志级别过滤 */
    if (!silogCheckLevel(level)) {
        return;
    }

    logEntry_t entry;
    int32_t ret = silogBuildEntry(&entry, level, tag, file, line, fmt);
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "silogBuildEntry failed, ret=%u", ret);
        return;
    }
    ret = SilogMpscQueuePush(&g_logEntryMgr.logQueue, &entry);
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_LOGGER, "silogSend failed, ret=%u", ret);
        return;
    }
}