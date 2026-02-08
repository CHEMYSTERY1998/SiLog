#include "silog_file_manager.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "silog_adapter.h"
#include "silog_error.h"
#include "silog_mpsc.h"
#include "silog_pqueue.h"
#include "silog_time.h"
#include "silog_utils.h"
#include "silog_securec.h"

// ================ 常量定义 ================

// 数据单位
#define BYTE_SIZE 1 // 字节大小

// 时间单位转换
#define MS_PER_SEC 1000 // 毫秒每秒
#define US_PER_MS  1000 // 微秒每毫秒

// 缓冲区大小
#define TIME_BUF_SIZE      32  // 时间字符串缓冲区大小
#define PATTERN_BUF_SIZE   128 // 模式匹配缓冲区大小
#define CMD_ARG_BUF_SIZE   32  // 命令参数缓冲区大小
#define MAX_LOG_FILE_COUNT 128 // 最大日志文件数量

// 默认配置
#define DEFAULT_LOG_DIR                 "/tmp/silog"
#define DEFAULT_LOG_FILE_BASE           "silog"
#define DEFAULT_MAX_FILE_SIZE           (10 * 1024 * 1024) // 10MB
#define DEFAULT_MAX_FILE_COUNT          10
#define DEFAULT_ENABLE_COMPRESSION      true
#define DEFAULT_COMPRESS_MODE           SILOG_COMPRESS_ASYNC
#define DEFAULT_FLUSH_MODE              SILOG_FLUSH_ASYNC
#define DEFAULT_ASYNC_FLUSH_INTERVAL_MS 1000
#define DEFAULT_ASYNC_FLUSH_SIZE        (4 * 1024) // 4KB
#define DEFAULT_ROTATE_RETRY_COUNT      3
#define DEFAULT_ROTATE_RETRY_DELAY_MS   100

#define PATH_MAX_LEN          512
#define COMPRESS_QUEUE_SIZE   64 // 压缩队列容量（必须是 2 的幂）
#define COMPRESS_PATH_MAX_LEN 512

// 压缩任务
typedef struct {
    char filePath[COMPRESS_PATH_MAX_LEN];
} CompressTask;

// 全局文件管理器
typedef struct {
    SilogLogFileConfig config;
    FILE *currentFd;          // 当前日志文件句柄
    uint32_t currentSize;     // 当前文件大小
    uint64_t asyncBufferSize; // 异步缓冲区已累积字节数
    uint64_t lastFlushTimeMs; // 上次刷盘时间
    bool initialized;         // 初始化标志
    pthread_mutex_t lock;     // 文件操作锁

    // 压缩线程相关
    pthread_t compressThread;     // 压缩线程
    SiLogMpscQueue compressQueue; // 压缩任务队列
    bool compressThreadRunning;   // 压缩线程运行标志
} SilogFileManager;

static SilogFileManager g_fileManager = {
    .currentFd = NULL,
    .currentSize = 0,
    .asyncBufferSize = 0,
    .lastFlushTimeMs = 0,
    .initialized = false,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .compressThreadRunning = false,
};

// ================ 内部辅助函数 ================

// 获取当前日志文件完整路径
STATIC void GetCurrentLogFilePath(char *path, size_t len)
{
    snprintf(path, len, "%s/%s.log", g_fileManager.config.logDir, g_fileManager.config.logFileBase);
}

// 确保日志目录存在
STATIC int32_t EnsureLogDirExists(void)
{
    struct stat st;
    if (stat(g_fileManager.config.logDir, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return SILOG_OK;
        }
        return SILOG_FILE_OPEN;
    }

    if (mkdir(g_fileManager.config.logDir, 0755) != 0 && errno != EEXIST) {
        return SILOG_FILE_OPEN;
    }
    return SILOG_OK;
}

// 格式化时间戳为文件名（YYYYMMDD_HHMMSS_mmm）
STATIC void FormatTimestampForFilename(uint64_t tsMs, char *buf, size_t len)
{
    time_t sec = tsMs / MS_PER_SEC;
    uint32_t msec = tsMs % MS_PER_SEC;
    struct tm tm;
    localtime_r(&sec, &tm);
    strftime(buf, len, "%Y%m%d_%H%M%S", &tm);
    // 追加毫秒部分
    snprintf(buf + strlen(buf), len - strlen(buf), "_%03u", msec);
}

// 文件信息（用于优先队列）
typedef struct {
    char path[PATH_MAX_LEN]; // 文件路径
    time_t mtime;            // 修改时间
} FileInfo;

