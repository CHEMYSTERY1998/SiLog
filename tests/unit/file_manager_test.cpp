#include "silog_file_manager.h"

#include <dirent.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "silog_error.h"

// ==================== FileManager 测试 ====================

class FileManagerTest : public ::testing::Test {
  protected:
    std::string testDir;

    void SetUp() override
    {
        // 创建唯一的测试目录
        char tmpDir[] = "/tmp/silog_test_XXXXXX";
        testDir = mkdtemp(tmpDir);
        ASSERT_FALSE(testDir.empty());

        // 确保反初始化
        SilogFileManagerDeinit();
    }

    void TearDown() override
    {
        SilogFileManagerDeinit();
        CleanupTestDir();
    }

    void CleanupTestDir()
    {
        // 清理测试目录中的所有文件
        DIR *dir = opendir(testDir.c_str());
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

    int CountFilesInDir()
    {
        int count = 0;
        DIR *dir = opendir(testDir.c_str());
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                count++;
            }
            closedir(dir);
        }
        return count;
    }
};

// 测试：获取默认配置
TEST_F(FileManagerTest, GetDefaultConfig)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);

    EXPECT_STREQ(config.logDir, "/tmp/silog");
    EXPECT_STREQ(config.logFileBase, "silog");
    EXPECT_EQ(config.maxFileSize, 10 * 1024 * 1024); // 10MB
    EXPECT_EQ(config.maxFileCount, 10);
    EXPECT_TRUE(config.enableCompression);
    EXPECT_EQ(config.compressMode, SILOG_COMPRESS_ASYNC);
    EXPECT_EQ(config.flushMode, SILOG_FLUSH_ASYNC);
    EXPECT_EQ(config.asyncFlushIntervalMs, 1000);
    EXPECT_EQ(config.asyncFlushSize, 4 * 1024);
    EXPECT_EQ(config.rotateRetryCount, 3);
    EXPECT_EQ(config.rotateRetryDelayMs, 100);
}

// 测试：获取默认配置时传入 NULL
TEST_F(FileManagerTest, GetDefaultConfigNull)
{
    // 应该不会崩溃
    SilogFileManagerGetDefaultConfig(NULL);
}

// 测试：基本初始化和反初始化
TEST_F(FileManagerTest, InitDeinit)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);

    // 修改日志目录为测试目录
    int ret = snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    ASSERT_GT(ret, 0);

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    SilogFileManagerDeinit();
}

// 测试：使用 NULL 配置初始化（使用默认配置）
TEST_F(FileManagerTest, InitWithNullConfig)
{
    // 先设置日志目录
    int32_t result = SilogFileManagerSetLogDir(testDir.c_str());
    EXPECT_EQ(result, SILOG_OK);

    result = SilogFileManagerInit(NULL);
    EXPECT_EQ(result, SILOG_OK);

    SilogFileManagerDeinit();
}

// 测试：重复初始化
TEST_F(FileManagerTest, DoubleInit)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 第二次初始化应该返回 OK（幂等）
    result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    SilogFileManagerDeinit();
}

// 测试：重复反初始化
TEST_F(FileManagerTest, DoubleDeinit)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    SilogFileManagerInit(&config);
    SilogFileManagerDeinit();

    // 第二次反初始化应该安全
    SilogFileManagerDeinit();
}

// 测试：未初始化时的操作
TEST_F(FileManagerTest, OperationsWithoutInit)
{
    // 获取当前大小应该返回 0
    EXPECT_EQ(SilogFileManagerGetCurrentSize(), 0);

    // 写入应该失败
    const char *data = "test data";
    int32_t result = SilogFileManagerWriteRaw((const uint8_t *)data, strlen(data));
    EXPECT_EQ(result, SILOG_FILE_MANAGER_NOT_INIT);

    // 手动轮转应该失败
    result = SilogFileManagerRotate();
    EXPECT_EQ(result, SILOG_FILE_MANAGER_NOT_INIT);
}

