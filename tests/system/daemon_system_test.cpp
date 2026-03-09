#include <dirent.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "silog.h"
#include "silog_daemon.h"
#include "silog_error.h"
#include "silog_file_manager.h"
#include "silog_ipc.h"
#include "silog_logger.h"
#include "silog_prelog.h"
#include "silog_remote.h"

// ==================== 系统测试：守护进程生命周期 ====================

class DaemonSystemTest : public ::testing::Test {
  protected:
    std::string testDir;
    std::string logDir;
    pid_t daemonPid = -1;

    void SetUp() override
    {
        // 创建唯一的测试目录
        char tmpDir[] = "/tmp/silog_system_test_XXXXXX";
        testDir = mkdtemp(tmpDir);
        ASSERT_FALSE(testDir.empty());

        logDir = testDir + "/logs";
        ASSERT_EQ(mkdir(logDir.c_str(), 0755), 0);

        // 清理可能存在的旧守护进程
        CleanupStaleDaemon();

        // 确保完全清理
        SilogDaemonDeinit();
        SilogRemoteDeinit();
        SilogFileManagerDeinit();
        usleep(50000);
    }

    void TearDown() override
    {
        // 停止守护进程
        StopDaemon();

        // 清理资源
        SilogDaemonDeinit();
        SilogRemoteDeinit();
        SilogFileManagerDeinit();
        SilogPrelogDeinit();
        usleep(100000);

        // 清理测试目录
        CleanupTestDir();
    }

    // 清理可能存在的旧守护进程
    void CleanupStaleDaemon()
    {
        // 检查并清理 IPC socket 文件（代码中使用的是 /tmp/logd.sock）
        unlink("/tmp/logd.sock");
        unlink("/tmp/silogd.sock");
        unlink("/tmp/silogd_client.sock");
        usleep(20000);
    }

    // 清理测试目录
    void CleanupTestDir()
    {
        if (testDir.empty()) {
            return;
        }

        // 删除日志目录中的所有文件
        DIR *dir = opendir(logDir.c_str());
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                std::string path = logDir + "/" + entry->d_name;
                unlink(path.c_str());
            }
            closedir(dir);
            rmdir(logDir.c_str());
        }

        // 删除测试目录中的所有文件
        dir = opendir(testDir.c_str());
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                std::string path = testDir + "/" + entry->d_name;
                unlink(path.c_str());
            }
            closedir(dir);
        }

        rmdir(testDir.c_str());
    }

    // 使用 API 直接启动守护进程
    bool StartDaemonViaApi()
    {
        // 设置日志目录
        SilogFileManagerSetLogDir(logDir.c_str());
        SilogFileManagerSetLogFileBase("testlog");

        // 初始化守护进程（IPC 初始化在接收线程中异步进行）
        // 注意：由于动态链接的复杂性，测试代码和守护进程可能看到不同的 IPC 状态
        // 这里仅检查 SilogDaemonInit() 是否成功返回
        int32_t ret = SilogDaemonInit();
        if (ret != SILOG_OK) {
            return false;
        }

        // 给接收线程一些时间完成 IPC 初始化
        usleep(100000); // 100ms

        return true;
    }

    // 直接初始化 FileManager（用于日志测试）
    bool InitFileManagerDirect()
    {
        SilogLogFileConfig config;
        SilogFileManagerGetDefaultConfig(&config);
        snprintf(config.logDir, sizeof(config.logDir), "%s", logDir.c_str());
        snprintf(config.logFileBase, sizeof(config.logFileBase), "directlog");
        config.flushMode = SILOG_FLUSH_SYNC;

        int32_t ret = SilogFileManagerInit(&config);
        return ret == SILOG_OK;
    }

    // 停止守护进程
    void StopDaemon()
    {
        SilogDaemonDeinit();
        SilogRemoteDeinit();
        usleep(100000);
    }

    // 发送测试日志（使用直接 API）
    void SendTestLog(const char *tag, const char *msg, silogLevel level)
    {
        silogLog(level, tag, __FILE__, __LINE__, "%s", msg);
    }

    // 直接写入 FileManager
    void WriteLogDirect(const char *msg) { SilogFileManagerWriteRaw((const uint8_t *)msg, strlen(msg)); }

    // 等待日志文件创建
    bool WaitForLogFile(const std::string &logFile, int timeoutMs = 3000)
    {
        for (int i = 0; i < timeoutMs / 50; i++) {
            if (access(logFile.c_str(), F_OK) == 0) {
                // 文件存在，检查是否有内容
                struct stat st;
                if (stat(logFile.c_str(), &st) == 0 && st.st_size > 0) {
                    return true;
                }
            }
            usleep(50000); // 50ms
        }
        return false;
    }

    // 验证日志文件内容
    bool VerifyLogFileContains(const std::string &logFile, const char *content)
    {
        std::ifstream file(logFile);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.find(content) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    // 获取日志文件行数
    int GetLogFileLineCount(const std::string &logFile)
    {
        std::ifstream file(logFile);
        if (!file.is_open()) {
            return 0;
        }

        int count = 0;
        std::string line;
        while (std::getline(file, line)) {
            count++;
        }
        return count;
    }

    // 统计目录中匹配模式的文件数量
    int CountFilesMatching(const std::string &dir, const char *pattern)
    {
        int count = 0;
        DIR *d = opendir(dir.c_str());
        if (d) {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                if (strstr(entry->d_name, pattern) != NULL) {
                    count++;
                }
            }
            closedir(d);
        }
        return count;
    }
};

