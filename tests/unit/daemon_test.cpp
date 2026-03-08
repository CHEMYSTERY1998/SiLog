#include "silog_daemon.h"

#include <dirent.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "silog_error.h"
#include "silog_file_manager.h"
#include "silog_remote.h"

// ==================== Daemon 基础测试 ====================

class DaemonTest : public ::testing::Test {
  protected:
    std::string testDir;

    void SetUp() override
    {
        // 创建唯一的测试目录
        char tmpDir[] = "/tmp/silog_daemon_test_XXXXXX";
        testDir = mkdtemp(tmpDir);

        // 确保完全清理
        SilogDaemonDeinit();
    }

    void TearDown() override
    {
        SilogDaemonDeinit();
        CleanupTestDir();

        // 等待资源释放
        usleep(50000);
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

// 测试：基本初始化和反初始化
TEST_F(DaemonTest, InitDeinit)
{
    int32_t result = SilogDaemonInit();
    EXPECT_EQ(result, SILOG_OK);

    SilogDaemonDeinit();
}

// 测试：重复反初始化
TEST_F(DaemonTest, DoubleDeinit)
{
    int32_t result = SilogDaemonInit();
    EXPECT_EQ(result, SILOG_OK);

    SilogDaemonDeinit();

    // 第二次反初始化应该安全
    SilogDaemonDeinit();
}

// 测试：重复初始化
TEST_F(DaemonTest, DoubleInit)
{
    int32_t result = SilogDaemonInit();
    EXPECT_EQ(result, SILOG_OK);

    // 第二次初始化应该返回 OK（幂等）
    result = SilogDaemonInit();
    EXPECT_EQ(result, SILOG_OK);

    SilogDaemonDeinit();
}

// ==================== Daemon 远程服务测试 ====================

class DaemonRemoteTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        // 完全清理
        SilogDaemonDeinit();
        SilogDaemonRemoteDeinit();
        SilogRemoteDeinit();
        usleep(20000);
    }

    void TearDown() override
    {
        SilogDaemonRemoteDeinit();
        SilogDaemonDeinit();
        SilogRemoteDeinit();
        usleep(50000);
    }
};

// 测试：远程服务默认状态
TEST_F(DaemonRemoteTest, RemoteServiceDefaultState)
{
    // 默认远程服务应该未启用
    EXPECT_FALSE(SilogDaemonRemoteIsEnabled());
    EXPECT_EQ(SilogDaemonRemoteGetClientCount(), 0);
}

// 测试：远程服务初始化
TEST_F(DaemonRemoteTest, RemoteServiceInit)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19090, // 非标准端口
        .maxClients = 5,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());
    EXPECT_EQ(SilogDaemonRemoteGetClientCount(), 0);

    SilogDaemonRemoteDeinit();
}

// 测试：使用 NULL 配置初始化远程服务
TEST_F(DaemonRemoteTest, RemoteServiceInitWithNullConfig)
{
    // NULL 配置会使用默认端口和默认最大客户端数
    int32_t result = SilogDaemonRemoteInit(NULL);
    EXPECT_EQ(result, SILOG_OK);
    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    SilogDaemonRemoteDeinit();
}

// 测试：重复初始化远程服务
TEST_F(DaemonRemoteTest, RemoteServiceDoubleInit)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19091,
        .maxClients = 5,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    // 第二次初始化应该返回 OK（幂等）
    result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);

    SilogDaemonRemoteDeinit();
}

// 测试：远程服务反初始化后状态
TEST_F(DaemonRemoteTest, RemoteServiceDeinitState)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19092,
        .maxClients = 5,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);
    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    SilogDaemonRemoteDeinit();

    // 反初始化后应该未启用
    EXPECT_FALSE(SilogDaemonRemoteIsEnabled());
    EXPECT_EQ(SilogDaemonRemoteGetClientCount(), 0);
}

// 测试：重复反初始化远程服务
TEST_F(DaemonRemoteTest, RemoteServiceDoubleDeinit)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19093,
        .maxClients = 5,
        .enable = true,
    };

    SilogDaemonRemoteInit(&config);
    SilogDaemonRemoteDeinit();

    // 第二次反初始化应该安全
    SilogDaemonRemoteDeinit();
}