// 小根堆比较函数（按修改时间升序，最旧的在堆顶）
STATIC int OldestFileCompare(const void *a, const void *b)
{
    const FileInfo *fa = (const FileInfo *)a;
    const FileInfo *fb = (const FileInfo *)b;
    if (fa->mtime < fb->mtime) {
        return -1; // a 更旧，优先级更高（小根堆）
    } else if (fa->mtime > fb->mtime) {
        return 1; // a 更新，优先级更低
    }
    return 0;
}

// 清理旧文件（使用优先队列找出需要删除的最旧文件）
STATIC int32_t CleanOldFiles(void)
{
    struct dirent **namelist;
    int n;
    uint32_t i;

    // 前缀模式
    char prefixPattern[PATTERN_BUF_SIZE];
    snprintf(prefixPattern, sizeof(prefixPattern), "%s_", g_fileManager.config.logFileBase);

    n = scandir(g_fileManager.config.logDir, &namelist, NULL, NULL);
    if (n < 0) {
        return SILOG_FILE_OPEN;
    }
    // 小根堆，容量足够大以容纳所有文件
    SiLogPQueue fileQueue;
    SilogPQueueInit(&fileQueue, sizeof(FileInfo), MAX_LOG_FILE_COUNT, OldestFileCompare);
    // 扫描目录，收集符合条件的文件
    for (uint32_t i = 0; i < (uint32_t)n; i++) {
        const char *name = namelist[i]->d_name;
        if (strncmp(name, prefixPattern, strlen(prefixPattern)) != 0) {
            continue;
        }
        const char *ext = strrchr(name, '.');
        if (ext == NULL || (strcmp(ext, ".log") != 0 && strcmp(ext, ".gz") != 0)) {
            continue;
        }
        struct stat st;
        FileInfo info = {.mtime = 0};
        snprintf(info.path, sizeof(info.path), "%s/%s", g_fileManager.config.logDir, name);
        if (stat(info.path, &st) == 0) {
            info.mtime = st.st_mtime;
            SilogPQueuePush(&fileQueue, &info);
        }
    }

    for (i = 0; i < (uint32_t)n; i++) {
        free(namelist[i]);
    }
    free(namelist);

    while (SilogPQueueSize(&fileQueue) > g_fileManager.config.maxFileCount) {
        FileInfo oldest;
        SilogPQueuePop(&fileQueue, &oldest);
        unlink(oldest.path);
    }
    SilogPQueueDestroy(&fileQueue);

    return SILOG_OK;
}

// 压缩文件（实际执行压缩）
STATIC void DoCompressFile(const char *filePath)
{
    char cmd[PATH_MAX_LEN + CMD_ARG_BUF_SIZE];
    snprintf(cmd, sizeof(cmd), "gzip \"%s\" 2>/dev/null", filePath);

    int ret = system(cmd);
    if (ret != 0) {
        // 压缩失败，可能 gzip 不存在，不影响日志继续写入
    }
}

// 压缩线程函数
STATIC void *CompressThreadFunc(void *arg)
{
    (void)arg;

    while (g_fileManager.compressThreadRunning) {
        CompressTask task;
        int32_t ret = SilogMpscQueuePop(&g_fileManager.compressQueue, &task);
        if (ret == SILOG_OK) {
            DoCompressFile(task.filePath);
        } else {
            // 队列为空，短暂休眠
            usleep(10000); // 10ms
        }
    }

    // 线程退出前处理剩余任务
    CompressTask task;
    while (SilogMpscQueuePop(&g_fileManager.compressQueue, &task) == SILOG_OK) {
        DoCompressFile(task.filePath);
    }

    return NULL;
}

// 提交压缩任务到队列或同步执行
STATIC int32_t CompressFile(const char *filePath)
{
    if (!g_fileManager.config.enableCompression) {
        return SILOG_OK;
    }

    if (g_fileManager.config.compressMode == SILOG_COMPRESS_ASYNC) {
        // 异步模式：提交任务到队列
        CompressTask task = {0};
        snprintf(task.filePath, sizeof(task.filePath), "%s", filePath);

        int32_t ret = SilogMpscQueuePush(&g_fileManager.compressQueue, &task);
        if (ret != SILOG_OK) {
            // 队列满，压缩失败但不影响日志继续写入
        }
    } else {
        // 同步模式：直接执行压缩
        DoCompressFile(filePath);
    }

    return SILOG_OK;
}

