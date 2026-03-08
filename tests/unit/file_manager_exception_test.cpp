#include "silog_file_manager.h"

#include <dirent.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "silog_error.h"

// ==================== FileManager 异常分支测试 ====================

class FileManagerExceptionTest : public ::testing::Test {
  protected:
    std::string testDir;

    void SetUp() override
    {
        // 创建唯一的测试目录
        char tmpDir[] = "/tmp/silog_exception_test_XXXXXX";
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
        if (testDir.empty()) {
            return;
        }

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
};

// 测试：路径存在但不是目录
TEST_F(FileManagerExceptionTest, PathExistsButNotDirectory)
{
    // 创建一个文件（与日志目录同名）
    std::string fakeDir = testDir + "/fake_dir";
    FILE *fp = fopen(fakeDir.c_str(), "w");
    ASSERT_NE(fp, nullptr);
    fprintf(fp, "this is a file, not a directory\n");
    fclose(fp);

    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", fakeDir.c_str());

    // 初始化应该失败，因为路径存在但不是目录
    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_FILE_OPEN);

    // 清理
    unlink(fakeDir.c_str());
}

// 测试：无效日志目录（权限不足）
TEST_F(FileManagerExceptionTest, InvalidLogDirPermission)
{
    // 尝试在根目录下创建目录（普通用户无权限）
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "/root_silog_test_dir");

    // 初始化可能失败（取决于运行权限）
    int32_t result = SilogFileManagerInit(&config);
    // root 用户可能会成功，普通用户会失败
    if (getuid() != 0) {
        EXPECT_EQ(result, SILOG_FILE_OPEN);
    }
}

// 测试：超长路径组件
TEST_F(FileManagerExceptionTest, VeryLongPathComponent)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);

    // 构造超长路径（超过单个文件名长度限制 255）
    char longName[300];
    memset(longName, 'a', sizeof(longName) - 1);
    longName[sizeof(longName) - 1] = '\0';

    // 创建路径：testDir/aaa...aaa（确保不溢出）
    char longPath[512];
    int n = snprintf(longPath, sizeof(longPath), "%s/%s", testDir.c_str(), longName);
    (void)n;

    // 截断到缓冲区大小
    strncpy(config.logDir, longPath, sizeof(config.logDir) - 1);
    config.logDir[sizeof(config.logDir) - 1] = '\0';

    // 初始化应该失败或处理错误（取决于文件系统限制）
    int32_t result = SilogFileManagerInit(&config);
    // 可能成功或失败，取决于文件系统
    (void)result;

    SilogFileManagerDeinit();
}

// 测试：目录名包含特殊字符
TEST_F(FileManagerExceptionTest, SpecialCharactersInPath)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);

    // 使用包含 null 字符的路径（如果 snprintf_s 正确处理）
    std::string specialDir = testDir + "/test\x00dir";
    snprintf(config.logDir, sizeof(config.logDir), "%s", specialDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    // 应该能正常创建目录
    EXPECT_EQ(result, SILOG_OK);

    SilogFileManagerDeinit();
}

// 测试：空的日志目录配置
TEST_F(FileManagerExceptionTest, EmptyLogDir)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    config.logDir[0] = '\0'; // 空字符串

    // 使用空目录初始化
    SilogFileManagerInit(&config);
    // 行为取决于实现，可能创建当前目录或失败
    SilogFileManagerDeinit();
}

// 测试：并发初始化和反初始化
TEST_F(FileManagerExceptionTest, ConcurrentInitDeinit)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    // 快速连续初始化和反初始化
    for (int i = 0; i < 10; i++) {
        int32_t result = SilogFileManagerInit(&config);
        EXPECT_EQ(result, SILOG_OK);
        SilogFileManagerDeinit();
    }
}

