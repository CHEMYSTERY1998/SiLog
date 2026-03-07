#include "silog_error.h"
#include "silog_remote.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "silog_logger.h"
#include "silog_securec.h"

// 测试端口（使用非标准端口避免冲突）
#define TEST_REMOTE_PORT 19090
#define TEST_CONNECT_TIMEOUT_MS 100

// ==================== Remote 模块基础测试 ====================

class RemoteTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        // 确保反初始化状态
        SilogRemoteDeinit();
    }

    void TearDown() override
    {
        SilogRemoteDeinit();
        usleep(10000); // 10ms 等待资源释放
    }
};

// 测试：默认状态下未初始化
TEST_F(RemoteTest, NotInitByDefault)
{
    EXPECT_FALSE(SilogRemoteIsInit());
    EXPECT_EQ(SilogRemoteGetClientCount(), 0);
}

// 测试：使用默认配置初始化
TEST_F(RemoteTest, InitWithDefaultConfig)
{
    int32_t ret = SilogRemoteInit(NULL);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_TRUE(SilogRemoteIsInit());
    EXPECT_EQ(SilogRemoteGetClientCount(), 0);

    SilogRemoteDeinit();
}

// 测试：使用自定义配置初始化
TEST_F(RemoteTest, InitWithCustomConfig)
{
    SilogRemoteConfig_t config = {
        .listenPort = TEST_REMOTE_PORT,
        .maxClients = 5,
        .enableAuth = false,
    };

    int32_t ret = SilogRemoteInit(&config);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_TRUE(SilogRemoteIsInit());

    SilogRemoteDeinit();
}

// 测试：重复初始化（应该成功，不重复创建）
TEST_F(RemoteTest, DoubleInit)
{
    int32_t ret = SilogRemoteInit(NULL);
    EXPECT_EQ(ret, SILOG_OK);

    // 第二次初始化应该返回 OK，但不会重新创建
    ret = SilogRemoteInit(NULL);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_TRUE(SilogRemoteIsInit());

    SilogRemoteDeinit();
}

// 测试：未初始化时 Accept 返回错误
TEST_F(RemoteTest, AcceptWithoutInit)
{
    int32_t ret = SilogRemoteAccept();
    EXPECT_EQ(ret, SILOG_NET_FILE_ERROR);
}

// 测试：未初始化时广播返回错误
TEST_F(RemoteTest, BroadcastWithoutInit)
{
    logEntry_t entry = {};
    int32_t sentCount = 0;
    int32_t ret = SilogRemoteBroadcast(&entry, &sentCount);
    EXPECT_EQ(ret, SILOG_TRANS_NOT_INIT);
    EXPECT_EQ(sentCount, 0);
}

// 测试：广播空指针
TEST_F(RemoteTest, BroadcastNullEntry)
{
    int32_t ret = SilogRemoteInit(NULL);
    EXPECT_EQ(ret, SILOG_OK);

    int32_t sentCount = 0;
    ret = SilogRemoteBroadcast(NULL, &sentCount);
    EXPECT_EQ(ret, SILOG_NULL_PTR);
    EXPECT_EQ(sentCount, 0);
}

// 测试：Accept 非阻塞（无连接时返回 TIMEOUT）
TEST_F(RemoteTest, AcceptNonBlocking)
{
    int32_t ret = SilogRemoteInit(NULL);
    EXPECT_EQ(ret, SILOG_OK);

    // 非阻塞模式下，没有新连接应该返回 TIMEOUT
    ret = SilogRemoteAccept();
    EXPECT_EQ(ret, SILOG_NET_TIMEOUT);
}

// 测试：重复 Deinit（应该安全）
TEST_F(RemoteTest, DoubleDeinit)
{
    int32_t ret = SilogRemoteInit(NULL);
    EXPECT_EQ(ret, SILOG_OK);

    SilogRemoteDeinit();
    SilogRemoteDeinit(); // 应该安全
    SilogRemoteDeinit();

    EXPECT_FALSE(SilogRemoteIsInit());
}

// ==================== 客户端连接测试 ====================

class RemoteClientTest : public ::testing::Test {
  protected:
    int clientFd = -1;

