/**
 * @file remote_integration_test.cpp
 * @brief silog_remote 模块集成测试
 *
 * 测试远程日志服务的完整工作流程，包括：
 * - 服务启动/停止
 * - 多客户端连接管理
 * - 日志广播
 * - 并发场景
 */

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <vector>

#include "silog_error.h"
#include "silog_logger.h"
#include "silog_remote.h"
#include "silog_securec.h"

#define TEST_INTEGRATION_PORT 29090
#define LOG_ENTRY_MAGIC 0xDEADBEEF

// 集成测试基类
class RemoteIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        SilogRemoteDeinit();
    }

    void TearDown() override
    {
        SilogRemoteDeinit();
        usleep(20000);  // 20ms 等待资源释放
    }

    bool StartServer(uint16_t port, uint32_t maxClients = 10)
    {
        SilogRemoteConfig_t config = {
            .listenPort = port,
            .maxClients = maxClients,
            .enableAuth = false,
        };
        return SilogRemoteInit(&config) == SILOG_OK;
    }

    int CreateClientSocket()
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        return fd;
    }

    bool ConnectClient(int fd, uint16_t port)
    {
        struct sockaddr_in addr;
        (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        int ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (ret < 0) {
            return false;
        }

        usleep(50000);  // 50ms 等待连接建立
        SilogRemoteAccept();
        usleep(10000);
        return true;
    }
};

// 测试：完整的服务启动-连接-广播-关闭流程
TEST_F(RemoteIntegrationTest, FullWorkflow)
{
    // 1. 启动服务
    EXPECT_TRUE(StartServer(TEST_INTEGRATION_PORT));
    EXPECT_TRUE(SilogRemoteIsInit());

    // 2. 客户端连接
    int client = CreateClientSocket();
    ASSERT_GE(client, 0);
    EXPECT_TRUE(ConnectClient(client, TEST_INTEGRATION_PORT));
    EXPECT_EQ(SilogRemoteGetClientCount(), 1);

    // 3. 发送日志
    logEntry_t entry = {};
    entry.ts = 1234567890123ULL;
    entry.level = SILOG_INFO;
    entry.pid = 12345;
    entry.tid = 12346;
    entry.line = 42;
    entry.msgLen = 20;
    entry.enabled = 1;
    (void)strncpy_s(entry.tag, sizeof(entry.tag), "IntegrationTest", strlen("IntegrationTest"));
    (void)strncpy_s(entry.file, sizeof(entry.file), "test.cpp", strlen("test.cpp"));
    (void)strncpy_s(entry.msg, sizeof(entry.msg), "Integration test msg", strlen("Integration test msg"));

    int32_t sent = 0;
    int32_t ret = SilogRemoteBroadcast(&entry, &sent);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_EQ(sent, 1);

    // 4. 客户端接收（序列化后的数据）
    uint8_t buf[SILOG_REMOTE_ENTRY_SIZE];
    ssize_t n = recv(client, buf, SILOG_REMOTE_ENTRY_SIZE, 0);
    EXPECT_EQ(n, SILOG_REMOTE_ENTRY_SIZE);

    // 验证时间戳（大端序）
    uint64_t recvTs = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
                      ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
                      ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
                      ((uint64_t)buf[6] << 8) | (uint64_t)buf[7];
    EXPECT_EQ(recvTs, entry.ts);

    // 验证日志级别
    EXPECT_EQ(buf[20], entry.level);

    // 5. 断开连接
    close(client);
    usleep(100000);

    // 6. 停止服务
    SilogRemoteDeinit();
    EXPECT_FALSE(SilogRemoteIsInit());
}

