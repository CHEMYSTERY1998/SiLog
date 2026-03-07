#include "silog_cat.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "silog_prelog.h"
#include "silog_remote.h"

/**
 * @brief silogcat 配置
 */
typedef struct {
    char serverAddr[64]; // 服务器地址
    uint16_t serverPort; // 服务器端口
    char filterTag[32];  // 标签过滤
    silogLevel minLevel; // 最小日志级别
    bool useColor;       // 彩色输出
    bool running;        // 运行标志
} SilogCatConfig;

static SilogCatConfig g_silogCatConfig = {
    .serverAddr = "127.0.0.1",
    .serverPort = 9090,
    .filterTag = "",
    .minLevel = SILOG_DEBUG,
    .useColor = false,
    .running = true,
};

/**
 * @brief 日志级别字符映射
 */
static const char g_levelChars[] = {'D', 'I', 'W', 'E', 'F'};

/**
 * @brief ANSI 颜色代码
 */
#define COLOR_RESET "\033[0m"
#define COLOR_DEBUG "\033[36m" // 青色
#define COLOR_INFO  "\033[32m" // 绿色
#define COLOR_WARN  "\033[33m" // 黄色
#define COLOR_ERROR "\033[31m" // 红色
#define COLOR_FATAL "\033[35m" // 紫色

/**
 * @brief 信号处理函数
 */
static void SilogCatSignalHandler(int sig)
{
    (void)sig;
    g_silogCatConfig.running = false;
}

/**
 * @brief 打印使用帮助
 */
static void SilogCatPrintUsage(const char *program)
{
    printf("Usage: %s [options]\n", program);
    printf("\nOptions:\n");
    printf("  -h, --help           Show this help message\n");
    printf("  -s, --server <addr>  Server address (default: 127.0.0.1)\n");
    printf("  -p, --port <port>    Server port (default: 9090)\n");
    printf("  -t, --tag <tag>      Filter by tag\n");
    printf("  -l, --level <level>  Minimum log level (DEBUG, INFO, WARN, ERROR, FATAL)\n");
    printf("  -c, --color          Enable colored output\n");
    printf("\n");
}

/**
 * @brief 解析日志级别字符串
 */
static silogLevel SilogCatParseLevel(const char *levelStr)
{
    if (strcasecmp(levelStr, "DEBUG") == 0 || strcasecmp(levelStr, "D") == 0) {
        return SILOG_DEBUG;
    }
    if (strcasecmp(levelStr, "INFO") == 0 || strcasecmp(levelStr, "I") == 0) {
        return SILOG_INFO;
    }
    if (strcasecmp(levelStr, "WARN") == 0 || strcasecmp(levelStr, "W") == 0) {
        return SILOG_WARN;
    }
    if (strcasecmp(levelStr, "ERROR") == 0 || strcasecmp(levelStr, "E") == 0) {
        return SILOG_ERROR;
    }
    if (strcasecmp(levelStr, "FATAL") == 0 || strcasecmp(levelStr, "F") == 0) {
        return SILOG_FATAL;
    }
    return SILOG_DEBUG;
}

/**
 * @brief 解析命令行参数
 */
static int SilogCatParseArgs(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            SilogCatPrintUsage(argv[0]);
            exit(0);
        }
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -s requires an argument\n");
                return -1;
            }
            strncpy(g_silogCatConfig.serverAddr, argv[++i], sizeof(g_silogCatConfig.serverAddr) - 1);
            g_silogCatConfig.serverAddr[sizeof(g_silogCatConfig.serverAddr) - 1] = '\0';
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -p requires an argument\n");
                return -1;
            }
            g_silogCatConfig.serverPort = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tag") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -t requires an argument\n");
                return -1;
            }
            strncpy(g_silogCatConfig.filterTag, argv[++i], sizeof(g_silogCatConfig.filterTag) - 1);
            g_silogCatConfig.filterTag[sizeof(g_silogCatConfig.filterTag) - 1] = '\0';
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--level") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -l requires an argument\n");
                return -1;
            }
            g_silogCatConfig.minLevel = SilogCatParseLevel(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--color") == 0) {
            g_silogCatConfig.useColor = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            SilogCatPrintUsage(argv[0]);
            return -1;
        }
    }
    return 0;
}

/**
 * @brief 格式化时间戳
 */
static void SilogCatFormatTime(uint64_t ts, char *buf, size_t bufSize)
{
    time_t sec = (time_t)(ts / 1000);
    int ms = (int)(ts % 1000);
    struct tm tm;
    localtime_r(&sec, &tm);
    snprintf(buf, bufSize, "%02d-%02d %02d:%02d:%02d.%03d", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
             ms);
}

/**
 * @brief 获取日志级别颜色
 */