// 执行轮转
STATIC int32_t RotateInternal(void)
{
    char currentPath[PATH_MAX_LEN];
    char historyPath[PATH_MAX_LEN];
    char timestamp[TIME_BUF_SIZE];

    GetCurrentLogFilePath(currentPath, sizeof(currentPath));
    FormatTimestampForFilename(SilogGetNowMs(), timestamp, sizeof(timestamp));
    // 处理文件名冲突：如果文件已存在，添加序列号
    uint32_t seq = 0;
    while (seq < 10) { // 最多尝试 10 个序列号
        if (seq == 0) {
            snprintf(historyPath, sizeof(historyPath), "%s/%s_%s.log", g_fileManager.config.logDir,
                     g_fileManager.config.logFileBase, timestamp);
        } else {
            snprintf(historyPath, sizeof(historyPath), "%s/%s_%s_%u.log", g_fileManager.config.logDir,
                     g_fileManager.config.logFileBase, timestamp, seq);
        }
        struct stat st;
        if (stat(historyPath, &st) != 0) {
            break;
        }
        seq++;
    }
    // 关闭当前文件
    if (g_fileManager.currentFd != NULL) {
        fflush(g_fileManager.currentFd);
        fclose(g_fileManager.currentFd);
        g_fileManager.currentFd = NULL;
    }
    // 重命名为历史文件（带重试）
    uint32_t retry = 0;
    while (retry < g_fileManager.config.rotateRetryCount) {
        if (rename(currentPath, historyPath) == 0) {
            break;
        }
        retry++;
        if (retry < g_fileManager.config.rotateRetryCount) {
            usleep(g_fileManager.config.rotateRetryDelayMs * US_PER_MS);
        }
    }

    if (retry >= g_fileManager.config.rotateRetryCount) {
        // 轮转失败，重新打开原文件继续追加写入，避免数据丢失
        g_fileManager.currentFd = fopen(currentPath, "a");
        if (g_fileManager.currentFd == NULL) {
            return SILOG_FILE_OPEN;
        }
        fprintf(stderr, "[SiLog] Rotate failed: rename %s -> %s, errno=%d\n", currentPath, historyPath, errno);
        return SILOG_FILE_WRITE;
    }

    // 轮转成功，重新打开新文件
    g_fileManager.currentFd = fopen(currentPath, "w");
    if (g_fileManager.currentFd == NULL) {
        return SILOG_FILE_OPEN;
    }

    g_fileManager.currentSize = 0;
    g_fileManager.asyncBufferSize = 0;
    CompressFile(historyPath);
    CleanOldFiles();
    return SILOG_OK;
}

// 使用指定配置初始化
STATIC int32_t SilogFileManagerInitWithConfig(const SilogLogFileConfig *config)
{
    if (config == NULL) {
        return SILOG_INVALID_ARG;
    }

    pthread_mutex_lock(&g_fileManager.lock);

    if (g_fileManager.initialized) {
        pthread_mutex_unlock(&g_fileManager.lock);
        return SILOG_OK;
    }

    // 复制配置
    (void)memcpy_s(&g_fileManager.config, sizeof(g_fileManager.config),
                   config, sizeof(SilogLogFileConfig));

    // 确保目录存在
    int32_t ret = EnsureLogDirExists();
    if (ret != SILOG_OK) {
        pthread_mutex_unlock(&g_fileManager.lock);
        return ret;
    }

    // 打开当前日志文件
    char currentPath[PATH_MAX_LEN];
    GetCurrentLogFilePath(currentPath, sizeof(currentPath));
    g_fileManager.currentFd = fopen(currentPath, "a");
    if (g_fileManager.currentFd == NULL) {
        pthread_mutex_unlock(&g_fileManager.lock);
        return SILOG_FILE_OPEN;
    }

    // 获取当前文件大小
    struct stat st;
    if (stat(currentPath, &st) == 0) {
        g_fileManager.currentSize = (uint32_t)st.st_size;
    } else {
        g_fileManager.currentSize = 0;
    }

    // 初始化压缩队列
    ret = SilogMpscQueueInit(&g_fileManager.compressQueue, sizeof(CompressTask), COMPRESS_QUEUE_SIZE);
    if (ret != SILOG_OK) {
        fclose(g_fileManager.currentFd);
        g_fileManager.currentFd = NULL;
        pthread_mutex_unlock(&g_fileManager.lock);
        return ret;
    }

    // 启动压缩线程
    g_fileManager.compressThreadRunning = true;
    int pthreadRet = pthread_create(&g_fileManager.compressThread, NULL, CompressThreadFunc, NULL);
    if (pthreadRet != 0) {
        SilogMpscQueueDestroy(&g_fileManager.compressQueue);
        fclose(g_fileManager.currentFd);
        g_fileManager.currentFd = NULL;
        pthread_mutex_unlock(&g_fileManager.lock);
        return SILOG_THREAD_CREATE;
    }

    g_fileManager.asyncBufferSize = 0;
    g_fileManager.lastFlushTimeMs = SilogGetNowMs();
    g_fileManager.initialized = true;

    pthread_mutex_unlock(&g_fileManager.lock);
    return SILOG_OK;
}

