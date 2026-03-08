#include "silog_daemon.h"

#include <dirent.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "silog_error.h"
#include "silog_file_manager.h"
#include "silog_remote.h"

// ==================== Daemon 异常分支测试 ====================

class DaemonExceptionTest : public ::testing::Test {
  protected:
    std::string testDir;

    void SetUp() override
    {
        char tmpDir[] = "/tmp/silog_daemon_exc_test_XXXXXX";
        testDir = mkdtemp(tmpDir);

        // 完全清理
        SilogDaemonDeinit();
        SilogDaemonRemoteDeinit();
        usleep(20000);
    }

    void TearDown() override
    {
        SilogDaemonRemoteDeinit();
        SilogDaemonDeinit();
        usleep(50000);
        CleanupTestDir();
    }

    void CleanupTestDir()
    {
        if (testDir.empty()) {
            return;
        }

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

// 测试：快速连续初始化和反初始化
TEST_F(DaemonExceptionTest, RapidInitDeinit)
{
    for (int i = 0; i < 5; i++) {
        int32_t result = SilogDaemonInit();
        EXPECT_EQ(result, SILOG_OK);

        usleep(10000); // 10ms

        SilogDaemonDeinit();
    }
}

// 测试：远程服务初始化在守护进程之后
TEST_F(DaemonExceptionTest, RemoteInitAfterDaemon)
{
    // 先初始化守护进程
    int32_t result = SilogDaemonInit();
    EXPECT_EQ(result, SILOG_OK);

    // 尝试再次初始化远程服务（应该幂等）
    SilogDaemonRemoteConfig remoteConfig = {
        .listenPort = 19100,
        .maxClients = 5,
        .enable = true,
    };

    result = SilogDaemonRemoteInit(&remoteConfig);
    // 可能返回 OK（已初始化）或错误

    SilogDaemonDeinit();
}

// 测试：多线程并发访问远程服务状态
TEST_F(DaemonExceptionTest, ConcurrentRemoteAccess)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19101,
        .maxClients = 5,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 多线程并发检查状态
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([]() {
            for (int j = 0; j < 100; j++) {
                (void)SilogDaemonRemoteIsEnabled();
                (void)SilogDaemonRemoteGetClientCount();
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    SilogDaemonRemoteDeinit();
}

// 测试：远程服务在另一个端口上重新初始化
TEST_F(DaemonExceptionTest, RemoteReinitDifferentPort)
{
    SilogDaemonRemoteConfig config1 = {
        .listenPort = 19102,
        .maxClients = 5,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config1);
    EXPECT_EQ(result, SILOG_OK);

    // 尝试在不同端口重新初始化（应该幂等，不重初始化）
    SilogDaemonRemoteConfig config2 = {
        .listenPort = 19103,
        .maxClients = 5,
        .enable = true,
    };

    result = SilogDaemonRemoteInit(&config2);
    EXPECT_EQ(result, SILOG_OK);

    // 应该仍在第一个端口上运行
    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    SilogDaemonRemoteDeinit();
}

// 测试：守护进程初始化时文件管理器失败
TEST_F(DaemonExceptionTest, DaemonInitWithInvalidLogDir)
{
    // 设置一个无效的文件管理器目录
    SilogFileManagerSetLogDir("/proc/invalid_dir_test");

    // 守护进程初始化可能失败
    int32_t result = SilogDaemonInit();
    // 结果取决于文件管理器对无效目录的处理

    if (result == SILOG_OK) {
        SilogDaemonDeinit();
    }
}

// 测试：远程服务使用已被占用的端口
TEST_F(DaemonExceptionTest, RemoteServicePortConflict)
{
    // 第一次初始化
    SilogDaemonRemoteConfig config1 = {
        .listenPort = 19104,
        .maxClients = 5,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config1);
    EXPECT_EQ(result, SILOG_OK);

    // 反初始化
    SilogDaemonRemoteDeinit();
    usleep(50000); // 等待端口释放

    // 再次初始化同一端口
    result = SilogDaemonRemoteInit(&config1);
    EXPECT_EQ(result, SILOG_OK);

    SilogDaemonRemoteDeinit();
}

// 测试：连续快速切换远程服务状态
TEST_F(DaemonExceptionTest, RapidRemoteToggle)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19105,
        .maxClients = 5,
        .enable = true,
    };

    for (int i = 0; i < 5; i++) {
        int32_t result = SilogDaemonRemoteInit(&config);
        EXPECT_EQ(result, SILOG_OK);
        EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

        SilogDaemonRemoteDeinit();
        EXPECT_FALSE(SilogDaemonRemoteIsEnabled());

        usleep(10000);
    }
}

// 测试：客户端计数在边界情况
TEST_F(DaemonExceptionTest, ClientCountBoundary)
{
    // 未初始化时客户端计数应为 0
    EXPECT_EQ(SilogDaemonRemoteGetClientCount(), 0);

    SilogDaemonRemoteConfig config = {
        .listenPort = 19106,
        .maxClients = 0, // 使用默认值
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 初始化后但无连接时应为 0
    EXPECT_EQ(SilogDaemonRemoteGetClientCount(), 0);

    SilogDaemonRemoteDeinit();

    // 反初始化后应为 0
    EXPECT_EQ(SilogDaemonRemoteGetClientCount(), 0);
}

// 测试：最大客户端数边界
TEST_F(DaemonExceptionTest, MaxClientsBoundary)
{
    // 测试 maxClients = 1
    SilogDaemonRemoteConfig config = {
        .listenPort = 19107,
        .maxClients = 1,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    SilogDaemonRemoteDeinit();

    // 测试非常大的 maxClients
    config.maxClients = 1000;
    config.listenPort = 19108;

    result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    SilogDaemonRemoteDeinit();
}

// ==================== Daemon 压力测试 ====================

class DaemonStressTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        SilogDaemonDeinit();
        SilogDaemonRemoteDeinit();
        usleep(20000);
    }

    void TearDown() override
    {
        SilogDaemonRemoteDeinit();
        SilogDaemonDeinit();
        usleep(50000);
    }
};

// 压力测试：大量远程服务初始化和反初始化
TEST_F(DaemonStressTest, RemoteServiceStress)
{
    const int iterations = 10;

    for (int i = 0; i < iterations; i++) {
        SilogDaemonRemoteConfig config = {
            .listenPort = static_cast<uint16_t>(19200 + (i % 10)),
            .maxClients = 5,
            .enable = true,
        };

        int32_t result = SilogDaemonRemoteInit(&config);
        if (result == SILOG_OK) {
            EXPECT_TRUE(SilogDaemonRemoteIsEnabled());
            SilogDaemonRemoteDeinit();
        }

        usleep(10000); // 短暂休息
    }
}

// 压力测试：并发检查远程状态
TEST_F(DaemonStressTest, ConcurrentRemoteStatusCheck)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19210,
        .maxClients = 10,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    ASSERT_EQ(result, SILOG_OK);

    const int threadCount = 20;
    const int checksPerThread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; i++) {
        threads.emplace_back([checksPerThread]() {
            for (int j = 0; j < checksPerThread; j++) {
                volatile bool enabled = SilogDaemonRemoteIsEnabled();
                (void)enabled;
                volatile uint32_t count = SilogDaemonRemoteGetClientCount();
                (void)count;
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    SilogDaemonRemoteDeinit();
}

// 压力测试：守护进程和远程服务交替初始化
TEST_F(DaemonStressTest, AlternatingInit)
{
    for (int i = 0; i < 3; i++) {
        // 初始化守护进程
        int32_t result = SilogDaemonInit();
        EXPECT_EQ(result, SILOG_OK);
        usleep(20000);

        // 反初始化
        SilogDaemonDeinit();
        usleep(20000);

        // 单独初始化远程服务
        SilogDaemonRemoteConfig config = {
            .listenPort = static_cast<uint16_t>(19220 + i),
            .maxClients = 5,
            .enable = true,
        };

        result = SilogDaemonRemoteInit(&config);
        EXPECT_EQ(result, SILOG_OK);
        usleep(10000);

        SilogDaemonRemoteDeinit();
        usleep(20000);
    }
}