// 测试：远程服务（enable 字段当前未使用）
TEST_F(DaemonRemoteTest, RemoteServiceEnableFieldNotUsed)
{
    // 注意：当前实现不检查 enable 字段
    SilogDaemonRemoteConfig config = {
        .listenPort = 19094,
        .maxClients = 5,
        .enable = false, // 此字段当前被忽略
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    // 当前实现不检查 enable 字段，所以仍然会初始化
    EXPECT_EQ(result, SILOG_OK);
    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    SilogDaemonRemoteDeinit();
}

// 测试：使用默认端口（端口为 0）
TEST_F(DaemonRemoteTest, RemoteServiceDefaultPort)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 0, // 使用默认端口
        .maxClients = 5,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);
    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    SilogDaemonRemoteDeinit();
}

// 测试：使用默认最大客户端数（为 0）
TEST_F(DaemonRemoteTest, RemoteServiceDefaultMaxClients)
{
    SilogDaemonRemoteConfig config = {
        .listenPort = 19095,
        .maxClients = 0, // 使用默认值
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&config);
    EXPECT_EQ(result, SILOG_OK);
    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    SilogDaemonRemoteDeinit();
}

// ==================== Daemon 和远程服务组合测试 ====================

class DaemonIntegrationTest : public ::testing::Test {
  protected:
    std::string testDir;

    void SetUp() override
    {
        // 创建唯一的测试目录
        char tmpDir[] = "/tmp/silog_integration_test_XXXXXX";
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

// 测试：完整守护进程初始化
TEST_F(DaemonIntegrationTest, FullDaemonInit)
{
    // 设置文件管理器配置
    SilogFileManagerSetLogDir(testDir.c_str());
    SilogFileManagerSetLogFileBase("testlog");

    // 初始化守护进程
    int32_t result = SilogDaemonInit();
    EXPECT_EQ(result, SILOG_OK);

    // 初始化远程服务
    SilogDaemonRemoteConfig remoteConfig = {
        .listenPort = 19096,
        .maxClients = 3,
        .enable = true,
    };

    result = SilogDaemonRemoteInit(&remoteConfig);
    EXPECT_EQ(result, SILOG_OK);

    EXPECT_TRUE(SilogDaemonRemoteIsEnabled());

    SilogDaemonRemoteDeinit();
    SilogDaemonDeinit();
}

// 测试：先初始化远程服务再初始化守护进程
TEST_F(DaemonIntegrationTest, RemoteInitBeforeDaemon)
{
    // 先初始化远程服务
    SilogDaemonRemoteConfig remoteConfig = {
        .listenPort = 19097,
        .maxClients = 3,
        .enable = true,
    };

    int32_t result = SilogDaemonRemoteInit(&remoteConfig);
    EXPECT_EQ(result, SILOG_OK);

    // 再初始化守护进程
    result = SilogDaemonInit();
    EXPECT_EQ(result, SILOG_OK);

    SilogDaemonDeinit();
    SilogDaemonRemoteDeinit();
}

// ==================== 错误处理测试 ====================

// 测试：未初始化时的操作
TEST(DaemonErrorTest, OperationsWithoutInit)
{
    // 确保完全清理
    SilogDaemonRemoteDeinit();
    SilogDaemonDeinit();
    usleep(20000);

    // 未初始化时获取客户端数量应该为 0
    EXPECT_EQ(SilogDaemonRemoteGetClientCount(), 0);

    // 未初始化时检查是否启用应该为 false
    EXPECT_FALSE(SilogDaemonRemoteIsEnabled());
}

// 测试：使用特权端口（在 root 环境下可以绑定）
TEST(DaemonErrorTest, PrivilegedPort)
{
    // 确保完全清理
    SilogDaemonRemoteDeinit();
    SilogDaemonDeinit();
    usleep(20000);

    SilogDaemonRemoteConfig config = {
        .listenPort = 80, // 特权端口，在 root 环境下可以绑定
        .maxClients = 5,
        .enable = true,
    };

    // 在 root 环境下应该成功
    int32_t result = SilogDaemonRemoteInit(&config);
    // 测试结果取决于运行权限
    if (getuid() == 0) {
        EXPECT_EQ(result, SILOG_OK);
    } else {
        EXPECT_NE(result, SILOG_OK);
    }

    SilogDaemonRemoteDeinit();
}
