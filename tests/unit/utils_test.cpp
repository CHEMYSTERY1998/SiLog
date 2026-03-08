#include "silog_utils.h"

#include <gtest/gtest.h>
#include <sys/syscall.h>
#include <unistd.h>

// ==================== Utils 测试 ====================

TEST(UtilsTest, GetTid)
{
    pid_t tid = SilogUtilsGetTid();
    // 线程ID应该大于0
    EXPECT_GT(tid, 0);

    // 与当前线程ID比较
    pid_t sysTid = static_cast<pid_t>(syscall(SYS_gettid));
    EXPECT_EQ(tid, sysTid);
}

TEST(UtilsTest, LevelToName_ValidLevels)
{
    EXPECT_STREQ(SilogUtilsLevelToName(SILOG_DEBUG), "DEBUG");
    EXPECT_STREQ(SilogUtilsLevelToName(SILOG_INFO), "INFO");
    EXPECT_STREQ(SilogUtilsLevelToName(SILOG_WARN), "WARN");
    EXPECT_STREQ(SilogUtilsLevelToName(SILOG_ERROR), "ERROR");
    EXPECT_STREQ(SilogUtilsLevelToName(SILOG_FATAL), "FATAL");
}

TEST(UtilsTest, LevelToName_InvalidLevels)
{
    // 负数级别
    EXPECT_STREQ(SilogUtilsLevelToName((silogLevel)(-1)), "UNKNOWN");

    // 超过范围的级别
    EXPECT_STREQ(SilogUtilsLevelToName((silogLevel)99), "UNKNOWN");
    EXPECT_STREQ(SilogUtilsLevelToName((silogLevel)1000), "UNKNOWN");

    // 边界值：刚好超过最大有效值
    EXPECT_STREQ(SilogUtilsLevelToName((silogLevel)5), "UNKNOWN");
}

TEST(UtilsTest, LevelToName_BoundaryValues)
{
    // 测试边界值
    EXPECT_STREQ(SilogUtilsLevelToName((silogLevel)0), "DEBUG");  // 最小有效值
    EXPECT_STREQ(SilogUtilsLevelToName((silogLevel)4), "FATAL");  // 最大有效值
}

TEST(UtilsTest, LevelToName_Consistency)
{
    // 测试多次调用返回相同结果
    for (int i = 0; i < 5; i++) {
        EXPECT_STREQ(SilogUtilsLevelToName(SILOG_INFO), "INFO");
        EXPECT_STREQ(SilogUtilsLevelToName(SILOG_ERROR), "ERROR");
    }
}
