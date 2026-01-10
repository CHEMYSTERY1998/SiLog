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
#include "silog_time.h"
#include "silog_trans.h"
#include "silog_utils.h"

#define LOG_ENTRY_QUEUE_CAPACITY 1024
#define LOG_REE_FILE_PATH "/tmp/pre_silog.txt"
#define LOG_LOGGER_INFO(fmt, ...)                                                                                         \
    do {                                                                                                               \
        int _err = errno;                                                                                              \
        fprintf(g_logEntryMgr.prelogFd, "[%s:%d %s] [%s]: " fmt "\n", __FILE__, __LINE__, __func__, strerror(_err),    \
                ##__VA_ARGS__);                                                                                        \
    } while (0)

typedef struct {
    silogLevel minLevel;
    FILE *prelogFd;
    pthread_once_t initOnce;
    bool initSuccess;
} logEntryManager_t;

STATIC logEntryManager_t g_logEntryMgr = {
    .minLevel = SILOG_DEBUG,
    .prelogFd = NULL,
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
    memset(entry, 0, sizeof(*entry));

    va_list args;
    va_start(args, fmt);
    int32_t n = vsnprintf(entry->msg, SILOG_MSG_MAX_LEN, fmt, args);
    if (n < 0) {
        LOG_LOGGER_INFO("vsnprintf failed");
        va_end(args);
        return SILOG_STR_ERR;
    }
    va_end(args);

    entry->ts = SilogGetNowMs();
    entry->pid = getpid();
    entry->tid = getTid();
    entry->level = level;
    entry->line = line;

    if (snprintf(entry->tag, SILOG_TAG_MAX_LEN, "%s", tag) < 0) {
        LOG_LOGGER_INFO("vsnprintf failed");
        return SILOG_STR_ERR;
    }
    if (snprintf(entry->file, SILOG_FILE_MAX_LEN, "%s", file) < 0) {
        LOG_LOGGER_INFO("vsnprintf failed");
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
        int32_t ret = SilogMpscQueuePop(&entry);
        if (ret != SILOG_OK) {
            usleep(1000); // 1ms
            continue;
        }
        ret = SilogTransClientSend(&entry, sizeof(logEntry_t));
        if (ret != SILOG_OK) {
            LOG_LOGGER_INFO("SilogTransClientSend failed, ret=%u", ret);
        }
    }
}

STATIC int32_t silogEntrySendTaskInit(void)
{
    pthread_t tid;
    SilogTransInit(TRAN_TYPE_UDP);
    int32_t ret = SilogTransClientInit();
    if (ret != SILOG_OK) {
        LOG_LOGGER_INFO("trans init failed, ret=%u", ret);
        return ret;
    }

    ret = pthread_create(&tid, NULL, silogEntrySendHandle, NULL);
    if (ret != 0) {
        LOG_LOGGER_INFO("pthread_create failed: %d", ret);
        return SILOG_THREAD_CREATE;
    }

    pthread_detach(tid);
    return SILOG_OK;
}

STATIC void silogEntryMngInit(void)
{
    g_logEntryMgr.prelogFd = fopen(LOG_REE_FILE_PATH, "w");
    if (g_logEntryMgr.prelogFd == NULL) {
        perror("fopen " LOG_REE_FILE_PATH " failed");
    }

    int32_t ret = SilogMpscQueueInit(sizeof(logEntry_t), LOG_ENTRY_QUEUE_CAPACITY);
    if (ret != SILOG_OK) {
        LOG_LOGGER_INFO("MPSC Queue init failed, ret=%u", ret);
        return;
    }

    ret = silogEntrySendTaskInit();
    if (ret != SILOG_OK) {
        LOG_LOGGER_INFO("MPSC Send Task init failed, ret=%u", ret);
        return;
    }
    g_logEntryMgr.initSuccess = true;
    LOG_LOGGER_INFO("SiLog socket initialized in constructor");
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
        LOG_LOGGER_INFO("silogBuildEntry failed, ret=%u", ret);
        return;
    }
    ret = SilogMpscQueuePush(&entry);
    if (ret != SILOG_OK) {
        LOG_LOGGER_INFO("silogSend failed, ret=%u", ret);
        return;
    }
}