// 测试：多客户端广播场景
TEST_F(RemoteIntegrationTest, MultiClientBroadcast)
{
    const int CLIENT_COUNT = 5;

    EXPECT_TRUE(StartServer(TEST_INTEGRATION_PORT, CLIENT_COUNT));

    // 创建多个客户端
    std::vector<int> clients;
    for (int i = 0; i < CLIENT_COUNT; i++) {
        int fd = CreateClientSocket();
        ASSERT_GE(fd, 0);
        EXPECT_TRUE(ConnectClient(fd, TEST_INTEGRATION_PORT));
        clients.push_back(fd);
    }

    EXPECT_EQ(SilogRemoteGetClientCount(), CLIENT_COUNT);

    // 发送多条日志
    for (int i = 0; i < 10; i++) {
        logEntry_t entry = {};
        entry.ts = 1000000ULL + i;
        entry.level = static_cast<silogLevel>(i % 5);
        entry.pid = getpid();
        entry.msgLen = 10;
        entry.enabled = 1;

        char msg[32];
        (void)sprintf_s(msg, sizeof(msg), "Message %d", i);
        (void)strncpy_s(entry.msg, sizeof(entry.msg), msg, strlen(msg));
        (void)strncpy_s(entry.tag, sizeof(entry.tag), "MultiTest", strlen("MultiTest"));

        int32_t sent = 0;
        int32_t ret = SilogRemoteBroadcast(&entry, &sent);
        EXPECT_EQ(ret, SILOG_OK);
        EXPECT_EQ(sent, CLIENT_COUNT);

        // 每个客户端都验证接收（序列化数据）
        for (int j = 0; j < CLIENT_COUNT; j++) {
            uint8_t buf[SILOG_REMOTE_ENTRY_SIZE];
            ssize_t n = recv(clients[j], buf, SILOG_REMOTE_ENTRY_SIZE, 0);
            EXPECT_EQ(n, SILOG_REMOTE_ENTRY_SIZE) << "Client " << j << " failed to receive msg " << i;

            // 验证时间戳
            uint64_t recvTs = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
                              ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
                              ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
                              ((uint64_t)buf[6] << 8) | (uint64_t)buf[7];
            EXPECT_EQ(recvTs, entry.ts) << "Client " << j << " timestamp mismatch msg " << i;
        }
    }

    // 清理
    for (int fd : clients) {
        close(fd);
    }
}

// 测试：服务重启后客户端重新连接
TEST_F(RemoteIntegrationTest, ServerRestart)
{
    // 第一次启动
    EXPECT_TRUE(StartServer(TEST_INTEGRATION_PORT));

    int client1 = CreateClientSocket();
    ASSERT_GE(client1, 0);
    EXPECT_TRUE(ConnectClient(client1, TEST_INTEGRATION_PORT));
    EXPECT_EQ(SilogRemoteGetClientCount(), 1);

    // 关闭服务
    SilogRemoteDeinit();
    close(client1);
    usleep(100000);

    // 重新启动服务
    EXPECT_TRUE(StartServer(TEST_INTEGRATION_PORT));

    int client2 = CreateClientSocket();
    ASSERT_GE(client2, 0);
    EXPECT_TRUE(ConnectClient(client2, TEST_INTEGRATION_PORT));
    EXPECT_EQ(SilogRemoteGetClientCount(), 1);

    // 验证通信正常
    logEntry_t entry = {};
    entry.ts = 999999;
    entry.level = SILOG_DEBUG;
    entry.pid = getpid();
    entry.msgLen = 5;
    entry.enabled = 1;
    (void)strncpy_s(entry.msg, sizeof(entry.msg), "After", strlen("After"));

    int32_t sent = 0;
    int32_t ret = SilogRemoteBroadcast(&entry, &sent);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_EQ(sent, 1);

    // 接收序列化数据
    uint8_t buf[SILOG_REMOTE_ENTRY_SIZE];
    ssize_t n = recv(client2, buf, SILOG_REMOTE_ENTRY_SIZE, 0);
    EXPECT_EQ(n, SILOG_REMOTE_ENTRY_SIZE);

    // 验证时间戳
    uint64_t recvTs = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
                      ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
                      ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
                      ((uint64_t)buf[6] << 8) | (uint64_t)buf[7];
    EXPECT_EQ(recvTs, entry.ts);

    close(client2);
}

// 测试：并发连接
TEST_F(RemoteIntegrationTest, ConcurrentConnections)
{
    const int THREAD_COUNT = 4;
    const int CLIENTS_PER_THREAD = 3;
    const int TOTAL_CLIENTS = THREAD_COUNT * CLIENTS_PER_THREAD;

    EXPECT_TRUE(StartServer(TEST_INTEGRATION_PORT, TOTAL_CLIENTS));

    std::atomic<int> successCount{0};
    pthread_t threads[THREAD_COUNT];

    auto connectFunc = [](void *arg) -> void * {
        auto *count = static_cast<std::atomic<int> *>(arg);
        uint16_t port = TEST_INTEGRATION_PORT;

        for (int i = 0; i < CLIENTS_PER_THREAD; i++) {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) continue;

            struct sockaddr_in addr;
            (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                usleep(30000);
                (*count)++;
            }
            close(fd);
        }
        return nullptr;
    };

    // 启动连接线程
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], nullptr, connectFunc, &successCount);
    }

    // 主线程接受连接
    for (int i = 0; i < TOTAL_CLIENTS; i++) {
        usleep(20000);
        SilogRemoteAccept();
    }

    // 等待所有线程完成
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], nullptr);
    }

    EXPECT_EQ(successCount.load(), TOTAL_CLIENTS);
    EXPECT_EQ(SilogRemoteGetClientCount(), TOTAL_CLIENTS);
}