// ==================== 配置接口测试 ====================

TEST_F(FileManagerTest, SetLogDir)
{
    // 初始化前设置日志目录
    int32_t result = SilogFileManagerSetLogDir(testDir.c_str());
    EXPECT_EQ(result, SILOG_OK);

    // 使用自定义配置初始化（只设置目录，其他默认）
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    // 使用 strncpy 确保复制完整路径
    strncpy(config.logDir, testDir.c_str(), sizeof(config.logDir) - 1);
    config.logDir[sizeof(config.logDir) - 1] = '\0';

    result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 验证当前文件路径包含测试目录
    char path[256];
    result = SilogFileManagerGetCurrentFilePath(path, sizeof(path));
    EXPECT_EQ(result, SILOG_OK);
    EXPECT_NE(strstr(path, testDir.c_str()), nullptr);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetLogDirNull)
{
    int32_t result = SilogFileManagerSetLogDir(NULL);
    EXPECT_EQ(result, SILOG_INVALID_ARG);
}

TEST_F(FileManagerTest, SetLogDirAfterInit)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 初始化后设置目录应该失败
    std::string newDir = testDir + "_new";
    result = SilogFileManagerSetLogDir(newDir.c_str());
    EXPECT_EQ(result, SILOG_BUSY);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetLogFileBase)
{
    // 使用自定义配置初始化
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    strncpy(config.logDir, testDir.c_str(), sizeof(config.logDir) - 1);
    config.logDir[sizeof(config.logDir) - 1] = '\0';
    strncpy(config.logFileBase, "mylog", sizeof(config.logFileBase) - 1);
    config.logFileBase[sizeof(config.logFileBase) - 1] = '\0';

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 验证当前文件路径包含自定义基础名
    char path[256];
    result = SilogFileManagerGetCurrentFilePath(path, sizeof(path));
    EXPECT_EQ(result, SILOG_OK);
    EXPECT_NE(strstr(path, "mylog"), nullptr);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetLogFileBaseNull)
{
    int32_t result = SilogFileManagerSetLogFileBase(NULL);
    EXPECT_EQ(result, SILOG_INVALID_ARG);
}

TEST_F(FileManagerTest, SetMaxFileSize)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 设置新的最大文件大小
    SilogFileManagerSetMaxFileSize(1024); // 1KB

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetMaxFileCount)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 设置新的最大文件数量
    SilogFileManagerSetMaxFileCount(5);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetCompression)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 关闭压缩
    SilogFileManagerSetCompression(false);

    // 开启压缩
    SilogFileManagerSetCompression(true);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetCompressMode)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 设置同步压缩
    SilogFileManagerSetCompressMode(SILOG_COMPRESS_SYNC);

    // 设置异步压缩
    SilogFileManagerSetCompressMode(SILOG_COMPRESS_ASYNC);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetFlushMode)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 设置同步刷盘
    SilogFileManagerSetFlushMode(SILOG_FLUSH_SYNC);

    // 设置异步刷盘
    SilogFileManagerSetFlushMode(SILOG_FLUSH_ASYNC);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetAsyncFlushInterval)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 设置异步刷盘间隔
    SilogFileManagerSetAsyncFlushInterval(500); // 500ms

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, SetAsyncFlushSize)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 设置异步刷盘大小阈值
    SilogFileManagerSetAsyncFlushSize(2048); // 2KB

    SilogFileManagerDeinit();
}

// ==================== 写入和查询测试 ====================

TEST_F(FileManagerTest, WriteRaw)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.flushMode = SILOG_FLUSH_SYNC; // 同步刷盘以便立即验证

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入数据
    const char *data = "Hello, World!\n";
    result = SilogFileManagerWriteRaw((const uint8_t *)data, strlen(data));
    EXPECT_EQ(result, SILOG_OK);

    // 验证文件大小
    EXPECT_EQ(SilogFileManagerGetCurrentSize(), strlen(data));

    SilogFileManagerDeinit();

    // 验证文件内容
    char path[256];
    snprintf(path, sizeof(path), "%s/silog.log", testDir.c_str());
    FILE *fp = fopen(path, "r");
    ASSERT_NE(fp, nullptr);

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf), fp);
    buf[n] = '\0';
    EXPECT_STREQ(buf, data);

    fclose(fp);
}