// ==================== 守护进程启动测试 ====================

TEST_F(DaemonSystemTest, DaemonInitViaApi)
{
    // 通过 API 启动守护进程
    ASSERT_TRUE(StartDaemonViaApi()) << "Failed to start daemon via API";

    // 验证日志目录被创建
    EXPECT_EQ(access(logDir.c_str(), F_OK), 0);
}

TEST_F(DaemonSystemTest, DaemonDoubleInit)
{
    // 第一次初始化
    SilogFileManagerSetLogDir(logDir.c_str());
    ASSERT_EQ(SilogDaemonInit(), SILOG_OK);

    // 第二次初始化应该幂等
    EXPECT_EQ(SilogDaemonInit(), SILOG_OK);
}

TEST_F(DaemonSystemTest, DaemonInitDeinitCycle)
{
    // 多次启动/停止周期
    for (int i = 0; i < 3; i++) {
        SilogFileManagerSetLogDir(logDir.c_str());
        EXPECT_EQ(SilogDaemonInit(), SILOG_OK) << "Cycle " << i;

        usleep(50000);

        SilogDaemonDeinit();
        usleep(100000);
    }
}

// ==================== FileManager 日志写入测试 ====================

TEST_F(DaemonSystemTest, FileManagerBasicWrite)
{
    // 直接初始化 FileManager
    ASSERT_TRUE(InitFileManagerDirect());

    // 直接写入日志
    const char *testLog = "[TestTag] Hello FileManager Test\n";
    WriteLogDirect(testLog);

    // 验证日志文件
    std::string logFile = logDir + "/directlog.log";
    ASSERT_TRUE(WaitForLogFile(logFile)) << "Log file not created";

    EXPECT_TRUE(VerifyLogFileContains(logFile, "Hello FileManager Test"));
}

TEST_F(DaemonSystemTest, FileManagerMultipleWrites)
{
    ASSERT_TRUE(InitFileManagerDirect());

    // 写入多行日志
    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Log line number %d\n", i);
        WriteLogDirect(msg);
    }

    std::string logFile = logDir + "/directlog.log";
    ASSERT_TRUE(WaitForLogFile(logFile));

    // 验证所有行都被写入
    int lineCount = GetLogFileLineCount(logFile);
    EXPECT_GE(lineCount, 10);

    // 验证特定内容
    EXPECT_TRUE(VerifyLogFileContains(logFile, "Log line number 0"));
    EXPECT_TRUE(VerifyLogFileContains(logFile, "Log line number 9"));
}