// 测试：大日志条目传输
TEST_F(RemoteIntegrationTest, LargelogEntry_t)
{
    EXPECT_TRUE(StartServer(TEST_INTEGRATION_PORT));

    int client = CreateClientSocket();
    ASSERT_GE(client, 0);
    EXPECT_TRUE(ConnectClient(client, TEST_INTEGRATION_PORT));

    // 创建填满最大长度的日志条目
    logEntry_t entry = {};
    entry.ts = 8888888888888ULL;
    entry.level = SILOG_FATAL;
    entry.pid = 99999;
    entry.tid = 99998;
    entry.line = 99999;
    entry.msgLen = SILOG_MSG_MAX_LEN - 1;
    entry.enabled = 1;

    // 填满 tag
    char longTag[SILOG_TAG_MAX_LEN];
    (void)memset_s(longTag, sizeof(longTag), 'T', sizeof(longTag) - 1);
    longTag[sizeof(longTag) - 1] = '\0';
    (void)strncpy_s(entry.tag, sizeof(entry.tag), longTag, strlen(longTag));

    // 填满 file
    char longFile[SILOG_FILE_MAX_LEN];
    (void)memset_s(longFile, sizeof(longFile), 'F', sizeof(longFile) - 1);
    longFile[sizeof(longFile) - 1] = '\0';
    (void)strncpy_s(entry.file, sizeof(entry.file), longFile, strlen(longFile));

    // 填满 msg
    char longMsg[SILOG_MSG_MAX_LEN];
    (void)memset_s(longMsg, sizeof(longMsg), 'M', sizeof(longMsg) - 1);
    longMsg[sizeof(longMsg) - 1] = '\0';
    (void)strncpy_s(entry.msg, sizeof(entry.msg), longMsg, strlen(longMsg));

    int32_t sent = 0;
    int32_t ret = SilogRemoteBroadcast(&entry, &sent);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_EQ(sent, 1);

    // 验证接收（序列化数据）
    uint8_t buf[SILOG_REMOTE_ENTRY_SIZE];
    ssize_t n = recv(client, buf, SILOG_REMOTE_ENTRY_SIZE, 0);
    EXPECT_EQ(n, SILOG_REMOTE_ENTRY_SIZE);

    // 验证时间戳
    uint64_t recvTs = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
                      ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
                      ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
                      ((uint64_t)buf[6] << 8) | (uint64_t)buf[7];
    EXPECT_EQ(recvTs, entry.ts);

    close(client);
}

// 测试：无效端口配置
TEST_F(RemoteIntegrationTest, InvalidPort)
{
    // 端口 0 应该使用默认端口
    SilogRemoteConfig_t config = {
        .listenPort = 0,
        .maxClients = 10,
        .enableAuth = false,
    };

    EXPECT_EQ(SilogRemoteInit(&config), SILOG_OK);
    SilogRemoteDeinit();
}

// 测试：无客户端时广播性能
TEST_F(RemoteIntegrationTest, BroadcastNoClientsPerformance)
{
    EXPECT_TRUE(StartServer(TEST_INTEGRATION_PORT));

    logEntry_t entry = {};
    entry.ts = 111111;
    entry.level = SILOG_INFO;
    entry.pid = getpid();
    entry.msgLen = 10;
    entry.enabled = 1;
    (void)strncpy_s(entry.msg, sizeof(entry.msg), "PerfTest", strlen("PerfTest"));

    // 广播 1000 次，应该快速返回
    for (int i = 0; i < 1000; i++) {
        int32_t sent = 0;
        int32_t ret = SilogRemoteBroadcast(&entry, &sent);
        EXPECT_EQ(ret, SILOG_OK);
        EXPECT_EQ(sent, 0);
    }
}
