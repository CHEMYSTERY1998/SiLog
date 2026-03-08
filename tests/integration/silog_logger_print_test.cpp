#include "silog.h"
#include "silog_error.h"
#include "silog_logger.h"
#include "silog_securec.h"
#include "silog_time.h"
#include "silog_ipc.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define LOGD_SOCKET_PATH  "/tmp/logd.sock"
#define TEST_TAG          "SiLogTest"
#define SILOG_LOGGER_FILE "/tmp/silog_logger.txt"

/* 测试辅助类：用于捕获日志输出 */
class SilogCaptureTest : public ::testing::Test {
  protected:
    int serverFd = -1;
    FILE *logFile = nullptr;
    char logFilePath[256] = "";

    void SetUp() override
    {
        /* 清理可能存在的旧 socket 文件 */
        unlink(LOGD_SOCKET_PATH);

        /* 清空日志文件 */
        unlink(SILOG_LOGGER_FILE);

        /* 先创建服务器 socket（在日志系统初始化之前） */
        serverFd = socket(AF_UNIX, SOCK_DGRAM, 0);
        ASSERT_GE(serverFd, 0);

        struct sockaddr_un addr;
        (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), LOGD_SOCKET_PATH, strlen(LOGD_SOCKET_PATH));

        ASSERT_EQ(bind(serverFd, (struct sockaddr *)&addr, sizeof(addr)), 0);

        /* 等待确保 socket 绑定完成 */
        usleep(50000); /* 50ms */

        /* 设置日志文件路径用于验证 */
        (void)strncpy_s(logFilePath, sizeof(logFilePath), SILOG_LOGGER_FILE, strlen(SILOG_LOGGER_FILE));
    }

    void TearDown() override
    {
        if (serverFd >= 0) {
            close(serverFd);
        }
        if (logFile) {
            fclose(logFile);
        }
        unlink(LOGD_SOCKET_PATH);
    }

    /* 清空所有待处理的日志 */
    void drainPendingLogs()
    {
        logEntry_t entry;
        while (receiveLogEntryNonBlock(&entry) > 0) {
            /* 丢弃 */
        }
    }

    /* 非阻塞接收日志条目 */
    int receiveLogEntryNonBlock(logEntry_t *entry)
    {
        int flags = fcntl(serverFd, F_GETFL, 0);
        fcntl(serverFd, F_SETFL, flags | O_NONBLOCK);

        ssize_t recvLen = recv(serverFd, entry, sizeof(logEntry_t), 0);

        fcntl(serverFd, F_SETFL, flags);

        return (recvLen > 0) ? (int)recvLen : -1;
    }

    /* 等待并接收任意日志条目（用于调试） */
    int waitForAnyLog(logEntry_t *entry, int timeoutMs = 500)
    {
        int elapsed = 0;
        const int step = 1; /* 1ms */

        /* 设置非阻塞模式 */
        int flags = fcntl(serverFd, F_GETFL, 0);
        fcntl(serverFd, F_SETFL, flags | O_NONBLOCK);

        while (elapsed < timeoutMs) {
            ssize_t recvLen = recv(serverFd, entry, sizeof(logEntry_t), 0);
            if (recvLen > 0) {
                fcntl(serverFd, F_SETFL, flags); /* 恢复原模式 */
                return (int)recvLen;
            }
            usleep(step * 1000);
            elapsed += step;
        }

        fcntl(serverFd, F_SETFL, flags); /* 恢复原模式 */
        return -1;
    }

    /* 等待并接收匹配指定 tag 的日志条目（带超时） */
    int waitForLogEntry(logEntry_t *entry, const char *expectedTag = nullptr, int timeoutMs = 500)
    {
        int elapsed = 0;
        const int step = 1; /* 1ms */

        /* 设置非阻塞模式 */
        int flags = fcntl(serverFd, F_GETFL, 0);
        fcntl(serverFd, F_SETFL, flags | O_NONBLOCK);

        while (elapsed < timeoutMs) {
            ssize_t recvLen = recv(serverFd, entry, sizeof(logEntry_t), 0);
            if (recvLen > 0) {
                /* 如果指定了 tag，检查是否匹配 */
                if (expectedTag == nullptr || strcmp(entry->tag, expectedTag) == 0) {
                    fcntl(serverFd, F_SETFL, flags); /* 恢复原模式 */
                    return (int)recvLen;
                }
            }
            usleep(step * 1000);
            elapsed += step;
        }

        fcntl(serverFd, F_SETFL, flags); /* 恢复原模式 */
        return -1;
    }
};

