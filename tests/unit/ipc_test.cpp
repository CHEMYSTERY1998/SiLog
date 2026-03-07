#include "silog_error.h"
#include "silog_ipc.h"
#include "silog_securec.h"

#include <errno.h>
#include <gtest/gtest.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// 代码中定义的 socket 路径
#define LOGD_SOCKET_PATH "/tmp/logd.sock"

// ==================== IPC 测试 ====================
// 注意：由于 silog_ipc.c 使用全局静态变量 g_silogIpcAgent
// SilogIpcInit 在 isInit=true 后不会再更新类型

// 测试：Stream 类型未实现
TEST(IpcTest, StreamTypeNotImplemented)
{
    /* 全局状态初始时，isInit = false，函数指针为 NULL */
    SilogIpcInit(SILOG_IPC_TYPE_UNIX_STREAM);

    EXPECT_EQ(SilogIpcClientInit(), SILOG_NOT_IMPLEMENTED);
    EXPECT_EQ(SilogIpcClientSend("test", 4), SILOG_NOT_IMPLEMENTED);
    EXPECT_EQ(SilogIpcServerInit(), SILOG_NOT_IMPLEMENTED);
    EXPECT_EQ(SilogIpcServerRecv(nullptr, 0), SILOG_NOT_IMPLEMENTED);

    /* Close 函数不返回值，但应该不会崩溃 */
    SilogIpcClientClose();
    SilogIpcServerClose();
}

// 测试：Dgram 类型初始化
TEST(IpcTest, DgramTypeInit)
{
    /* 注意：前面的测试已经调用了 SilogIpcInit(SILOG_IPC_TYPE_UNIX_STREAM) */
    /* 由于 isInit 已经为 true，再次 Init 不会改变类型 */
    /* 但实际上 Stream 的函数指针是 NULL，所以调用 Init 应该返回 NOT_IMPLEMENTED */
    /* 实际上这个测试验证的是：当 isInit=true 时，函数指针不会被更新 */
    EXPECT_EQ(SilogIpcClientInit(), SILOG_NOT_IMPLEMENTED);
}

// 测试：ServerClose 重复调用安全
TEST(IpcTest, ServerCloseRepeated)
{
    SilogIpcServerClose();
    SilogIpcServerClose();
    SilogIpcServerClose();
    /* 不应该崩溃 */
}

// 测试：ClientClose 重复调用安全
TEST(IpcTest, ClientCloseRepeated)
{
    SilogIpcClientClose();
    SilogIpcClientClose();
    SilogIpcClientClose();
    /* 不应该崩溃 */
}

// 测试：客户端发送（未初始化 Dgram）
TEST(IpcTest, ClientSendWithoutDgramInit)
{
    const char *testData = "Hello";
    size_t testLen = strlen(testData);
    int ret = SilogIpcClientSend(testData, (uint32_t)testLen);
    EXPECT_EQ(ret, SILOG_NOT_IMPLEMENTED);
}

// 测试：服务器接收（未初始化 Dgram）
TEST(IpcTest, ServerRecvWithoutDgramInit)
{
    char buffer[128];
    int ret = SilogIpcServerRecv(buffer, sizeof(buffer));
    EXPECT_EQ(ret, SILOG_NOT_IMPLEMENTED);
}

// ==================== 使用原生 socket 的集成测试 ====================
// 这些测试不依赖 silog_ipc 的全局状态
// 而是直接使用 Unix socket 来测试通信机制

class SocketCommTest : public ::testing::Test {
  protected:
    int serverFd = -1;
    int clientFd = -1;

    void SetUp() override
    {
        /* 清理可能存在的旧 socket 文件 */
        unlink(LOGD_SOCKET_PATH);

        /* 创建服务器 socket */
        serverFd = socket(AF_UNIX, SOCK_DGRAM, 0);
        ASSERT_GE(serverFd, 0);

        struct sockaddr_un addr;
        (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), LOGD_SOCKET_PATH, strlen(LOGD_SOCKET_PATH));

        ASSERT_EQ(bind(serverFd, (struct sockaddr *)&addr, sizeof(addr)), 0);

        /* 创建客户端 socket */
        clientFd = socket(AF_UNIX, SOCK_DGRAM, 0);
        ASSERT_GE(clientFd, 0);
    }

    void TearDown() override
    {
        if (clientFd >= 0)
            close(clientFd);
        if (serverFd >= 0)
            close(serverFd);
        unlink(LOGD_SOCKET_PATH);
    }
};