TEST_F(FileManagerTest, WriteRawNullData)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入 NULL 数据应该失败
    result = SilogFileManagerWriteRaw(NULL, 10);
    EXPECT_EQ(result, SILOG_INVALID_ARG);

    // 写入空数据应该失败
    result = SilogFileManagerWriteRaw((const uint8_t *)"", 0);
    EXPECT_EQ(result, SILOG_INVALID_ARG);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, WriteMultiple)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.flushMode = SILOG_FLUSH_SYNC;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入多行数据
    const char *lines[] = {"Line 1\n", "Line 2\n", "Line 3\n"};
    size_t totalLen = 0;
    for (int i = 0; i < 3; i++) {
        result = SilogFileManagerWriteRaw((const uint8_t *)lines[i], strlen(lines[i]));
        EXPECT_EQ(result, SILOG_OK);
        totalLen += strlen(lines[i]);
    }

    EXPECT_EQ(SilogFileManagerGetCurrentSize(), totalLen);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, GetCurrentFilePath)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    char path[256];
    result = SilogFileManagerGetCurrentFilePath(path, sizeof(path));
    EXPECT_EQ(result, SILOG_OK);
    EXPECT_NE(strstr(path, ".log"), nullptr);

    SilogFileManagerDeinit();
}

TEST_F(FileManagerTest, GetCurrentFilePathNull)
{
    // 传入 NULL 应该失败
    int32_t result = SilogFileManagerGetCurrentFilePath(NULL, 256);
    EXPECT_EQ(result, SILOG_INVALID_ARG);

    result = SilogFileManagerGetCurrentFilePath((char *)1, 0);
    EXPECT_EQ(result, SILOG_INVALID_ARG);
}

TEST_F(FileManagerTest, Flush)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.flushMode = SILOG_FLUSH_ASYNC;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入数据（异步模式下不会立即刷盘）
    const char *data = "Async data\n";
    result = SilogFileManagerWriteRaw((const uint8_t *)data, strlen(data));
    EXPECT_EQ(result, SILOG_OK);

    // 手动刷盘
    SilogFileManagerFlush();

    SilogFileManagerDeinit();

    // 验证文件内容
    char path[256];
    snprintf(path, sizeof(path), "%s/silog.log", testDir.c_str());
    FILE *fp = fopen(path, "r");
    ASSERT_NE(fp, nullptr);

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf), fp);
    buf[n] = '\0';
    EXPECT_STREQ(buf, data);

    fclose(fp);
}

// ==================== 轮转测试 ====================

TEST_F(FileManagerTest, ManualRotate)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.flushMode = SILOG_FLUSH_SYNC;
    config.enableCompression = false; // 关闭压缩以便检查文件

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入一些数据
    const char *data = "Before rotate\n";
    result = SilogFileManagerWriteRaw((const uint8_t *)data, strlen(data));
    EXPECT_EQ(result, SILOG_OK);

    // 手动轮转
    result = SilogFileManagerRotate();
    EXPECT_EQ(result, SILOG_OK);

    // 验证文件大小重置
    EXPECT_EQ(SilogFileManagerGetCurrentSize(), 0);

    // 写入新数据
    const char *data2 = "After rotate\n";
    result = SilogFileManagerWriteRaw((const uint8_t *)data2, strlen(data2));
    EXPECT_EQ(result, SILOG_OK);

    SilogFileManagerDeinit();

    // 验证有两个文件（轮转生成的历史文件 + 新文件）
    int fileCount = CountFilesInDir();
    EXPECT_EQ(fileCount, 2);
}