// ================ 公共接口实现 ================

void SilogFileManagerGetDefaultConfig(SilogLogFileConfig *config)
{
    if (config == NULL) {
        return;
    }

    (void)memset_s(config, sizeof(*config), 0, sizeof(*config));

    snprintf(config->logDir, sizeof(config->logDir), "%s", DEFAULT_LOG_DIR);
    snprintf(config->logFileBase, sizeof(config->logFileBase), "%s", DEFAULT_LOG_FILE_BASE);
    config->maxFileSize = DEFAULT_MAX_FILE_SIZE;
    config->maxFileCount = DEFAULT_MAX_FILE_COUNT;
    config->enableCompression = DEFAULT_ENABLE_COMPRESSION;
    config->compressMode = DEFAULT_COMPRESS_MODE;
    config->flushMode = DEFAULT_FLUSH_MODE;
    config->asyncFlushIntervalMs = DEFAULT_ASYNC_FLUSH_INTERVAL_MS;
    config->asyncFlushSize = DEFAULT_ASYNC_FLUSH_SIZE;
    config->rotateRetryCount = DEFAULT_ROTATE_RETRY_COUNT;
    config->rotateRetryDelayMs = DEFAULT_ROTATE_RETRY_DELAY_MS;
}

int32_t SilogFileManagerInit(const SilogLogFileConfig *config)
{
    if (config == NULL) {
        SilogLogFileConfig defaultConfig;
        SilogFileManagerGetDefaultConfig(&defaultConfig);
        return SilogFileManagerInitWithConfig(&defaultConfig);
    }
    return SilogFileManagerInitWithConfig(config);
}

void SilogFileManagerDeinit(void)
{
    pthread_mutex_lock(&g_fileManager.lock);

    if (g_fileManager.currentFd != NULL) {
        fflush(g_fileManager.currentFd);
        fclose(g_fileManager.currentFd);
        g_fileManager.currentFd = NULL;
    }

    g_fileManager.initialized = false;

    // 停止压缩线程
    if (g_fileManager.compressThreadRunning) {
        g_fileManager.compressThreadRunning = false;
        pthread_mutex_unlock(&g_fileManager.lock);

        // 等待压缩线程结束
        pthread_join(g_fileManager.compressThread, NULL);

        // 销毁压缩队列
        SilogMpscQueueDestroy(&g_fileManager.compressQueue);

        return;
    }

    pthread_mutex_unlock(&g_fileManager.lock);
}

int32_t SilogFileManagerSetLogDir(const char *dir)
{
    if (dir == NULL) {
        return SILOG_INVALID_ARG;
    }

    pthread_mutex_lock(&g_fileManager.lock);

    if (g_fileManager.initialized) {
        pthread_mutex_unlock(&g_fileManager.lock);
        return SILOG_BUSY;
    }

    snprintf(g_fileManager.config.logDir, sizeof(g_fileManager.config.logDir), "%s", dir);

    pthread_mutex_unlock(&g_fileManager.lock);
    return SILOG_OK;
}

int32_t SilogFileManagerSetLogFileBase(const char *base)
{
    if (base == NULL) {
        return SILOG_INVALID_ARG;
    }

    pthread_mutex_lock(&g_fileManager.lock);

    if (g_fileManager.initialized) {
        pthread_mutex_unlock(&g_fileManager.lock);
        return SILOG_BUSY;
    }

    snprintf(g_fileManager.config.logFileBase, sizeof(g_fileManager.config.logFileBase), "%s", base);

    pthread_mutex_unlock(&g_fileManager.lock);
    return SILOG_OK;
}

void SilogFileManagerSetMaxFileSize(uint32_t size)
{
    pthread_mutex_lock(&g_fileManager.lock);
    g_fileManager.config.maxFileSize = size;
    pthread_mutex_unlock(&g_fileManager.lock);
}

void SilogFileManagerSetMaxFileCount(uint32_t count)
{
    pthread_mutex_lock(&g_fileManager.lock);
    g_fileManager.config.maxFileCount = count;
    pthread_mutex_unlock(&g_fileManager.lock);
}