// 测试：基本的数据收发
TEST_F(SocketCommTest, BasicSendRecv)
{
    const char *testData = "Hello, Unix Socket!";
    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), LOGD_SOCKET_PATH, strlen(LOGD_SOCKET_PATH));

    /* 客户端发送 */
    size_t testLen = strlen(testData);
    ssize_t sent = sendto(clientFd, testData, testLen, 0, (struct sockaddr *)&addr, sizeof(addr));
    EXPECT_GT(sent, 0);

    /* 服务器接收 */
    char buffer[128] = {0};
    ssize_t recvLen = recv(serverFd, buffer, sizeof(buffer) - 1, 0);
    EXPECT_GT(recvLen, 0);
    EXPECT_STREQ(buffer, testData);
}

// 测试：多次收发
TEST_F(SocketCommTest, MultipleSendRecv)
{
    const char *messages[] = {"First message", "Second message", "Third message"};
    const int msgCount = sizeof(messages) / sizeof(messages[0]);

    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), LOGD_SOCKET_PATH, strlen(LOGD_SOCKET_PATH));

    /* 发送并接收多条消息 */
    char buffer[128] = {0};
    for (int i = 0; i < msgCount; i++) {
        size_t msgLen = strlen(messages[i]);
        ssize_t sent = sendto(clientFd, messages[i], msgLen, 0, (struct sockaddr *)&addr, sizeof(addr));
        EXPECT_GT(sent, 0);

        /* 每次接收前清空 buffer */
        (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
        ssize_t recvLen = recv(serverFd, buffer, sizeof(buffer) - 1, 0);
        EXPECT_GT(recvLen, 0);
        EXPECT_STREQ(buffer, messages[i]);
    }
}

// 测试：大数据传输
TEST_F(SocketCommTest, LargeDataTransfer)
{
    /* 发送 4KB 数据 */
    const size_t dataSize = 4096;
    char *sendData = new char[dataSize];
    (void)memset_s(sendData, dataSize, 'X', dataSize - 1);
    sendData[dataSize - 1] = '\0';

    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), LOGD_SOCKET_PATH, strlen(LOGD_SOCKET_PATH));

    size_t sendLen = strlen(sendData);
    ssize_t sent = sendto(clientFd, sendData, sendLen, 0, (struct sockaddr *)&addr, sizeof(addr));
    EXPECT_GT(sent, 0);

    char *recvData = new char[dataSize];
    (void)memset_s(recvData, dataSize, 0, dataSize);
    ssize_t recvLen = recv(serverFd, recvData, dataSize - 1, 0);
    EXPECT_GT(recvLen, 0);
    EXPECT_STREQ(recvData, sendData);

    delete[] sendData;
    delete[] recvData;
}

// 测试：空消息
TEST_F(SocketCommTest, EmptyMessage)
{
    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), LOGD_SOCKET_PATH, strlen(LOGD_SOCKET_PATH));

    /* 发送空消息 */
    ssize_t sent = sendto(clientFd, "", 0, 0, (struct sockaddr *)&addr, sizeof(addr));
    EXPECT_EQ(sent, 0);

    /* 接收（非阻塞模式下可能返回 0） */
    /* 这里我们使用阻塞模式，所以应该返回 0 */
    char buffer[128];
    ssize_t recvLen = recv(serverFd, buffer, sizeof(buffer), 0);
    EXPECT_EQ(recvLen, 0);
}

// 测试：connect + send 方式
TEST_F(SocketCommTest, ConnectSend)
{
    const char *testData = "Using connect()";

    struct sockaddr_un addr;
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), LOGD_SOCKET_PATH, strlen(LOGD_SOCKET_PATH));

    /* 客户端使用 connect */
    ASSERT_EQ(connect(clientFd, (struct sockaddr *)&addr, sizeof(addr)), 0);

    /* 使用 send 而不是 sendto */
    size_t testLen = strlen(testData);
    ssize_t sent = send(clientFd, testData, testLen, 0);
    EXPECT_GT(sent, 0);

    /* 服务器接收 */
    char buffer[128] = {0};
    ssize_t recvLen = recv(serverFd, buffer, sizeof(buffer) - 1, 0);
    EXPECT_GT(recvLen, 0);
    EXPECT_STREQ(buffer, testData);
}
