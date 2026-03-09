/**
 * @file prelog_integration_test.cpp
 * @brief SiLog 预日志模块集成测试
 *
 * 测试 prelog 模块与 silog_logger 和 silog_daemon 的集成
 */

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <unistd.h>

#include "gtest/gtest.h"
#include "silog_error.h"
#include "silog_prelog.h"

// 测试辅助函数：读取文件内容
static std::string ReadFileContent(const char *path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    return content;
}

// 测试辅助函数：删除文件
static void DeleteFile(const char *path)
{
    (void)remove(path);
}

// 测试辅助函数：检查文件是否存在
static bool FileExists(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

class PrelogIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 确保每次测试前清理状态
        SilogPrelogDeinit();
        DeleteFile("/tmp/silog_prelog.txt");
        DeleteFile("/tmp/silog_logger.txt");
        DeleteFile("/tmp/silog_daemon.txt");
    }

    void TearDown() override
    {
        SilogPrelogDeinit();
        DeleteFile("/tmp/silog_prelog.txt");
        DeleteFile("/tmp/silog_logger.txt");
        DeleteFile("/tmp/silog_daemon.txt");
    }
};

// 测试预日志模块独立使用
TEST_F(PrelogIntegrationTest,StandaloneUsage)
{
    const char *testPath = "/tmp/silog_prelog.txt";

    SilogPrelogConfig_t config;
    (void)strncpy(config.path, testPath, sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config.enableStdout = false;

    // 初始化
    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);
    EXPECT_TRUE(SilogPrelogIsInitialized());

    // 写入日志
    SILOG_PRELOG_I("SILOG_PRELOG_COMM", "Standalone test message");
    SILOG_PRELOG_D("SILOG_PRELOG_COMM", "Debug: %d", 42);

    // 反初始化
    SilogPrelogDeinit();

    // 验证文件内容
    EXPECT_TRUE(FileExists(testPath));
    std::string content = ReadFileContent(testPath);
    EXPECT_NE(content.find("Standalone test message"), std::string::npos);
    EXPECT_NE(content.find("Debug: 42"), std::string::npos);
    EXPECT_NE(content.find("[INFO]"), std::string::npos);
}

// 测试预日志模块重复初始化处理
TEST_F(PrelogIntegrationTest,MultipleInitHandling)
{
    const char *testPath1 = "/tmp/silog_prelog.txt";
    const char *testPath2 = "/tmp/silog_logger.txt";

    // 第一次初始化
    SilogPrelogConfig_t config1;
    (void)strncpy(config1.path, testPath1, sizeof(config1.path) - 1);
    config1.path[sizeof(config1.path) - 1] = '\0';
    config1.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config1.enableStdout = false;
    EXPECT_EQ(SilogPrelogInit(&config1), SILOG_OK);

    // 写入第一条日志
    SILOG_PRELOG_I("SILOG_PRELOG_COMM", "First init message");

    // 尝试第二次初始化（应该被忽略，保持第一次的配置）
    SilogPrelogConfig_t config2;
    (void)strncpy(config2.path, testPath2, sizeof(config2.path) - 1);
    config2.path[sizeof(config2.path) - 1] = '\0';
    config2.minLevel = SILOG_PRELOG_LEVEL_ERROR;
    config2.enableStdout = false;
    EXPECT_EQ(SilogPrelogInit(&config2), SILOG_OK);

    // 写入第二条日志
    SILOG_PRELOG_I("SILOG_PRELOG_COMM", "Second init message");

    SilogPrelogDeinit();

    // 验证只有第一个文件存在（第二次初始化被忽略）
    EXPECT_TRUE(FileExists(testPath1));

    std::string content = ReadFileContent(testPath1);
    // 两条日志都应该在第一个文件中
    EXPECT_NE(content.find("First init message"), std::string::npos);
    EXPECT_NE(content.find("Second init message"), std::string::npos);
}

