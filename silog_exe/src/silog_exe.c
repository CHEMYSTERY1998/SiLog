/**
 * @file silog_exe.c
 * @brief SiLog 主程序 - 守护进程启动器和管理工具
 *
 * 本程序用于：
 * 1. 启动 silogd 守护进程
 * 2. 管理守护进程（查看状态、停止等）
 * 3. 作为命令行工具发送测试日志
 */

#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "silog_daemon.h"
#include "silog_error.h"
#include "silog_ipc.h"
#include "silog_prelog.h"
#include "silog_remote.h"

#define PID_FILE "/tmp/silogd.pid"
#define DEFAULT_LOG_DIR "/tmp/silog"

/**
 * @brief 打印使用帮助
 */
static void SilogExePrintUsage(const char *program)
{
    printf("SiLog - 日志系统守护进程管理工具\n\n");
    printf("Usage: %s [command] [options]\n\n", program);
    printf("Commands:\n");
    printf("  start                启动守护进程\n");
    printf("  stop                 停止守护进程\n");
    printf("  status               查看守护进程状态\n");
    printf("  test                 发送测试日志\n");
    printf("\nOptions:\n");
    printf("  -d, --daemon         后台运行模式\n");
    printf("  -h, --help           显示帮助\n");
    printf("\nExamples:\n");
    printf("  %s start             # 前台启动守护进程\n", program);
    printf("  %s start -d          # 后台启动守护进程\n", program);
    printf("  %s status            # 查看守护进程状态\n", program);
    printf("  %s stop              # 停止守护进程\n", program);
    printf("  %s test              # 发送测试日志\n", program);
}

/**
 * @brief 检查守护进程是否运行
 * @return true 表示运行中，false 表示未运行
 */
static bool SilogExeDaemonIsRunning(void)
{
    SilogIpcInit(SILOG_IPC_TYPE_UNIX_DGRAM);
    int32_t ret = SilogIpcClientInit();
    if (ret == SILOG_OK) {
        SilogIpcClientClose();
        return true;
    }
    return false;
}

/**
 * @brief 将 PID 写入文件
 */
static void SilogExeWritePidFile(pid_t pid)
{
    FILE *fp = fopen(PID_FILE, "w");
    if (fp != NULL) {
        fprintf(fp, "%d\n", pid);
        fclose(fp);
    }
}

/**
 * @brief 从文件读取 PID
 */
static pid_t SilogExeReadPidFile(void)
{
    FILE *fp = fopen(PID_FILE, "r");
    if (fp == NULL) {
        return -1;
    }
    pid_t pid = -1;
    if (fscanf(fp, "%d", &pid) != 1) {
        pid = -1;
    }
    fclose(fp);
    return pid;
}

/**
 * @brief 删除 PID 文件
 */
static void SilogExeRemovePidFile(void)
{
    unlink(PID_FILE);
}

/**
 * @brief 前台启动守护进程
 */
static int32_t SilogExeStartForeground(void)
{
    /* 初始化预日志模块 */
    SilogPrelogConfig_t prelogConfig = {
        .path = "/tmp/silog_exe.txt",
        .minLevel = SILOG_PRELOG_LEVEL_DEBUG,
        .enableStdout = true,
    };
    (void)SilogPrelogInit(&prelogConfig);

    SILOG_PRELOG_I(SILOG_PRELOG_EXE, "SiLog Daemon starting (foreground)");

    int32_t ret = SilogDaemonInit();
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_EXE, "SilogDaemonInit failed: %d", ret);
        return ret;
    }

    SILOG_PRELOG_I(SILOG_PRELOG_EXE, "SiLog Daemon Started Successfully");
    printf("SiLog Daemon is running (PID: %d)\n", getpid());
    printf("Press Ctrl+C to stop\n\n");

    /* 主事件循环 */
    while (1) {
        sleep(1);
    }

    return 0;
}

/**
 * @brief 后台启动守护进程
 */
