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
#include "silog_securec.h"

#define LOG_ENTRY_QUEUE_CAPACITY 1024
#define LOG_REE_FILE_PATH        "/tmp/silog_logger.txt"

// 时间单位转换
#define US_PER_MS    1000 // 微秒每毫秒
#define LOG_BUF_SIZE 1024 // 日志缓冲区大小

#define SILOG_LOGGER(...)        silog_logger_log(__VA_ARGS__)
#define SILOG_LOGGER_E(fmt, ...) SILOG_LOGGER("[ERROR]" fmt, ##__VA_ARGS__)
#define SILOG_LOGGER_W(fmt, ...) SILOG_LOGGER("[WARNING]" fmt, ##__VA_ARGS__)
#define SILOG_LOGGER_I(fmt, ...) SILOG_LOGGER("[INFO]" fmt, ##__VA_ARGS__)
#define SILOG_LOGGER_D(fmt, ...) SILOG_LOGGER("[DEBUG]" fmt, ##__VA_ARGS__)

typedef struct {
    silogLevel minLevel;
    FILE *prelogFd;
    pthread_once_t initOnce;
    SiLogMpscQueue logQueue;
    bool initSuccess;
    pthread_mutex_t lock;
} logEntryManager_t;

STATIC logEntryManager_t g_logEntryMgr = {
    .minLevel = SILOG_DEBUG,
    .prelogFd = NULL,
    .initOnce = PTHREAD_ONCE_INIT,
    .initSuccess = false,
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

static inline void silog_logger_log(const char *fmt, ...)
{
    char buf[LOG_BUF_SIZE];
    va_list ap;
    int err = errno;
    /* 1. 生成完整日志 */
    int n = snprintf(buf, sizeof(buf), "[%s] ", strerror(err));
    va_start(ap, fmt);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    va_end(ap);
    (void)strncat_s(buf, sizeof(buf), "\n", 1);
    pthread_mutex_lock(&g_logEntryMgr.lock);
    /* 2. 输出目标集合 */
#ifdef SILOG_EXE
    FILE *fds[2] = {stdout, g_logEntryMgr.prelogFd};
    int fdCount = 2;
#else
    FILE *fds[1] = {g_logEntryMgr.prelogFd ? g_logEntryMgr.prelogFd : stdout};
    int fdCount = 1;
#endif
    for (int i = 0; i < fdCount; i++) {
        if (!fds[i]) {
            continue;
        }
        fputs(buf, fds[i]);
        fflush(fds[i]);
    }
    pthread_mutex_unlock(&g_logEntryMgr.lock);
}

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
        SILOG_LOGGER_E("vsnprintf failed");
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
        SILOG_LOGGER_E("vsnprintf failed");
        return SILOG_STR_ERR;
    }
    if (snprintf(entry->file, SILOG_FILE_MAX_LEN, "%s", file) < 0) {
        SILOG_LOGGER_E("vsnprintf failed");
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
            SILOG_LOGGER_E("SilogTransClientSend failed, ret=%u", ret);
        }
    }
}

STATIC int32_t silogEntrySendTaskInit(void)
{
    pthread_t tid;
    SilogTransInit(TRAN_TYPE_UDP);
    int32_t ret = SilogTransClientInit();
    if (ret != SILOG_OK) {
        SILOG_LOGGER_E("trans init failed, ret=%u", ret);
        return ret;
    }

    ret = pthread_create(&tid, NULL, silogEntrySendHandle, NULL);
    if (ret != 0) {
        SILOG_LOGGER_E("pthread_create failed: %d", ret);
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

    int32_t ret = SilogMpscQueueInit(&g_logEntryMgr.logQueue, sizeof(logEntry_t), LOG_ENTRY_QUEUE_CAPACITY);
    if (ret != SILOG_OK) {
        SILOG_LOGGER_E("MPSC Queue init failed, ret=%u", ret);
        return;
    }

    ret = silogEntrySendTaskInit();
    if (ret != SILOG_OK) {
        SILOG_LOGGER_E("MPSC Send Task init failed, ret=%u", ret);
        return;
    }
    g_logEntryMgr.initSuccess = true;
    SILOG_LOGGER_I("SiLog socket initialized in constructor");
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
        SILOG_LOGGER_E("silogBuildEntry failed, ret=%u", ret);
        return;
    }
    ret = SilogMpscQueuePush(&g_logEntryMgr.logQueue, &entry);
    if (ret != SILOG_OK) {
        SILOG_LOGGER_E("silogSend failed, ret=%u", ret);
        return;
    }
}