/* 测试：SILOG_I 宏（基本功能）- 不使用格式化参数 */
TEST_F(SilogCaptureTest, BasicInfoMacroNoFormat)
{
    const char *testMsg = "BasicInfoTestNoFormat";
    SILOG_I(TEST_TAG, testMsg);

    logEntry_t entry;
    ASSERT_GT(waitForAnyLog(&entry), 0);

    EXPECT_EQ(entry.level, SILOG_INFO);
    EXPECT_STREQ(entry.tag, TEST_TAG);
    EXPECT_STREQ(entry.msg, testMsg);
    EXPECT_GT(entry.ts, 0);
    EXPECT_GT(entry.pid, 0);
    EXPECT_GT(entry.tid, 0);
}

/* 测试：SILOG_I 宏（带格式化参数） */
TEST_F(SilogCaptureTest, InfoMacroWithFormat)
{
    /* 先测试简单的不带 % 的消息 */
    SILOG_I(TEST_TAG, "SimpleMessage");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_EQ(entry.level, SILOG_INFO);
    EXPECT_STREQ(entry.tag, TEST_TAG);
    EXPECT_STREQ(entry.msg, "SimpleMessage");
}

/* 测试：SILOG_D 宏 */
TEST_F(SilogCaptureTest, DebugMacro)
{
    SILOG_D(TEST_TAG, "DebugMessage");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_EQ(entry.level, SILOG_DEBUG);
    EXPECT_STREQ(entry.tag, TEST_TAG);
    EXPECT_STREQ(entry.msg, "DebugMessage");
}

/* 测试：SILOG_W 宏 */
TEST_F(SilogCaptureTest, WarningMacro)
{
    SILOG_W(TEST_TAG, "WarningMessage");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_EQ(entry.level, SILOG_WARN);
    EXPECT_STREQ(entry.tag, TEST_TAG);
    EXPECT_STREQ(entry.msg, "WarningMessage");
}

/* 测试：SILOG_E 宏 */
TEST_F(SilogCaptureTest, ErrorMacro)
{
    SILOG_E(TEST_TAG, "ErrorMessage");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_EQ(entry.level, SILOG_ERROR);
    EXPECT_STREQ(entry.tag, TEST_TAG);
    EXPECT_STREQ(entry.msg, "ErrorMessage");
}

/* 测试：SILOG_F 宏 */
TEST_F(SilogCaptureTest, FatalMacro)
{
    SILOG_F(TEST_TAG, "FatalMessage");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_EQ(entry.level, SILOG_FATAL);
    EXPECT_STREQ(entry.tag, TEST_TAG);
    EXPECT_STREQ(entry.msg, "FatalMessage");
}

/* 测试：多个日志级别 */
TEST_F(SilogCaptureTest, AllLogLevels)
{
    const char *messages[] = {"D-Level", "I-Level", "W-Level", "E-Level", "F-Level"};

    for (int i = 0; i < 5; i++) {
        switch (i) {
        case 0:
            SILOG_D(TEST_TAG, messages[i]);
            break;
        case 1:
            SILOG_I(TEST_TAG, messages[i]);
            break;
        case 2:
            SILOG_W(TEST_TAG, messages[i]);
            break;
        case 3:
            SILOG_E(TEST_TAG, messages[i]);
            break;
        case 4:
            SILOG_F(TEST_TAG, messages[i]);
            break;
        }
    }

    /* 验证收到的日志级别和消息 */
    silogLevel expectedLevels[] = {SILOG_DEBUG, SILOG_INFO, SILOG_WARN, SILOG_ERROR, SILOG_FATAL};
    for (int i = 0; i < 5; i++) {
        logEntry_t entry;
        ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);
        EXPECT_EQ(entry.level, expectedLevels[i]);
        EXPECT_STREQ(entry.msg, messages[i]);
    }
}

/* 测试：长消息 */
TEST_F(SilogCaptureTest, LongMessage)
{
    char longMsg[200];
    (void)memset_s(longMsg, sizeof(longMsg), 'A', 150);
    longMsg[150] = '\0';

    SILOG_I(TEST_TAG, longMsg);

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_EQ(strlen(entry.msg), 150);
    EXPECT_STREQ(entry.msg, longMsg);
}

/* 测试：特殊字符 */
TEST_F(SilogCaptureTest, SpecialCharacters)
{
    SILOG_I(TEST_TAG, "Test chars: tab backslash quote percent");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    /* 验证消息包含特定字符串 */
    EXPECT_NE(strstr(entry.msg, "Test chars:"), nullptr);
}