static int32_t SilogExeStartDaemon(void)
{
    if (SilogExeDaemonIsRunning()) {
        printf("SiLog Daemon is already running\n");
        return SILOG_OK;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return SILOG_SYS_CALL;
    }

    if (pid > 0) {
        /* 父进程：等待子进程启动 */
        printf("Starting SiLog Daemon...\n");

        /* 等待守护进程启动（最多 3 秒） */
        for (int i = 0; i < 30; i++) {
            usleep(100000); /* 100ms */
            if (SilogExeDaemonIsRunning()) {
                printf("SiLog Daemon started successfully (PID: %d)\n", pid);
                SilogExeWritePidFile(pid);
                return SILOG_OK;
            }
        }

        fprintf(stderr, "Failed to start SiLog Daemon\n");
        return SILOG_TIMEOUT;
    }

    /* 子进程：成为守护进程 */
    if (setsid() < 0) {
        perror("setsid failed");
        exit(1);
    }

    /* 重定向标准输入输出 */
    if (freopen("/dev/null", "r", stdin) == NULL) {
        /* 忽略错误 */
    }
    if (freopen("/dev/null", "w", stdout) == NULL) {
        /* 忽略错误 */
    }
    if (freopen("/dev/null", "w", stderr) == NULL) {
        /* 忽略错误 */
    }

    /* 初始化预日志模块（后台模式） */
    SilogPrelogConfig_t prelogConfig = {
        .path = "/tmp/silog_exe.txt",
        .minLevel = SILOG_PRELOG_LEVEL_DEBUG,
        .enableStdout = false,
    };
    (void)SilogPrelogInit(&prelogConfig);

    SILOG_PRELOG_I(SILOG_PRELOG_EXE, "SiLog Daemon starting (background)");

    int32_t ret = SilogDaemonInit();
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_EXE, "SilogDaemonInit failed: %d", ret);
        exit(1);
    }

    SILOG_PRELOG_I(SILOG_PRELOG_EXE, "SiLog Daemon Started (PID: %d)", getpid());

    /* 主事件循环 */
    while (1) {
        sleep(1);
    }

    exit(0);
}

/**
 * @brief 停止守护进程
 */
static int32_t SilogExeStopDaemon(void)
{
    if (!SilogExeDaemonIsRunning()) {
        printf("SiLog Daemon is not running\n");
        SilogExeRemovePidFile();
        return SILOG_OK;
    }

    pid_t pid = SilogExeReadPidFile();
    if (pid > 0) {
        /* 尝试优雅终止 */
        if (kill(pid, SIGTERM) == 0) {
            /* 等待进程退出（最多 3 秒） */
            for (int i = 0; i < 30; i++) {
                usleep(100000); /* 100ms */
                if (!SilogExeDaemonIsRunning()) {
                    printf("SiLog Daemon stopped\n");
                    SilogExeRemovePidFile();
                    return SILOG_OK;
                }
            }

            /* 强制终止 */
            printf("Force stopping SiLog Daemon...\n");
            kill(pid, SIGKILL);
        }
    }

    /* 清理 */
    SilogExeRemovePidFile();
    printf("SiLog Daemon stopped\n");
    return SILOG_OK;
}

/**
 * @brief 显示守护进程状态
 */
static int32_t SilogExeShowStatus(void)
{
    bool running = SilogExeDaemonIsRunning();

    printf("SiLog Daemon Status:\n");
    printf("  Running: %s\n", running ? "Yes" : "No");

    if (running) {
        pid_t pid = SilogExeReadPidFile();
        if (pid > 0) {
            printf("  PID: %d\n", pid);
        }

        /* 显示远程服务状态 */
        uint32_t clientCount = SilogRemoteGetClientCount();
        printf("  Remote clients: %u\n", clientCount);

        printf("\nSiLog is ready to receive logs.\n");
        printf("Use SILOG_I(), SILOG_D(), etc. macros to send logs.\n");
    } else {
        printf("\nSiLog Daemon is not running.\n");
        printf("Start it with: silog start\n");
    }

    return SILOG_OK;
}