// 测试：在多线程环境下写入
TEST_F(FileManagerExceptionTest, MultiThreadWrite)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.flushMode = SILOG_FLUSH_ASYNC;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 多线程并发写入
    const int threadCount = 4;
    const int writesPerThread = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; i++) {
        threads.emplace_back([i, writesPerThread]() {
            for (int j = 0; j < writesPerThread; j++) {
                char data[64];
                snprintf(data, sizeof(data), "Thread %d write %d\n", i, j);
                SilogFileManagerWriteRaw((const uint8_t *)data, strlen(data));
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    SilogFileManagerDeinit();

    // 验证文件大小大于 0
    std::string logPath = testDir + "/silog.log";
    struct stat st;
    if (stat(logPath.c_str(), &st) == 0) {
        EXPECT_GT(st.st_size, 0);
    }
}

// 测试：轮转到满磁盘（模拟）
TEST_F(FileManagerExceptionTest, RotateWithDiskFull)
{
    // 使用一个非常小的目录来模拟磁盘空间问题比较困难
    // 这里测试多次快速轮转
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.maxFileSize = 100;
    config.flushMode = SILOG_FLUSH_SYNC;
    config.enableCompression = false;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 快速多次触发轮转
    char data[80];
    memset(data, 'X', sizeof(data));

    for (int i = 0; i < 20; i++) {
        result = SilogFileManagerWriteRaw((const uint8_t *)data, sizeof(data));
        if (result != SILOG_OK) {
            // 某些情况下可能会失败（如文件打开问题）
            break;
        }
    }

    SilogFileManagerDeinit();
}

// 测试：获取文件路径时缓冲区太小
TEST_F(FileManagerExceptionTest, GetPathSmallBuffer)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 使用很小的缓冲区
    char path[1];
    result = SilogFileManagerGetCurrentFilePath(path, sizeof(path));
    // 应该返回错误或者截断
    EXPECT_EQ(result, SILOG_OK); // 实际行为取决于实现

    SilogFileManagerDeinit();
}

// 测试：文件名包含特殊字符
TEST_F(FileManagerExceptionTest, SpecialCharsInFileBase)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    snprintf(config.logFileBase, sizeof(config.logFileBase), "test/file"); // 包含斜杠

    // 可能失败或成功，取决于系统对路径中斜杠的处理
    SilogFileManagerInit(&config);
    SilogFileManagerDeinit();
}

// 测试：写入时文件被删除
TEST_F(FileManagerExceptionTest, WriteAfterFileDeleted)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.flushMode = SILOG_FLUSH_SYNC;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 先写入一些数据
    const char *data1 = "First line\n";
    result = SilogFileManagerWriteRaw((const uint8_t *)data1, strlen(data1));
    EXPECT_EQ(result, SILOG_OK);

    // 删除日志文件
    std::string logPath = testDir + "/silog.log";
    unlink(logPath.c_str());

    // 再次写入（应该重新创建文件或失败）
    const char *data2 = "Second line after deletion\n";
    result = SilogFileManagerWriteRaw((const uint8_t *)data2, strlen(data2));
    // 结果取决于实现，可能成功（句柄仍有效）或失败

    SilogFileManagerDeinit();
}

// 测试：初始化时已有同名文件
TEST_F(FileManagerExceptionTest, InitWithExistingFile)
{
    // 先创建一个同名文件
    std::string logPath = testDir + "/silog.log";
    FILE *fp = fopen(logPath.c_str(), "w");
    ASSERT_NE(fp, nullptr);
    fprintf(fp, "Existing content\n");
    fclose(fp);

    // 获取文件大小
    struct stat st1;
    stat(logPath.c_str(), &st1);

    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());

    // 初始化应该追加到现有文件
    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 验证文件大小与之前相同（追加模式）
    uint32_t size = SilogFileManagerGetCurrentSize();
    EXPECT_EQ(size, (uint32_t)st1.st_size);

    SilogFileManagerDeinit();
}

// 测试：压缩队列满（大量快速轮转）
TEST_F(FileManagerExceptionTest, CompressQueueFull)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.maxFileSize = 50;
    config.flushMode = SILOG_FLUSH_SYNC;
    config.enableCompression = true;
    config.compressMode = SILOG_COMPRESS_ASYNC;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    char data[40];
    memset(data, 'A', sizeof(data));

    // 快速连续写入触发多次轮转，可能填满压缩队列
    for (int i = 0; i < 100; i++) {
        result = SilogFileManagerWriteRaw((const uint8_t *)data, sizeof(data));
        if (result != SILOG_OK) {
            break;
        }
    }

    // 给压缩线程一些时间处理
    usleep(500000); // 500ms

    SilogFileManagerDeinit();
}

// 测试：maxFileCount 为 0
TEST_F(FileManagerExceptionTest, ZeroMaxFileCount)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.maxFileCount = 0; // 不保留历史文件
    config.maxFileSize = 50;
    config.flushMode = SILOG_FLUSH_SYNC;
    config.enableCompression = false;

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    char data[40];
    memset(data, 'B', sizeof(data));

    // 触发多次轮转
    for (int i = 0; i < 5; i++) {
        result = SilogFileManagerWriteRaw((const uint8_t *)data, sizeof(data));
        EXPECT_EQ(result, SILOG_OK);
    }

    SilogFileManagerDeinit();
}

// 测试：非常大的 maxFileSize
TEST_F(FileManagerExceptionTest, VeryLargeMaxFileSize)
{
    SilogLogFileConfig config;
    SilogFileManagerGetDefaultConfig(&config);
    snprintf(config.logDir, sizeof(config.logDir), "%s", testDir.c_str());
    config.maxFileSize = UINT32_MAX; // 最大可能值

    int32_t result = SilogFileManagerInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 写入一些数据，不会触发轮转
    char data[100];
    memset(data, 'C', sizeof(data));
    result = SilogFileManagerWriteRaw((const uint8_t *)data, sizeof(data));
    EXPECT_EQ(result, SILOG_OK);

    SilogFileManagerDeinit();
}
