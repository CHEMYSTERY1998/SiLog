/**
 * @file silog_test.c
 * @brief SiLog 日志系统测试程序
 *
 * 本程序演示如何使用 SILOG_I、SILOG_D 等宏发送日志到守护进程
 *
 * 使用方法：
 * 1. 先启动守护进程: ./silog start
 * 2. 运行测试程序: ./silog_test
 * 3. 查看日志: tail -f /tmp/silog_daemon.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "silog.h"

#define TEST_TAG "MyApp"

/**
 * @brief 测试各种日志级别
 */
static void test_log_levels(void)
{
    printf("Testing different log levels...\n");

    /* DEBUG 级别 */
    SILOG_D(TEST_TAG, "This is a DEBUG message, value=%d", 42);
    usleep(10000); /* 10ms */

    /* INFO 级别 */
    SILOG_I(TEST_TAG, "This is an INFO message, user=%s", "Alice");
    usleep(10000);

    /* WARN 级别 */
    SILOG_W(TEST_TAG, "This is a WARN message, code=0x%x", 0xFF);
    usleep(10000);

    /* ERROR 级别 */
    SILOG_E(TEST_TAG, "This is an ERROR message, errno=%d", -1);
    usleep(10000);

    /* FATAL 级别 */
    SILOG_F(TEST_TAG, "This is a FATAL message!");
    usleep(10000);

    printf("Log levels test completed.\n\n");
}

/**
 * @brief 测试不同模块标签
 */
static void test_modules(void)
{
    printf("Testing different module tags...\n");

    SILOG_I("Network", "Network connection established, fd=%d", 5);
    usleep(10000);

    SILOG_I("Database", "Query executed: SELECT * FROM users");
    usleep(10000);

    SILOG_W("Cache", "Cache miss for key: %s", "user_123");
    usleep(10000);

    SILOG_E("Auth", "Authentication failed for user: %s", "bob");
    usleep(10000);

    printf("Module tags test completed.\n\n");
}

/**
 * @brief 测试批量日志
 */
static void test_batch_logs(void)
{
    printf("Testing batch logs (100 messages)...\n");

    for (int i = 0; i < 100; i++) {
        SILOG_I("BatchTest", "Batch message %d/100", i + 1);
        usleep(1000); /* 1ms */
    }

    printf("Batch logs test completed.\n\n");
}

/**
 * @brief 测试长消息
 */
static void test_long_messages(void)
{
    printf("Testing long messages...\n");

    char long_msg[512];
    snprintf(long_msg, sizeof(long_msg),
             "This is a very long message that contains a lot of information. "
             "It might be truncated by the system if it exceeds the maximum "
             "message length. The current message is designed to test this "
             "limit and see how the system handles it. "
             "Timestamp: %lu", (unsigned long)time(NULL));

    SILOG_I(TEST_TAG, "%s", long_msg);
    usleep(10000);

    printf("Long messages test completed.\n\n");
}

/**
 * @brief 测试格式化字符串
 */
static void test_format_strings(void)
{
    printf("Testing format strings...\n");

    SILOG_D(TEST_TAG, "Integer: %d, Hex: 0x%x, Octal: %o", 255, 255, 255);
    usleep(10000);

    SILOG_I(TEST_TAG, "String: %s, Char: %c", "hello", 'X');
    usleep(10000);

    SILOG_I(TEST_TAG, "Float: %.2f, Double: %.4f", 3.14f, 2.718281828);
    usleep(10000);

    SILOG_I(TEST_TAG, "Pointer: %p", (void *)&test_format_strings);
    usleep(10000);

    printf("Format strings test completed.\n\n");
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("SiLog Test Program\n");
    printf("==================\n\n");

    /* 发送启动日志 */
    SILOG_I(TEST_TAG, "Test program started (PID: %d)", getpid());

    /* 运行各项测试 */
    test_log_levels();
    test_modules();
    test_format_strings();
    test_long_messages();
    test_batch_logs();

    /* 发送结束日志 */
    SILOG_I(TEST_TAG, "Test program finished");

    printf("All tests completed!\n");
    printf("Check the log file: /tmp/silog_daemon.txt\n");

    return 0;
}