void SilogFileManagerSetCompression(bool enable)
{
    pthread_mutex_lock(&g_fileManager.lock);
    g_fileManager.config.enableCompression = enable;
    pthread_mutex_unlock(&g_fileManager.lock);
}

void SilogFileManagerSetCompressMode(SilogCompressMode mode)
{
    pthread_mutex_lock(&g_fileManager.lock);
    g_fileManager.config.compressMode = mode;
    pthread_mutex_unlock(&g_fileManager.lock);
}

void SilogFileManagerSetFlushMode(SilogFlushMode mode)
{
    pthread_mutex_lock(&g_fileManager.lock);
    g_fileManager.config.flushMode = mode;
    pthread_mutex_unlock(&g_fileManager.lock);
}

void SilogFileManagerSetAsyncFlushInterval(uint32_t intervalMs)
{
    pthread_mutex_lock(&g_fileManager.lock);
    g_fileManager.config.asyncFlushIntervalMs = intervalMs;
    pthread_mutex_unlock(&g_fileManager.lock);
}

void SilogFileManagerSetAsyncFlushSize(uint32_t size)
{
    pthread_mutex_lock(&g_fileManager.lock);
    g_fileManager.config.asyncFlushSize = size;
    pthread_mutex_unlock(&g_fileManager.lock);
}

int32_t SilogFileManagerWriteRaw(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0) {
        return SILOG_INVALID_ARG;
    }

    pthread_mutex_lock(&g_fileManager.lock);

    if (!g_fileManager.initialized || g_fileManager.currentFd == NULL) {
        pthread_mutex_unlock(&g_fileManager.lock);
        return SILOG_FILE_MANAGER_NOT_INIT;
    }

    // 先检查是否需要轮转（如果写入后会超过最大文件大小）
    if (g_fileManager.currentSize + len >= g_fileManager.config.maxFileSize) {
        int32_t ret = RotateInternal();
        if (ret != SILOG_OK) {
            pthread_mutex_unlock(&g_fileManager.lock);
            return ret;
        }
    }

    // 写入数据
    size_t written = fwrite(data, BYTE_SIZE, len, g_fileManager.currentFd);
    if (written != len) {
        pthread_mutex_unlock(&g_fileManager.lock);
        return SILOG_FILE_WRITE;
    }

    g_fileManager.currentSize += len;

    // 刷盘处理
    if (g_fileManager.config.flushMode == SILOG_FLUSH_SYNC) {
        fflush(g_fileManager.currentFd);
        g_fileManager.asyncBufferSize = 0;
    } else {
        // 异步模式：判断是否需要刷盘
        g_fileManager.asyncBufferSize += len;
        uint64_t now = SilogGetNowMs();

        if (g_fileManager.asyncBufferSize >= g_fileManager.config.asyncFlushSize ||
            (now - g_fileManager.lastFlushTimeMs) >= g_fileManager.config.asyncFlushIntervalMs) {
            fflush(g_fileManager.currentFd);
            g_fileManager.asyncBufferSize = 0;
            g_fileManager.lastFlushTimeMs = now;
        }
    }

    pthread_mutex_unlock(&g_fileManager.lock);
    return SILOG_OK;
}

int32_t SilogFileManagerRotate(void)
{
    pthread_mutex_lock(&g_fileManager.lock);

    if (!g_fileManager.initialized) {
        pthread_mutex_unlock(&g_fileManager.lock);
        return SILOG_FILE_MANAGER_NOT_INIT;
    }

    int32_t ret = RotateInternal();

    pthread_mutex_unlock(&g_fileManager.lock);
    return ret;
}

void SilogFileManagerFlush(void)
{
    pthread_mutex_lock(&g_fileManager.lock);

    if (g_fileManager.currentFd != NULL) {
        fflush(g_fileManager.currentFd);
        g_fileManager.asyncBufferSize = 0;
        g_fileManager.lastFlushTimeMs = SilogGetNowMs();
    }

    pthread_mutex_unlock(&g_fileManager.lock);
}

uint32_t SilogFileManagerGetCurrentSize(void)
{
    pthread_mutex_lock(&g_fileManager.lock);
    uint32_t size = g_fileManager.currentSize;
    pthread_mutex_unlock(&g_fileManager.lock);
    return size;
}

int32_t SilogFileManagerGetCurrentFilePath(char *path, uint32_t len)
{
    if (path == NULL || len == 0) {
        return SILOG_INVALID_ARG;
    }

    pthread_mutex_lock(&g_fileManager.lock);
    GetCurrentLogFilePath(path, len);
    pthread_mutex_unlock(&g_fileManager.lock);

    return SILOG_OK;
}