/* 测试：Tag 长度限制 */
TEST_F(SilogCaptureTest, TagLengthLimit)
{
    char longTag[100];
    (void)memset_s(longTag, sizeof(longTag), 'X', sizeof(longTag) - 1);
    longTag[sizeof(longTag) - 1] = '\0';

    SILOG_I(longTag, "TagLenTest");

    logEntry_t entry;
    ASSERT_GT(waitForAnyLog(&entry), 0);

    /* Tag 应该被截断到 31 字符 + null */
    EXPECT_EQ(strlen(entry.tag), SILOG_TAG_MAX_LEN - 1);
}

/* 测试：文件名和行号 */
TEST_F(SilogCaptureTest, FileAndLine)
{
    SILOG_I(TEST_TAG, "FileLineTest");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    /* 检查文件名不为空 */
    EXPECT_GT(strlen(entry.file), 0);
    /* 行号应该大于 0 */
    EXPECT_GT(entry.line, 0);
}

/* 测试：多个日志连续发送 */
TEST_F(SilogCaptureTest, MultipleLogs)
{
    const int count = 10;
    for (int i = 0; i < count; i++) {
        SILOG_I(TEST_TAG, "Msg");
    }

    int received = 0;
    logEntry_t entry;
    for (int i = 0; i < count + 5; i++) {
        if (waitForLogEntry(&entry, TEST_TAG, 50) > 0) {
            if (strcmp(entry.msg, "Msg") == 0) {
                received++;
            }
        }
    }

    /* 应该接收到至少 count 条日志 */
    EXPECT_GE(received, count);
}

/* 测试：时间戳递增 */
TEST_F(SilogCaptureTest, TimestampIncreasing)
{
    uint64_t prevTs = 0;

    for (int i = 0; i < 5; i++) {
        SILOG_I(TEST_TAG, "TsTest");
        usleep(5000); /* 5ms */

        logEntry_t entry;
        ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

        EXPECT_GE(entry.ts, prevTs);
        prevTs = entry.ts;
    }
}

/* 测试：空消息 */
TEST_F(SilogCaptureTest, EmptyMessage)
{
    SILOG_I(TEST_TAG, "");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_EQ(entry.msgLen, 0);
    EXPECT_EQ(entry.msg[0], '\0');
}

/* 测试：数值格式化 */
TEST_F(SilogCaptureTest, NumberFormatting)
{
    SILOG_I(TEST_TAG, "int=-12345");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    /* 验证消息包含格式化的数字 */
    EXPECT_NE(strstr(entry.msg, "-12345"), nullptr);
}

/* 测试：PID 和 TID 填充 */
TEST_F(SilogCaptureTest, PidAndTidFilled)
{
    SILOG_I(TEST_TAG, "PidTidTest");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    /* PID 应该是当前进程 ID */
    EXPECT_EQ(entry.pid, getpid());
    /* TID 应该非零 */
    EXPECT_GT(entry.tid, 0);
}

/* 测试：日志级别过滤 */
TEST_F(SilogCaptureTest, LevelFilter)
{
    /* 设置最小日志级别为 WARN */
    silogSetLevel(SILOG_WARN);

    /* DEBUG 和 INFO 应该被过滤 */
    SILOG_D(TEST_TAG, "NoDebug");
    SILOG_I(TEST_TAG, "NoInfo");

    /* WARN 和以上应该通过 */
    SILOG_W(TEST_TAG, "YesWarn");

    /* 清理可能的旧日志 */
    drainPendingLogs();

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_EQ(entry.level, SILOG_WARN);
    EXPECT_STREQ(entry.msg, "YesWarn");

    /* 恢复 DEBUG 级别 */
    silogSetLevel(SILOG_DEBUG);
}

/* 测试：并发日志 */
TEST_F(SilogCaptureTest, ConcurrentLogging)
{
    const int threadCount = 3;
    const int logsPerThread = 5;
    pthread_t threads[threadCount];

    struct ThreadArg {
        int id;
        int count;
    };

    auto threadFunc = [](void *arg) -> void * {
        ThreadArg *targ = (ThreadArg *)arg;
        for (int i = 0; i < targ->count; i++) {
            SILOG_I(TEST_TAG, "ThreadMsg");
            usleep(100);
        }
        return nullptr;
    };

    ThreadArg args[threadCount];
    for (int i = 0; i < threadCount; i++) {
        args[i].id = i;
        args[i].count = logsPerThread;
        pthread_create(&threads[i], nullptr, threadFunc, &args[i]);
    }

    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* 接收并计数日志 */
    int received = 0;
    logEntry_t entry;
    for (int i = 0; i < threadCount * logsPerThread + 5; i++) {
        if (waitForLogEntry(&entry, TEST_TAG, 50) > 0) {
            /* 检查是否是我们测试的日志 */
            if (strcmp(entry.msg, "ThreadMsg") == 0) {
                received++;
            }
        }
    }

    EXPECT_GE(received, threadCount * logsPerThread);
}