/**
 * @brief 发送测试日志
 */
static int32_t SilogExeSendTestLogs(void)
{
    if (!SilogExeDaemonIsRunning()) {
        fprintf(stderr, "Error: SiLog Daemon is not running.\n");
        fprintf(stderr, "Start it with: silog start\n");
        return SILOG_TRANS_NOT_INIT;
    }

    printf("Sending test logs to SiLog Daemon...\n\n");

    /* 使用 IPC 直接发送测试日志 */
    SilogIpcInit(SILOG_IPC_TYPE_UNIX_DGRAM);
    int32_t ret = SilogIpcClientInit();
    if (ret != SILOG_OK) {
        fprintf(stderr, "Failed to connect to daemon: %d\n", ret);
        return ret;
    }

    /* 发送不同级别的测试日志 */
    logEntry_t entry;
    memset(&entry, 0, sizeof(entry));

    /* DEBUG 日志 */
    entry.ts = 0; /* 守护进程会填充 */
    entry.pid = getpid();
    entry.tid = getpid();
    entry.level = SILOG_DEBUG;
    strncpy(entry.tag, "Test", SILOG_TAG_MAX_LEN - 1);
    strncpy(entry.file, "test.cpp", SILOG_FILE_MAX_LEN - 1);
    strncpy(entry.msg, "This is a DEBUG test message", SILOG_MSG_MAX_LEN - 1);
    entry.line = 1;
    entry.msgLen = strlen(entry.msg);
    entry.enabled = 1;
    SilogIpcClientSend(&entry, sizeof(entry));
    printf("[DEBUG] Test message sent\n");
    usleep(100000); /* 100ms */

    /* INFO 日志 */
    entry.level = SILOG_INFO;
    strncpy(entry.msg, "This is an INFO test message", SILOG_MSG_MAX_LEN - 1);
    entry.msgLen = strlen(entry.msg);
    SilogIpcClientSend(&entry, sizeof(entry));
    printf("[INFO] Test message sent\n");
    usleep(100000);

    /* WARN 日志 */
    entry.level = SILOG_WARN;
    strncpy(entry.msg, "This is a WARN test message", SILOG_MSG_MAX_LEN - 1);
    entry.msgLen = strlen(entry.msg);
    SilogIpcClientSend(&entry, sizeof(entry));
    printf("[WARN] Test message sent\n");
    usleep(100000);

    /* ERROR 日志 */
    entry.level = SILOG_ERROR;
    strncpy(entry.msg, "This is an ERROR test message", SILOG_MSG_MAX_LEN - 1);
    entry.msgLen = strlen(entry.msg);
    SilogIpcClientSend(&entry, sizeof(entry));
    printf("[ERROR] Test message sent\n");

    SilogIpcClientClose();

    printf("\nTest logs sent successfully.\n");
    printf("Check the log file: /tmp/silog_daemon.txt\n");

    return SILOG_OK;
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[])
{
    /* 解析命令 */
    if (argc < 2) {
        SilogExePrintUsage(argv[0]);
        return 1;
    }

    const char *command = argv[1];

    /* 检查是否是后台运行标志 */
    bool daemon_mode = false;
    if (argc >= 3 && (strcmp(argv[2], "-d") == 0 || strcmp(argv[2], "--daemon") == 0)) {
        daemon_mode = true;
    }

    if (strcmp(command, "start") == 0) {
        if (daemon_mode) {
            return SilogExeStartDaemon();
        } else {
            return SilogExeStartForeground();
        }
    } else if (strcmp(command, "stop") == 0) {
        return SilogExeStopDaemon();
    } else if (strcmp(command, "status") == 0) {
        return SilogExeShowStatus();
    } else if (strcmp(command, "test") == 0) {
        return SilogExeSendTestLogs();
    } else if (strcmp(command, "-h") == 0 || strcmp(command, "--help") == 0) {
        SilogExePrintUsage(argv[0]);
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n\n", command);
        SilogExePrintUsage(argv[0]);
        return 1;
    }
}