static const char *SilogCatGetLevelColor(silogLevel level)
{
    if (!g_silogCatConfig.useColor) {
        return "";
    }
    switch (level) {
    case SILOG_DEBUG:
        return COLOR_DEBUG;
    case SILOG_INFO:
        return COLOR_INFO;
    case SILOG_WARN:
        return COLOR_WARN;
    case SILOG_ERROR:
        return COLOR_ERROR;
    case SILOG_FATAL:
        return COLOR_FATAL;
    default:
        return "";
    }
}

/**
 * @brief 日志输出回调
 */
static void SilogCatPrintLog(const logEntry_t *entry, void *userData)
{
    (void)userData;

    /* 级别过滤 */
    if (entry->level < g_silogCatConfig.minLevel) {
        return;
    }

    /* 标签过滤 */
    if (g_silogCatConfig.filterTag[0] != '\0' && strstr(entry->tag, g_silogCatConfig.filterTag) == NULL) {
        return;
    }

    /* 格式化时间 */
    char timeStr[32];
    SilogCatFormatTime(entry->ts, timeStr, sizeof(timeStr));

    /* 获取级别字符和颜色 */
    char levelChar = (entry->level < 5) ? g_levelChars[entry->level] : '?';
    const char *color = SilogCatGetLevelColor(entry->level);
    const char *reset = g_silogCatConfig.useColor ? COLOR_RESET : "";

    /* 输出日志 */
    printf("%s %5d %5d %s%c%s/%s: %s (%s:%d)\n", timeStr, (int)entry->pid, (int)entry->tid, color, levelChar, reset,
           entry->tag, entry->msg, entry->file, entry->line);
    fflush(stdout);
}

int32_t SilogDaemonClientInit(void)
{
    /* 初始化预日志模块 */
    SilogPrelogConfig_t prelogConfig = {
        .path = "/tmp/silog_cat.txt",
        .minLevel = SILOG_PRELOG_LEVEL_DEBUG,
        .enableStdout = false,
    };
    (void)SilogPrelogInit(&prelogConfig);

    SILOG_PRELOG_I(SILOG_PRELOG_CAT, "SilogCat initialized");
    return 0;
}

int main(int argc, char *argv[])
{
    /* 初始化预日志 */
    SilogDaemonClientInit();

    /* 解析命令行参数 */
    if (SilogCatParseArgs(argc, argv) != 0) {
        return 1;
    }

    /* 设置信号处理 */
    signal(SIGINT, SilogCatSignalHandler);
    signal(SIGTERM, SilogCatSignalHandler);

    /* 初始化远程客户端 */
    SilogRemoteClientConfig remoteConfig = {
        .serverPort = g_silogCatConfig.serverPort,
        .useReconnect = true,
        .reconnectMs = 5000,
    };
    strncpy(remoteConfig.serverAddr, g_silogCatConfig.serverAddr, sizeof(remoteConfig.serverAddr) - 1);
    remoteConfig.serverAddr[sizeof(remoteConfig.serverAddr) - 1] = '\0';

    int32_t ret = SilogRemoteClientInit(&remoteConfig);
    if (ret != 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_CAT, "Failed to initialize remote client");
        return 1;
    }

    printf("Connecting to %s:%d...\n", g_silogCatConfig.serverAddr, g_silogCatConfig.serverPort);
    if (g_silogCatConfig.filterTag[0] != '\0') {
        printf("Filtering tag: %s\n", g_silogCatConfig.filterTag);
    }
    if (g_silogCatConfig.minLevel > SILOG_DEBUG) {
        printf("Min level: %d\n", g_silogCatConfig.minLevel);
    }
    printf("Press Ctrl+C to exit\n\n");

    /* 主循环 */
    bool wasConnected = false;
    while (g_silogCatConfig.running) {
        /* 尝试连接（如果未连接） */
        if (!SilogRemoteClientIsConnected()) {
            if (wasConnected) {
                printf("\n[Disconnected, reconnecting...]\n");
                wasConnected = false;
            }
            ret = SilogRemoteClientConnect();
            if (ret == 0) {
                printf("[Connected to %s:%d]\n\n", g_silogCatConfig.serverAddr, g_silogCatConfig.serverPort);
                wasConnected = true;
            } else {
                sleep(1); // 连接失败等待1秒后重试
                continue;
            }
        }

        /* 接收日志 */
        ret = SilogRemoteClientReceive(SilogCatPrintLog, NULL, 100);
        if (ret != 0 && ret != -2) { // -2 是超时
            // 连接断开，下次循环会尝试重连
        }
    }

    printf("\n[Shutting down...]\n");
    SilogRemoteClientDeinit();
    return 0;
}
