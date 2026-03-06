/**
 * @file prelog_test.cpp
 * @brief SiLog 预日志模块单元测试
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

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

class PrelogTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 确保每次测试前清理状态
        SilogPrelogDeinit();
        DeleteFile("/tmp/test_prelog.txt");
        DeleteFile("/tmp/silog_prelog.txt");
    }

    void TearDown() override
    {
        SilogPrelogDeinit();
        DeleteFile("/tmp/test_prelog.txt");
        DeleteFile("/tmp/silog_prelog.txt");
    }
};

// 测试初始化和反初始化
TEST_F(PrelogTest, InitDeinit)
{
    // 测试默认配置初始化
    EXPECT_EQ(SilogPrelogInit(nullptr), SILOG_OK);
    EXPECT_TRUE(SilogPrelogIsInitialized());

    // 测试重复初始化（应该成功，不报错）
    EXPECT_EQ(SilogPrelogInit(nullptr), SILOG_OK);

    SilogPrelogDeinit();
    EXPECT_FALSE(SilogPrelogIsInitialized());

    // 测试自定义配置初始化
    SilogPrelogConfig_t config;
    (void)strncpy(config.path, "/tmp/test_prelog.txt", sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_INFO;
    config.enableStdout = false;
    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);
    EXPECT_TRUE(SilogPrelogIsInitialized());

    SilogPrelogDeinit();
    EXPECT_FALSE(SilogPrelogIsInitialized());
}

// 测试写入和读取
TEST_F(PrelogTest,WriteAndRead)
{
    const char *testPath = "/tmp/test_prelog.txt";

    SilogPrelogConfig_t config;
    (void)strncpy(config.path, testPath, sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config.enableStdout = false;

    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);

    // 写入不同级别的日志
    EXPECT_EQ(SilogPrelogWrite(SILOG_PRELOG_COMM, SILOG_PRELOG_LEVEL_DEBUG, "Debug message: %d", 42), SILOG_OK);
    EXPECT_EQ(SilogPrelogWrite(SILOG_PRELOG_COMM, SILOG_PRELOG_LEVEL_INFO, "Info message: %s", "test"), SILOG_OK);
    EXPECT_EQ(SilogPrelogWrite(SILOG_PRELOG_COMM, SILOG_PRELOG_LEVEL_WARNING, "Warning message"), SILOG_OK);
    EXPECT_EQ(SilogPrelogWrite(SILOG_PRELOG_COMM, SILOG_PRELOG_LEVEL_ERROR, "Error message"), SILOG_OK);

    // 使用宏写入
    SILOG_PRELOG_D(SILOG_PRELOG_COMM, "Macro debug: %d", 100);
    SILOG_PRELOG_I(SILOG_PRELOG_COMM, "Macro info");
    SILOG_PRELOG_W(SILOG_PRELOG_COMM, "Macro warning");
    SILOG_PRELOG_E(SILOG_PRELOG_COMM, "Macro error");

    SilogPrelogDeinit();

    // 读取并验证文件内容
    std::string content = ReadFileContent(testPath);
    EXPECT_NE(content.find("DEBUG"), std::string::npos);
    EXPECT_NE(content.find("INFO"), std::string::npos);
    EXPECT_NE(content.find("WARNING"), std::string::npos);
    EXPECT_NE(content.find("ERROR"), std::string::npos);
    EXPECT_NE(content.find("Debug message: 42"), std::string::npos);
    EXPECT_NE(content.find("Info message: test"), std::string::npos);
}

// 测试级别过滤
TEST_F(PrelogTest,LevelFiltering)
{
    const char *testPath = "/tmp/test_prelog.txt";

    // 设置最小级别为 WARNING
    SilogPrelogConfig_t config;
    (void)strncpy(config.path, testPath, sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_WARNING;
    config.enableStdout = false;

    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);

    // 写入各级别日志
    SILOG_PRELOG_D("SILOG_PRELOG_COMM", "This debug should be filtered");
    SILOG_PRELOG_I("SILOG_PRELOG_COMM", "This info should be filtered");
    SILOG_PRELOG_W("SILOG_PRELOG_COMM", "This warning should appear");
    SILOG_PRELOG_E("SILOG_PRELOG_COMM", "This error should appear");

    SilogPrelogDeinit();

    // 读取并验证文件内容
    std::string content = ReadFileContent(testPath);

    // DEBUG 和 INFO 应该被过滤
    EXPECT_EQ(content.find("This debug should be filtered"), std::string::npos);
    EXPECT_EQ(content.find("This info should be filtered"), std::string::npos);

    // WARNING 和 ERROR 应该出现
    EXPECT_NE(content.find("This warning should appear"), std::string::npos);
    EXPECT_NE(content.find("This error should appear"), std::string::npos);
}

// 测试多线程安全
TEST_F(PrelogTest,ThreadSafety)
{
    const char *testPath = "/tmp/test_prelog.txt";

    SilogPrelogConfig_t config;
    (void)strncpy(config.path, testPath, sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config.enableStdout = false;

    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);

    const int numThreads = 10;
    const int logsPerThread = 100;
    std::vector<std::thread> threads;

    // 创建多个线程同时写入日志
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([i, logsPerThread]() {
            for (int j = 0; j < logsPerThread; j++) {
                SILOG_PRELOG_I("SILOG_PRELOG_COMM", "Thread %d Log %d", i, j);
            }
        });
    }

    // 等待所有线程完成
    for (auto &t : threads) {
        t.join();
    }

    SilogPrelogDeinit();

    // 读取文件并验证行数
    std::string content = ReadFileContent(testPath);
    int lineCount = 0;
    for (char c : content) {
        if (c == '\n') {
            lineCount++;
        }
    }

    // 应该写入 numThreads * logsPerThread 行日志
    EXPECT_EQ(lineCount, numThreads * logsPerThread);
}

// 测试空指针参数
TEST_F(PrelogTest,NullPointerHandling)
{
    EXPECT_EQ(SilogPrelogInit(nullptr), SILOG_OK);

    // 测试空模块名称
    EXPECT_EQ(SilogPrelogWrite(NULL, SILOG_PRELOG_LEVEL_INFO, "Test"), SILOG_NULL_PTR);

    // 测试空格式字符串 - 在 C++ 中使用 NULL 代替 nullptr
    EXPECT_EQ(SilogPrelogWrite("SILOG_PRELOG_COMM", SILOG_PRELOG_LEVEL_INFO, NULL), SILOG_NULL_PTR);

    SilogPrelogDeinit();
}

// 测试获取文件指针
TEST_F(PrelogTest,GetFilePointer)
{
    // 未初始化时应该返回 nullptr
    EXPECT_EQ(SilogPrelogGetFile(), nullptr);

    SilogPrelogConfig_t config;
    (void)strncpy(config.path, "/tmp/test_prelog.txt", sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config.enableStdout = false;

    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);

    // 初始化后应该返回有效的文件指针
    FILE *file = SilogPrelogGetFile();
    EXPECT_NE(file, nullptr);

    SilogPrelogDeinit();

    // 反初始化后应该返回 nullptr
    EXPECT_EQ(SilogPrelogGetFile(), nullptr);
}

// 测试未初始化时写入
TEST_F(PrelogTest,WriteWithoutInit)
{
    // 未初始化时写入应该返回错误
    EXPECT_EQ(SilogPrelogWrite("SILOG_PRELOG_COMM", SILOG_PRELOG_LEVEL_INFO, "Test message"), SILOG_TRANS_NOT_INIT);
}

// 测试长消息
TEST_F(PrelogTest,LongMessage)
{
    const char *testPath = "/tmp/test_prelog.txt";

    SilogPrelogConfig_t config;
    (void)strncpy(config.path, testPath, sizeof(config.path) - 1);
    config.path[sizeof(config.path) - 1] = '\0';
    config.minLevel = SILOG_PRELOG_LEVEL_DEBUG;
    config.enableStdout = false;

    EXPECT_EQ(SilogPrelogInit(&config), SILOG_OK);

    // 创建一条较长的消息
    char longMsg[512];
    memset(longMsg, 'A', sizeof(longMsg) - 1);
    longMsg[sizeof(longMsg) - 1] = '\0';

    EXPECT_EQ(SilogPrelogWrite("SILOG_PRELOG_COMM", SILOG_PRELOG_LEVEL_INFO, "%s", longMsg), SILOG_OK);

    SilogPrelogDeinit();

    // 验证文件内容
    std::string content = ReadFileContent(testPath);
    EXPECT_NE(content.find("AAAAAAAA"), std::string::npos);
}