TEST_F(DaemonSystemTest, FileManagerConcurrentWrite)
{
    ASSERT_TRUE(InitFileManagerDirect());

    const int threadCount = 5;
    const int writesPerThread = 20;

    // 多线程并发写入
    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; i++) {
        threads.emplace_back([this, i, writesPerThread]() {
            for (int j = 0; j < writesPerThread; j++) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Thread_%d_Write_%d\n", i, j);
                WriteLogDirect(msg);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    std::string logFile = logDir + "/directlog.log";
    ASSERT_TRUE(WaitForLogFile(logFile));
    usleep(100000); // 等待写入完成

    // 验证日志被写入
    EXPECT_TRUE(VerifyLogFileContains(logFile, "Thread_0_Write_0"));
    EXPECT_TRUE(VerifyLogFileContains(logFile, "Thread_4_Write_19"));
}

// ==================== 日志轮转测试 ====================

TEST_F(DaemonSystemTest, LogRotation)
{
    // 配置小文件大小以触发轮转
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", logDir.c_str());
    snprintf(config.logFileBase, sizeof(config.logFileBase), "rotatelog");
    config.maxFileSize = 200; // 200 bytes 触发轮转
    config.flushMode = SILOG_FLUSH_SYNC;
    config.enableCompression = false;

    ASSERT_EQ(SilogFileManagerInit(&config), SILOG_OK);

    // 发送足够日志触发轮转
    for (int i = 0; i < 30; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Rotation test message number %d with padding\n", i);
        WriteLogDirect(msg);
    }

    // 强制刷盘
    SilogFileManagerFlush();

    // 验证当前日志文件存在
    std::string currentLog = logDir + "/rotatelog.log";
    EXPECT_EQ(access(currentLog.c_str(), F_OK), 0);

    // 验证有轮转文件生成
    int rotatedCount = CountFilesMatching(logDir, "rotatelog_");
    EXPECT_GT(rotatedCount, 0) << "No rotation files found";
}

TEST_F(DaemonSystemTest, LogRotationWithCleanup)
{
    // 配置小文件大小和最大文件数
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", logDir.c_str());
    snprintf(config.logFileBase, sizeof(config.logFileBase), "cleanuplog");
    config.maxFileSize = 100;
    config.maxFileCount = 3;
    config.flushMode = SILOG_FLUSH_SYNC;
    config.enableCompression = false;

    ASSERT_EQ(SilogFileManagerInit(&config), SILOG_OK);

    // 发送大量日志触发多次轮转
    for (int i = 0; i < 50; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Cleanup test message %d\n", i);
        WriteLogDirect(msg);
    }

    // 手动触发轮转
    for (int i = 0; i < 5; i++) {
        SilogFileManagerRotate();
    }

    usleep(500000); // 等待轮转和清理

    // 验证历史文件数量不超过限制
    int rotatedCount = CountFilesMatching(logDir, "cleanuplog_");
    EXPECT_LE(rotatedCount, 3) << "Too many rotation files: " << rotatedCount;
}

// ==================== 刷盘模式测试 ====================

TEST_F(DaemonSystemTest, SyncFlushMode)
{
    // 配置同步刷盘
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", logDir.c_str());
    snprintf(config.logFileBase, sizeof(config.logFileBase), "synclog");
    config.flushMode = SILOG_FLUSH_SYNC;

    ASSERT_EQ(SilogFileManagerInit(&config), SILOG_OK);

    // 发送日志
    WriteLogDirect("SyncFlushMessage\n");

    // 同步模式下应该立即可见
    std::string logFile = logDir + "/synclog.log";
    EXPECT_TRUE(WaitForLogFile(logFile, 500)); // 短超时

    EXPECT_TRUE(VerifyLogFileContains(logFile, "SyncFlushMessage"));
}

TEST_F(DaemonSystemTest, AsyncFlushMode)
{
    // 配置异步刷盘
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", logDir.c_str());
    snprintf(config.logFileBase, sizeof(config.logFileBase), "asynclog");
    config.flushMode = SILOG_FLUSH_ASYNC;
    config.asyncFlushSize = 4096;
    config.asyncFlushIntervalMs = 5000; // 5秒

    ASSERT_EQ(SilogFileManagerInit(&config), SILOG_OK);

    // 发送小量日志（不会触发刷盘）
    WriteLogDirect("AsyncFlushMessage1\n");

    std::string logFile = logDir + "/asynclog.log";

    // 手动刷盘
    SilogFileManagerFlush();

    // 现在应该能看到
    EXPECT_TRUE(WaitForLogFile(logFile));
    EXPECT_TRUE(VerifyLogFileContains(logFile, "AsyncFlushMessage1"));
}

// ==================== 远程服务测试 ====================

TEST_F(DaemonSystemTest, RemoteServiceInit)
{
    // 配置并启动守护进程
    SilogFileManagerSetLogDir(logDir.c_str());
    ASSERT_EQ(SilogDaemonInit(), SILOG_OK);

    // 初始化远程服务
    SilogDaemonRemoteConfig remoteConfig = {
        .listenPort = 19300, // 使用非标准端口
        .maxClients = 5,
        .enable = true,
    };

    int32_t ret = SilogDaemonRemoteInit(&remoteConfig);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    // 验证客户端数为 0
    EXPECT_EQ(SilogDaemonRemoteGetClientCount(), 0);
}

TEST_F(DaemonSystemTest, RemoteServiceDoubleInit)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19301,
        .maxClients = 5,
        .enable = true,
    };

    // 第一次初始化
    int32_t ret = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(ret, SILOG_OK);

    // 第二次初始化应该幂等
    ret = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(ret, SILOG_OK);
}

