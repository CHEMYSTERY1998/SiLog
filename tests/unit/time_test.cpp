#include "silog_time.h"

#include <cstring>
#include <gtest/gtest.h>

// ==================== Time 工具测试 ====================

TEST(TimeTest, GetNowMs)
{
    uint64_t ms = SilogGetNowMs();
    EXPECT_GT(ms, 0);
}

TEST(TimeTest, FormatWallClockMs)
{
    char timeStr[32];
    uint64_t ms = SilogGetNowMs();
    SilogFormatWallClockMs(ms, timeStr, sizeof(timeStr));

    EXPECT_GT(strlen(timeStr), 0);
}

TEST(TimeTest, GetMonoMs)
{
    uint64_t ms1 = SilogGetMonoMs();
    EXPECT_GT(ms1, 0);

    // 确保单调递增
    uint64_t ms2 = SilogGetMonoMs();
    EXPECT_GE(ms2, ms1);
}

TEST(TimeTest, FormatWallClockMs_NullBuffer)
{
    // 测试空指针处理，应该不崩溃
    SilogFormatWallClockMs(0, NULL, 32);
}

TEST(TimeTest, FormatWallClockMs_ZeroLen)
{
    char timeStr[32] = "test";
    // 测试0长度处理，应该不崩溃
    SilogFormatWallClockMs(0, timeStr, 0);
}

TEST(TimeTest, FormatWallClockMs_SmallBuffer)
{
    char timeStr[10];
    // 使用小缓冲区测试截断处理
    SilogFormatWallClockMs(SilogGetNowMs(), timeStr, sizeof(timeStr));
}

TEST(TimeTest, FormatWallClockMs_OverflowTimestamp)
{
    char timeStr[32] = {0};
    // 使用极大的时间戳（超过 INT64_MAX）测试溢出处理
    uint64_t hugeTs = (uint64_t)INT64_MAX + 1000;
    SilogFormatWallClockMs(hugeTs, timeStr, sizeof(timeStr));
    // 溢出时会返回空字符串或格式化失败，但不崩溃
    // 注意：实际行为取决于 localtime_r 如何处理
}

TEST(TimeTest, FormatWallClockMs_ZeroTimestamp)
{
    char timeStr[32];
    // 测试0时间戳
    SilogFormatWallClockMs(0, timeStr, sizeof(timeStr));
    // 应该成功格式化（1970-01-01 00:00:00.0000）
    EXPECT_GT(strlen(timeStr), 0);
}

TEST(TimeTest, GetMonoMsConsistency)
{
    // 测试单调时钟的一致性
    uint64_t start = SilogGetMonoMs();

    // 做一些工作
    volatile int sum = 0;
    for (int i = 0; i < 100000; i++) {
        sum += i;
    }

    uint64_t end = SilogGetMonoMs();

    // 单调时钟应该递增（或至少不减少）
    EXPECT_GE(end, start);
}

TEST(TimeTest, GetNowMsNotZero)
{
    // 测试当前时间不为0
    uint64_t now = SilogGetNowMs();
    EXPECT_GT(now, 0);

    // 应该是合理的时间（2020年之后，毫秒时间戳大于 1577836800000）
    EXPECT_GT(now, 1577836800000ULL);
}
