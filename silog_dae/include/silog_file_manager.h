#ifndef SILOG_FILE_MANAGER_H
#define SILOG_FILE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 常量定义 */
#define SILOG_MAX_PATH_LEN 256
#define SILOG_MAX_NAME_LEN 64

/* 刷盘模式 */
typedef enum {
    SILOG_FLUSH_SYNC = 0,
    SILOG_FLUSH_ASYNC,
} SilogFlushMode;

/* 压缩模式 */
typedef enum {
    SILOG_COMPRESS_SYNC = 0, /* 同步压缩（阻塞直到压缩完成） */
    SILOG_COMPRESS_ASYNC,    /* 异步压缩（后台线程处理） */
} SilogCompressMode;

/* 日志文件配置 */
typedef struct {
    char logDir[SILOG_MAX_PATH_LEN];      /* 日志目录，默认 "/tmp/silog" */
    char logFileBase[SILOG_MAX_NAME_LEN]; /* 基础文件名，默认 "silog" */
    uint32_t maxFileSize;                 /* 单文件最大大小（字节），默认 10MB */
    uint32_t maxFileCount;                /* 最多保留的历史文件数量，默认 10 */
    bool enableCompression;               /* 是否压缩历史文件，默认 true */
    SilogCompressMode compressMode;       /* 压缩模式，默认 ASYNC */
    SilogFlushMode flushMode;             /* 刷盘模式，默认 ASYNC */
    uint32_t asyncFlushIntervalMs;        /* 异步刷盘间隔（毫秒），默认 1000 */
    uint32_t asyncFlushSize;              /* 异步模式下触发刷盘的字节数，默认 4KB */
    uint32_t rotateRetryCount;            /* 轮转失败重试次数，默认 3 */
    uint32_t rotateRetryDelayMs;          /* 轮转重试间隔（毫秒），默认 100 */
} SilogLogFileConfig;

/* ================ 初始化/反初始化 ================ */

/* 获取默认配置 */
void SilogFileManagerGetDefaultConfig(SilogLogFileConfig *config);

/* 初始化（使用自定义配置） */
int32_t SilogFileManagerInit(const SilogLogFileConfig *config);

/* 反初始化 */
void SilogFileManagerDeinit(void);

/* ================ 配置接口 ================ */

/* 设置日志目录 */
int32_t SilogFileManagerSetLogDir(const char *dir);

/* 设置基础文件名 */
int32_t SilogFileManagerSetLogFileBase(const char *base);

/* 设置最大文件大小 */
void SilogFileManagerSetMaxFileSize(uint32_t size);

/* 设置最大文件数量 */
void SilogFileManagerSetMaxFileCount(uint32_t count);

/* 设置是否压缩 */
void SilogFileManagerSetCompression(bool enable);

/* 设置压缩模式 */
void SilogFileManagerSetCompressMode(SilogCompressMode mode);

/* 设置刷盘模式 */
void SilogFileManagerSetFlushMode(SilogFlushMode mode);

/* 设置异步刷盘间隔 */
void SilogFileManagerSetAsyncFlushInterval(uint32_t intervalMs);

/* 设置异步刷盘大小阈值 */
void SilogFileManagerSetAsyncFlushSize(uint32_t size);

/* ================ 写入接口 ================ */

/* 写入已格式化的字符串 */
int32_t SilogFileManagerWriteRaw(const uint8_t *data, uint32_t len);

/* ================ 轮转接口 ================ */

/* 手动触发轮转 */
int32_t SilogFileManagerRotate(void);

/* 强制刷盘 */
void SilogFileManagerFlush(void);

/* ================ 查询接口 ================ */

/* 获取当前文件大小 */
uint32_t SilogFileManagerGetCurrentSize(void);

/* 获取当前日志文件路径 */
int32_t SilogFileManagerGetCurrentFilePath(char *path, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_FILE_MANAGER_H */