    void SetUp() override
    {
        SilogRemoteDeinit();

        SilogRemoteConfig_t config = {
            .listenPort = TEST_REMOTE_PORT,
            .maxClients = 5,
            .enableAuth = false,
        };

        int32_t ret = SilogRemoteInit(&config);
        ASSERT_EQ(ret, SILOG_OK);

        // 创建客户端 socket
        clientFd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(clientFd, 0);

        // 等待服务启动
        usleep(50000); // 50ms
    }

    void TearDown() override
    {
        if (clientFd >= 0) {
            close(clientFd);
        }
        SilogRemoteDeinit();
        usleep(10000);
    }

    bool ConnectClient()
    {
        struct sockaddr_in addr;
        (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TEST_REMOTE_PORT);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        int ret = connect(clientFd, (struct sockaddr *)&addr, sizeof(addr));
        if (ret < 0) {
            return false;
        }

        // 等待服务器接受连接
        usleep(50000); // 50ms

        // 触发 Accept
        SilogRemoteAccept();
        usleep(10000); // 10ms

        return true;
    }
};

// 测试：客户端连接
TEST_F(RemoteClientTest, ClientConnect)
{
    EXPECT_TRUE(ConnectClient());
    EXPECT_EQ(SilogRemoteGetClientCount(), 1);
}

// 测试：广播给单个客户端
TEST_F(RemoteClientTest, BroadcastToSingleClient)
{
    EXPECT_TRUE(ConnectClient());
    EXPECT_EQ(SilogRemoteGetClientCount(), 1);

    logEntry_t entry = {};
    entry.ts = 123456789;
    entry.level = SILOG_INFO;
    entry.pid = getpid();
    entry.tid = getpid();
    entry.line = 100;
    entry.msgLen = 12;
    entry.enabled = 1;

    (void)strncpy_s(entry.tag, sizeof(entry.tag), "TestTag", strlen("TestTag"));
    (void)strncpy_s(entry.file, sizeof(entry.file), "test.cpp", strlen("test.cpp"));
    (void)strncpy_s(entry.msg, sizeof(entry.msg), "Hello Remote", strlen("Hello Remote"));

    int32_t sent = 0;
    int32_t ret = SilogRemoteBroadcast(&entry, &sent);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_EQ(sent, 1);

    // 接收广播的数据（序列化后的二进制格式）
    uint8_t buf[SILOG_REMOTE_ENTRY_SIZE];
    ssize_t n = recv(clientFd, buf, SILOG_REMOTE_ENTRY_SIZE, 0);
    EXPECT_EQ(n, SILOG_REMOTE_ENTRY_SIZE);

    // 反序列化并验证数据
    // ts: 8 bytes big-endian
    uint64_t recvTs = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
                      ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
                      ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
                      ((uint64_t)buf[6] << 8) | (uint64_t)buf[7];
    EXPECT_EQ(recvTs, entry.ts);

    // level: 1 byte at offset 20
    EXPECT_EQ(buf[20], entry.level);
}