TEST_F(FileManagerTest, AutoRotate)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.maxFileSize = 100; // 100 bytes 触发轮转
    config.flushMode = SILOG_FLUSH_SYNC;
    config.enableCompression = false;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入超过 100 字节的数据触发自动轮转
    char data[60];
    memset(data, 'A', sizeof(data));

    result = SilogFileManagerWriteRaw((const uint8_t *)data, sizeof(data));
    EXPECT_EQ(result, SILOG_OK);

    // 第一次写入后大小为 60，未触发轮转
    EXPECT_EQ(SilogFileManagerGetCurrentSize(), 60);

    // 第二次写入会触发轮转（60 + 60 >= 100）
    result = SilogFileManagerWriteRaw((const uint8_t *)data, sizeof(data));
    EXPECT_EQ(result, SILOG_OK);

    // 新文件大小为 60
    EXPECT_EQ(SilogFileManagerGetCurrentSize(), 60);

    SilogFileManagerDeinit();

    // 验证有历史文件和新文件
    int fileCount = CountFilesInDir();
    EXPECT_GE(fileCount, 2);
}

TEST_F(FileManagerTest, RotateWithoutInit)
{
    int32_t result = SilogFileManagerRotate();
    EXPECT_EQ(result, SILOG_FILE_MANAGER_NOT_INIT);
}

// ==================== 文件清理测试 ====================

TEST_F(FileManagerTest, FileCleanup)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.maxFileSize = 50; // 小文件大小，容易触发轮转
    config.maxFileCount = 3; // 最多保留 3 个历史文件
    config.flushMode = SILOG_FLUSH_SYNC;
    config.enableCompression = false;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入多轮数据，生成超过 maxFileCount 个历史文件
    char data[50];
    memset(data, 'X', sizeof(data));

    for (int i = 0; i < 6; i++) {
        result = SilogFileManagerWriteRaw((const uint8_t *)data, sizeof(data));
        EXPECT_EQ(result, SILOG_OK);
        // 每次写入触发轮转
        result = SilogFileManagerRotate();
        EXPECT_EQ(result, SILOG_OK);
    }

    SilogFileManagerDeinit();

    // 验证历史文件数量不超过 maxFileCount + 1（当前文件）
    int fileCount = CountFilesInDir();
    EXPECT_LE(fileCount, config.maxFileCount + 1);
}

// ==================== 同步刷盘模式测试 ====================

TEST_F(FileManagerTest, SyncFlushMode)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.flushMode = SILOG_FLUSH_SYNC;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 同步模式下每次写入都会刷盘
    const char *data = "Sync test\n";
    result = SilogFileManagerWriteRaw((const uint8_t *)data, strlen(data));
    EXPECT_EQ(result, SILOG_OK);

    // 文件应该已经刷盘，可以直接读取
    char path[256];
    snprintf(path, sizeof(path), "%s/silog.log", testDir.c_str());
    FILE *fp = fopen(path, "r");
    ASSERT_NE(fp, nullptr);

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf), fp);
    EXPECT_EQ(n, strlen(data));

    fclose(fp);

    SilogFileManagerDeinit();
}

// ==================== 异步刷盘模式测试 ====================

TEST_F(FileManagerTest, AsyncFlushMode)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.flushMode = SILOG_FLUSH_ASYNC;
    config.asyncFlushSize = 100;
    config.asyncFlushIntervalMs = 5000; // 长间隔，测试大小触发

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入小于 asyncFlushSize 的数据
    char data[50];
    memset(data, 'A', sizeof(data));
    result = SilogFileManagerWriteRaw((const uint8_t *)data, sizeof(data));
    EXPECT_EQ(result, SILOG_OK);

    // 未达到刷盘条件，数据在缓冲区
    EXPECT_EQ(SilogFileManagerGetCurrentSize(), 50);

    SilogFileManagerDeinit();
}
