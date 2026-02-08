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