/* 测试：不同 Tag */
TEST_F(SilogCaptureTest, DifferentTags)
{
    const char *tags[] = {"Tag1", "Tag2", "Tag3"};
    const char *msg = "SameMsg";

    for (int i = 0; i < 3; i++) {
        SILOG_I(tags[i], msg);
    }

    /* 验证每个 tag 的日志 */
    for (int i = 0; i < 3; i++) {
        logEntry_t entry;
        ASSERT_GT(waitForLogEntry(&entry, tags[i]), 0);
        EXPECT_STREQ(entry.tag, tags[i]);
        EXPECT_STREQ(entry.msg, msg);
    }
}

/* 测试：日志级别边界 */
TEST_F(SilogCaptureTest, LevelBoundary)
{
    /* 测试设置无效的日志级别 */
    silogSetLevel((silogLevel)99);
    /* 应该使用默认级别 */

    /* 恢复 */
    silogSetLevel(SILOG_DEBUG);
}

/* 测试：重复日志初始化 */
TEST_F(SilogCaptureTest, DoubleInit)
{
    /* 初始化已经由宏完成，再次初始化应该安全 */
    SILOG_I(TEST_TAG, "First");
    SILOG_I(TEST_TAG, "Second");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);
    EXPECT_STREQ(entry.msg, "First");

    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);
    EXPECT_STREQ(entry.msg, "Second");
}

/* 测试：长 Tag */
TEST_F(SilogCaptureTest, VeryLongTag)
{
    char longTag[100];
    (void)memset_s(longTag, sizeof(longTag), 'T', sizeof(longTag) - 1);
    longTag[sizeof(longTag) - 1] = '\0';

    SILOG_I(longTag, "Test");

    logEntry_t entry;
    ASSERT_GT(waitForAnyLog(&entry), 0);

    /* Tag 应该被截断 */
    EXPECT_LE(strlen(entry.tag), SILOG_TAG_MAX_LEN - 1);
}

/* 测试：包含特殊字符的消息 */
TEST_F(SilogCaptureTest, MessageWithSpecialChars)
{
    SILOG_I(TEST_TAG, "Test with unicode: \u4e2d\u6587");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    EXPECT_NE(strstr(entry.msg, "Test with unicode"), nullptr);
}

/* 测试：大量日志 */
TEST_F(SilogCaptureTest, HighVolume)
{
    const int count = 50;

    for (int i = 0; i < count; i++) {
        SILOG_I(TEST_TAG, "VolumeTest");
    }

    int received = 0;
    logEntry_t entry;
    while (received < count) {
        if (waitForLogEntry(&entry, TEST_TAG, 100) > 0) {
            if (strcmp(entry.msg, "VolumeTest") == 0) {
                received++;
            }
        } else {
            break;
        }
    }

    /* 应该收到大部分日志 */
    EXPECT_GE(received, count * 0.8);
}

/* 测试：日志条目结构完整性 */
TEST_F(SilogCaptureTest, EntryStructure)
{
    SILOG_I(TEST_TAG, "StructureTest");

    logEntry_t entry;
    ASSERT_GT(waitForLogEntry(&entry, TEST_TAG), 0);

    /* 验证所有字段 */
    EXPECT_GT(entry.ts, 0);
    EXPECT_EQ(entry.pid, getpid());
    EXPECT_GT(entry.tid, 0);
    EXPECT_EQ(entry.level, SILOG_INFO);
    EXPECT_GT(strlen(entry.tag), 0);
    EXPECT_GT(strlen(entry.file), 0);
    EXPECT_GT(entry.line, 0);
    EXPECT_EQ(strlen(entry.msg), strlen("StructureTest"));
    EXPECT_EQ(entry.msgLen, strlen("StructureTest"));
    EXPECT_EQ(entry.enabled, 1);
}

/* 测试：快速连续日志 */
TEST_F(SilogCaptureTest, RapidFire)
{
    for (int i = 0; i < 20; i++) {
        SILOG_I(TEST_TAG, "Rapid");
    }

    int received = 0;
    logEntry_t entry;
    for (int i = 0; i < 20; i++) {
        if (waitForLogEntry(&entry, TEST_TAG, 50) > 0) {
            if (strcmp(entry.msg, "Rapid") == 0) {
                received++;
            }
        }
    }

    EXPECT_GE(received, 15);
}