// 测试预日志级别过滤与并发写入
TEST_F(PrelogIntegrationTest,ConcurrentLevelFiltering)
{
    const char *testPath = "/tmp/silog_prelog.txt";

    // 设置级别为 WARNING，只有 WARNING 和 ERROR 会被记录
    SilogPrelogConfig_t config;
    (void)strncpy(config.path, testPath, sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_WARNING;
    config.enableStdout = false;

    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);

    const int numThreads = 5;
    const int logsPerThread = 50;
    std::thread threads[numThreads];

    // 创建多个线程，每个线程写入不同级别的日志
    std::atomic<bool> threadDone[numThreads];
    for (int i = 0; i < numThreads; i++) {
        threadDone[i].store(false);
        threads[i] = std::thread([i, logsPerThread, &threadDone]() {
            for (int j = 0; j < logsPerThread; j++) {
                SILOG_PRELOG_D("SILOG_PRELOG_COMM", "Thread %d Debug %d", i, j);  // 应该被过滤
                SILOG_PRELOG_I("SILOG_PRELOG_COMM", "Thread %d Info %d", i, j);   // 应该被过滤
                SILOG_PRELOG_W("SILOG_PRELOG_COMM", "Thread %d Warning %d", i, j); // 应该被记录
                SILOG_PRELOG_E("SILOG_PRELOG_COMM", "Thread %d Error %d", i, j);   // 应该被记录
            }
            threadDone[i].store(true);
        });
    }

    // 等待所有线程完成（带超时）
    for (int i = 0; i < numThreads; i++) {
        int waitMs = 0;
#ifdef __SANITIZE_THREAD__
        const int timeoutMs = 60000; /* TSan 下 60 秒超时 */
#else
        const int timeoutMs = 30000; /* 正常模式 30 秒超时 */
#endif
        while (!threadDone[i].load() && waitMs < timeoutMs) {
            usleep(10000);
            waitMs += 10;
        }
        if (threadDone[i].load()) {
            threads[i].join();
        } else {
            // 超时，detach 线程避免阻塞
            threads[i].detach();
            ADD_FAILURE() << "Thread " << i << " join timeout";
        }
    }

    SilogPrelogDeinit();

    // 验证文件内容
    std::string content = ReadFileContent(testPath);

    // 验证 WARNING 和 ERROR 日志存在
    EXPECT_NE(content.find("[WARNING]"), std::string::npos);
    EXPECT_NE(content.find("[ERROR]"), std::string::npos);

    // 验证 DEBUG 和 INFO 日志不存在
    EXPECT_EQ(content.find("[DEBUG]"), std::string::npos);
    EXPECT_EQ(content.find("Thread 0 Debug"), std::string::npos);
    EXPECT_EQ(content.find("Thread 0 Info"), std::string::npos);

    // 计算行数，应该是 numThreads * logsPerThread * 2 (WARNING + ERROR)
    int lineCount = 0;
    for (char c : content) {
        if (c == '\n') {
            lineCount++;
        }
    }
    EXPECT_EQ(lineCount, numThreads * logsPerThread * 2);
}

// 测试预日志模块的资源清理
TEST_F(PrelogIntegrationTest,ResourceCleanup)
{
    const char *testPath = "/tmp/silog_prelog.txt";

    // 多次初始化和反初始化
    for (int i = 0; i < 5; i++) {
        SilogPrelogConfig_t config;
        (void)strncpy(config.path, testPath, sizeof(config.path) - 1);
        config.path[sizeof(config.path) - 1] = '\0';
        config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
        config.enableStdout = false;

        EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);
        SILOG_PRELOG_I("SILOG_PRELOG_COMM", "Iteration %d", i);
        SilogPrelogDeinit();
    }

    // 验证最后一次的文件内容
    std::string content = ReadFileContent(testPath);
    EXPECT_NE(content.find("Iteration 4"), std::string::npos);
}

// 测试预日志模块的错误处理
TEST_F(PrelogIntegrationTest,ErrorHandling)
{
    // 测试未初始化时写入
    EXPECT_EQ(SilogPrelogWrite("SILOG_PRELOG_COMM", SILOG_PRELOG_LEVEL_INFO, "Test"), SILOG_TRANS_NOT_INIT);
    EXPECT_FALSE(SilogPrelogIsInitialized());

    // 测试空格式字符串和空模块名称
    SilogPrelogConfig_t config;
    (void)strncpy(config.path, "/tmp/silog_prelog.txt", sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config.enableStdout = false;
    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);
    EXPECT_EQ(SilogPrelogWrite("SILOG_PRELOG_COMM", SILOG_PRELOG_LEVEL_INFO, NULL), SILOG_NULL_PTR);
    EXPECT_EQ(SilogPrelogWrite(NULL, SILOG_PRELOG_LEVEL_INFO, "Test"), SILOG_NULL_PTR);

    SilogPrelogDeinit();
}

// 测试预日志文件权限问题（写入到无效路径）
TEST_F(PrelogIntegrationTest,InvalidPathHandling)
{
    // 尝试写入到不存在的目录
    SilogPrelogConfig_t config;
    (void)strncpy(config.path, "/nonexistent/path/test.log", sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config.enableStdout = false;  // 不输出到 stdout

    // 初始化应该成功（因为文件会在第一次写入时打开）
    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);

    // 写入应该失败，但不应该崩溃
    // 由于文件无法打开，enableStdout 会被自动启用
    EXPECT_EQ(SilogPrelogWrite("SILOG_PRELOG_COMM", SILOG_PRELOG_LEVEL_INFO, "Test message"), SILOG_OK);

    SilogPrelogDeinit();
}

// 测试预日志格式
TEST_F(PrelogIntegrationTest,LogFormat)
{
    const char *testPath = "/tmp/silog_prelog.txt";

    SilogPrelogConfig_t config;
    (void)strncpy(config.path, testPath, sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config.enableStdout = false;

    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);

    // 设置 errno 以测试错误字符串前缀
    errno = ENOENT;
    SILOG_PRELOG_E("SILOG_PRELOG_COMM", "File not found error");

    errno = 0;
    SILOG_PRELOG_I("SILOG_PRELOG_COMM", "Success message");

    SilogPrelogDeinit();

    // 验证文件格式
    std::string content = ReadFileContent(testPath);

    // 检查是否包含 errno 字符串
    EXPECT_NE(content.find("[ERROR]"), std::string::npos);
    EXPECT_NE(content.find("[INFO]"), std::string::npos);

    // 检查消息内容
    EXPECT_NE(content.find("File not found error"), std::string::npos);
    EXPECT_NE(content.find("Success message"), std::string::npos);
}
