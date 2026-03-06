/**
 * @file silog_prelog.c
 * @brief SiLog 预日志模块实现
 */

#include "silog_prelog.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "silog_error.h"
#include "silog_securec.h"

#define SILOG_PRELOG_DEFAULT_PATH "/tmp/silog_prelog.txt"

typedef struct {
    FILE *file;
    pthread_mutex_t lock;
    SilogPrelogConfig_t config;
    bool initialized;
} SilogPrelogManager_t;

static SilogPrelogManager_t g_prelogMgr = {
    .file = NULL,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .config = {{0}},
    .initialized = false,
};

static const char *SilogPrelogLevelToString(SilogPrelogLevel_t level)
{
    switch (level) {
        case SILOG_PRELOG_LEVEL_DEBUG:
            return "DEBUG";
        case SILOG_PRELOG_LEVEL_INFO:
            return "INFO";
        case SILOG_PRELOG_LEVEL_WARNING:
            return "WARNING";
        case SILOG_PRELOG_LEVEL_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

int32_t SilogPrelogInit(const SilogPrelogConfig_t *config)
{
    pthread_mutex_lock(&g_prelogMgr.lock);

    if (g_prelogMgr.initialized) {
        pthread_mutex_unlock(&g_prelogMgr.lock);
        return SILOG_OK;
    }

    /* 应用配置 */
    if (config != NULL) {
        (void)memcpy_s(&g_prelogMgr.config, sizeof(g_prelogMgr.config), config, sizeof(SilogPrelogConfig_t));
    } else {
        /* 使用默认配置 */
        (void)strncpy_s(g_prelogMgr.config.path, sizeof(g_prelogMgr.config.path), SILOG_PRELOG_DEFAULT_PATH,
                        strlen(SILOG_PRELOG_DEFAULT_PATH));
        g_prelogMgr.config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
        g_prelogMgr.config.enableStdout = false;
    }

    /* 打开日志文件 */
    g_prelogMgr.file = fopen(g_prelogMgr.config.path, "w");
    if (g_prelogMgr.file == NULL) {
        /* 文件打开失败，如果未明确设置 enableStdout，则启用 stdout */
        if (!g_prelogMgr.config.enableStdout) {
            g_prelogMgr.config.enableStdout = true;
        }
    }

    g_prelogMgr.initialized = true;

    pthread_mutex_unlock(&g_prelogMgr.lock);

    return SILOG_OK;
}

void SilogPrelogDeinit(void)
{
    pthread_mutex_lock(&g_prelogMgr.lock);

    if (!g_prelogMgr.initialized) {
        pthread_mutex_unlock(&g_prelogMgr.lock);
        return;
    }

    if (g_prelogMgr.file != NULL) {
        (void)fflush(g_prelogMgr.file);
        (void)fclose(g_prelogMgr.file);
        g_prelogMgr.file = NULL;
    }

    g_prelogMgr.initialized = false;

    pthread_mutex_unlock(&g_prelogMgr.lock);
    pthread_mutex_destroy(&g_prelogMgr.lock);
}

int32_t SilogPrelogWriteV(const char *module, SilogPrelogLevel_t level, const char *fmt, va_list args)
{
    if (module == NULL || fmt == NULL) {
        return SILOG_NULL_PTR;
    }

    pthread_mutex_lock(&g_prelogMgr.lock);

    if (!g_prelogMgr.initialized) {
        pthread_mutex_unlock(&g_prelogMgr.lock);
        return SILOG_TRANS_NOT_INIT;
    }

    /* 级别过滤 */
    if (level < g_prelogMgr.config.minLevel) {
        pthread_mutex_unlock(&g_prelogMgr.lock);
        return SILOG_OK;
    }

    char buf[SILOG_PRELOG_MSG_MAX];
    int err = errno;

    /* 生成日志前缀: [errno_str] [module] [LEVEL] */
    int n = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "[%s] [%s] [%s] ", strerror(err), module,
                       SilogPrelogLevelToString(level));
    if (n < 0) {
        buf[0] = '\0';
        n = 0;
    }

    /* 格式化消息 */
    va_list args_copy;
    va_copy(args_copy, args);
    int msg_len = vsnprintf(buf + n, sizeof(buf) - n, fmt, args_copy);
    va_end(args_copy);

    if (msg_len < 0) {
        pthread_mutex_unlock(&g_prelogMgr.lock);
        return SILOG_STR_ERR;
    }

    /* 添加换行符 */
    (void)strncat_s(buf, sizeof(buf), "\n", 1);

    /* 输出到文件 */
    if (g_prelogMgr.file != NULL) {
        (void)fputs(buf, g_prelogMgr.file);
        (void)fflush(g_prelogMgr.file);
    }

    /* 输出到 stdout */
    if (g_prelogMgr.config.enableStdout) {
        (void)fputs(buf, stdout);
        (void)fflush(stdout);
    }

    pthread_mutex_unlock(&g_prelogMgr.lock);

    return SILOG_OK;
}

int32_t SilogPrelogWrite(const char *module, SilogPrelogLevel_t level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int32_t ret = SilogPrelogWriteV(module, level, fmt, args);
    va_end(args);
    return ret;
}

bool SilogPrelogIsInitialized(void)
{
    pthread_mutex_lock(&g_prelogMgr.lock);
    bool initialized = g_prelogMgr.initialized;
    pthread_mutex_unlock(&g_prelogMgr.lock);
    return initialized;
}

FILE *SilogPrelogGetFile(void)
{
    pthread_mutex_lock(&g_prelogMgr.lock);
    FILE *file = g_prelogMgr.file;
    pthread_mutex_unlock(&g_prelogMgr.lock);
    return file;
}