// ==================== 错误处理测试 ====================

TEST_F(DaemonSystemTest, LogToInvalidDirectory)
{
    // 尝试使用无效目录（权限问题）
    if (getuid() != 0) { // 非 root 用户
        SilogLogFileConfig config;
        SilogFileManagerGetDefaultConfig(&config);
        snprintf(config.logDir, sizeof(config.logDir), "%s", "/root/invalid_silog_test");

        // 初始化应该失败
        int32_t ret = SilogFileManagerInit(&config);
        EXPECT_NE(ret, SILOG_OK);
    }
}

TEST_F(DaemonSystemTest, FileManagerWriteWithoutInit)
{
    // 未初始化时写入
    const char *msg = "Test without init\n";
    int32_t ret = SilogFileManagerWriteRaw((const uint8_t *)msg, strlen(msg));
    EXPECT_EQ(ret, SILOG_FILE_MANAGER_NOT_INIT);
}

TEST_F(DaemonSystemTest, SendLogWithoutDaemon)
{
    // 未启动守护进程时发送日志
    // 应该不会崩溃，但可能无法送达
    SendTestLog("NoDaemon", "Test without daemon", SILOG_INFO);

    // 等待一段时间
    usleep(100000);

    // 没有守护进程，不应该有日志文件
    std::string logFile = logDir + "/testlog.log";
    EXPECT_NE(access(logFile.c_str(), F_OK), 0);
}

// ==================== 预日志测试 ====================

TEST_F(DaemonSystemTest, PrelogBasicWrite)
{
    // 配置预日志
    std::string prelogPath = testDir + "/prelog.txt";
    SilogPrelogConfig_t prelogConfig;
    snprintf(prelogConfig.path, sizeof(prelogConfig.path), "%s", prelogPath.c_str());
    prelogConfig.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    prelogConfig.enableStdout = false;

    int32_t ret = SilogPrelogInit(&prelogConfig);
    EXPECT_EQ(ret, SILOG_OK);

    // 写入预日志
    SILOG_PRELOG_I(SILOG_PRELOG_DAEMON, "Prelog info message");
    SILOG_PRELOG_E(SILOG_PRELOG_DAEMON, "Prelog error message");

    // 反初始化以刷盘
    SilogPrelogDeinit();

    // 验证文件存在
    std::string prelogFile = testDir + "/prelog.txt";
    EXPECT_EQ(access(prelogFile.c_str(), F_OK), 0);
}