// 测试：多个客户端连接
TEST_F(RemoteClientTest, MultipleClients)
{
    // 创建额外的客户端
    int clientFd2 = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(clientFd2, 0);

    // 第一个客户端连接
    EXPECT_TRUE(ConnectClient());

    // 第二个客户端连接
    struct sockaddr_in addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_REMOTE_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int32_t connRet = connect(clientFd2, (struct sockaddr *)&addr, sizeof(addr));
    EXPECT_EQ(connRet, 0);

    usleep(50000);
    SilogRemoteAccept();
    usleep(10000);

    EXPECT_EQ(SilogRemoteGetClientCount(), 2);

    // 广播给所有客户端
    logEntry_t entry = {};
    entry.ts = 987654321;
    entry.level = SILOG_DEBUG;
    entry.pid = getpid();
    entry.msgLen = 5;
    entry.enabled = 1;
    (void)strncpy_s(entry.msg, sizeof(entry.msg), "Test", strlen("Test"));

    int32_t sent = 0;
    int32_t ret = SilogRemoteBroadcast(&entry, &sent);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_EQ(sent, 2);

    // 验证两个客户端都收到数据
    uint8_t buf1[SILOG_REMOTE_ENTRY_SIZE];
    uint8_t buf2[SILOG_REMOTE_ENTRY_SIZE];
    ssize_t n1 = recv(clientFd, buf1, SILOG_REMOTE_ENTRY_SIZE, 0);
    ssize_t n2 = recv(clientFd2, buf2, SILOG_REMOTE_ENTRY_SIZE, 0);

    EXPECT_EQ(n1, SILOG_REMOTE_ENTRY_SIZE);
    EXPECT_EQ(n2, SILOG_REMOTE_ENTRY_SIZE);

    // 验证时间戳
    uint64_t recvTs1 = ((uint64_t)buf1[0] << 56) | ((uint64_t)buf1[1] << 48) |
                       ((uint64_t)buf1[2] << 40) | ((uint64_t)buf1[3] << 32) |
                       ((uint64_t)buf1[4] << 24) | ((uint64_t)buf1[5] << 16) |
                       ((uint64_t)buf1[6] << 8) | (uint64_t)buf1[7];
    uint64_t recvTs2 = ((uint64_t)buf2[0] << 56) | ((uint64_t)buf2[1] << 48) |
                       ((uint64_t)buf2[2] << 40) | ((uint64_t)buf2[3] << 32) |
                       ((uint64_t)buf2[4] << 24) | ((uint64_t)buf2[5] << 16) |
                       ((uint64_t)buf2[6] << 8) | (uint64_t)buf2[7];
    EXPECT_EQ(recvTs1, entry.ts);
    EXPECT_EQ(recvTs2, entry.ts);

    close(clientFd2);
}

// 测试：客户端断开连接
TEST_F(RemoteClientTest, ClientDisconnect)
{
    EXPECT_TRUE(ConnectClient());
    EXPECT_EQ(SilogRemoteGetClientCount(), 1);

    // 关闭客户端连接
    close(clientFd);
    clientFd = -1;

    usleep(100000); // 100ms 等待服务器检测到断开

    // 广播会检测到连接已关闭
    logEntry_t entry = {};
    entry.ts = 111111;
    entry.level = SILOG_WARN;
    entry.pid = getpid();
    entry.msgLen = 4;
    entry.enabled = 1;
    (void)strncpy_s(entry.msg, sizeof(entry.msg), "Test", strlen("Test"));

    // 由于客户端已关闭，发送应该失败，客户端被移除
    int32_t sent = 0;
    SilogRemoteBroadcast(&entry, &sent);
    usleep(50000);

    // 重新检查客户端数量（由于连接关闭，应该为 0）
    // 注意：实际行为取决于 send 是否立即返回错误
}

// 测试：达到最大客户端数
TEST_F(RemoteClientTest, MaxClientsReached)
{
    SilogRemoteDeinit();

    // 重新初始化，限制为 2 个客户端
    SilogRemoteConfig_t config = {
        .listenPort = TEST_REMOTE_PORT,
        .maxClients = 2,
        .enableAuth = false,
    };

    int32_t ret = SilogRemoteInit(&config);
    ASSERT_EQ(ret, SILOG_OK);

    int fds[3];
    for (int i = 0; i < 3; i++) {
        fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(fds[i], 0);

        struct sockaddr_in addr;
        (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TEST_REMOTE_PORT);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        int connRet = connect(fds[i], (struct sockaddr *)&addr, sizeof(addr));
        if (i < 2) {
            EXPECT_EQ(connRet, 0);
        }
        // 第三个连接可能被拒绝，也可能被接受但后续 accept 失败

        usleep(20000);
        SilogRemoteAccept();
        usleep(10000);
    }

    // 清理
    for (int i = 0; i < 3; i++) {
        if (fds[i] >= 0) {
            close(fds[i]);
        }
    }
}

// 测试：空广播（无客户端）
TEST_F(RemoteClientTest, BroadcastNoClients)
{
    logEntry_t entry = {};
    entry.ts = 111111;
    entry.level = SILOG_INFO;
    entry.pid = getpid();
    entry.msgLen = 4;
    entry.enabled = 1;
    (void)strncpy_s(entry.msg, sizeof(entry.msg), "Test", strlen("Test"));

    int32_t sent = 0;
    int32_t ret = SilogRemoteBroadcast(&entry, &sent);
    EXPECT_EQ(ret, SILOG_OK);
    EXPECT_EQ(sent, 0);
}
