#include "silog_securec.h"

#include <gtest/gtest.h>
#include <cstring>
#include <string>

// ==================== 内存操作函数测试 ====================

TEST(SecurecTest, MemsetS_Basic)
{
    char buffer[16] = {0};  // 初始化为 0
    errno_t ret = memset_s(buffer, sizeof(buffer), 'A', 5);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(buffer[0], 'A');
    EXPECT_EQ(buffer[4], 'A');
    EXPECT_EQ(buffer[5], 0);  // 未设置的部分保持原值（0）
}

TEST(SecurecTest, MemsetS_NullDest)
{
    errno_t ret = memset_s(NULL, 16, 'A', 5);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, MemsetS_Overflow)
{
    char buffer[16];
    errno_t ret = memset_s(buffer, sizeof(buffer), 'A', 20);
    EXPECT_EQ(ret, ERANGE);
}

TEST(SecurecTest, MemcpyS_Basic)
{
    char src[] = "Hello";
    char dest[16];
    errno_t ret = memcpy_s(dest, sizeof(dest), src, strlen(src) + 1);
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello");
}

TEST(SecurecTest, MemcpyS_NullDest)
{
    char src[] = "Hello";
    errno_t ret = memcpy_s(NULL, 16, src, strlen(src));
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, MemcpyS_NullSrc)
{
    char dest[16];
    errno_t ret = memcpy_s(dest, sizeof(dest), NULL, 5);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, MemcpyS_Overflow)
{
    char src[] = "Hello World!";
    char dest[5];
    errno_t ret = memcpy_s(dest, sizeof(dest), src, strlen(src) + 1);
    EXPECT_EQ(ret, ERANGE);
}

TEST(SecurecTest, MemmoveS_Basic)
{
    char buffer[] = "Hello World";
    errno_t ret = memmove_s(buffer + 6, sizeof(buffer) - 6, buffer, 5);
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(buffer, "Hello Hello");
}

// ==================== 字符串复制函数测试 ====================

TEST(SecurecTest, StrcpyS_Basic)
{
    char dest[16];
    errno_t ret = strcpy_s(dest, sizeof(dest), "Hello");
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello");
}

TEST(SecurecTest, StrcpyS_NullDest)
{
    errno_t ret = strcpy_s(NULL, 16, "Hello");
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrcpyS_NullSrc)
{
    char dest[16];
    errno_t ret = strcpy_s(dest, sizeof(dest), NULL);
    EXPECT_EQ(ret, EINVAL);
    EXPECT_EQ(dest[0], '\0');
}

TEST(SecurecTest, StrcpyS_Truncate)
{
    char dest[5];
    errno_t ret = strcpy_s(dest, sizeof(dest), "Hello World");
    EXPECT_EQ(ret, ERANGE);
    EXPECT_EQ(dest[0], '\0');
}

TEST(SecurecTest, StrncpyS_Basic)
{
    char dest[16];
    errno_t ret = strncpy_s(dest, sizeof(dest), "Hello World", 5);
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello");
}

// ==================== 字符串连接函数测试 ====================

TEST(SecurecTest, StrcatS_Basic)
{
    char dest[16] = "Hello";
    errno_t ret = strcat_s(dest, sizeof(dest), " World");
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello World");
}

TEST(SecurecTest, StrcatS_Overflow)
{
    char dest[8] = "Hello";
    errno_t ret = strcat_s(dest, sizeof(dest), " World");
    EXPECT_EQ(ret, ERANGE);
}

TEST(SecurecTest, StrncatS_Basic)
{
    char dest[16] = "Hello";
    errno_t ret = strncat_s(dest, sizeof(dest), " World", 3);
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello Wo");
}

// ==================== 字符串比较/查找函数测试 ====================

TEST(SecurecTest, StrnlenS_Basic)
{
    size_t len = strnlen_s("Hello", 16);
    EXPECT_EQ(len, 5);
}

TEST(SecurecTest, StrnlenS_Null)
{
    size_t len = strnlen_s(NULL, 16);
    EXPECT_EQ(len, 0);
}

TEST(SecurecTest, StrcmpS_Equal)
{
    int result;
    errno_t ret = strcmp_s("Hello", 16, "Hello", 16, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(result, 0);
}

TEST(SecurecTest, StrcmpS_Less)
{
    int result;
    errno_t ret = strcmp_s("Apple", 16, "Banana", 16, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_LT(result, 0);
}

TEST(SecurecTest, StrcmpS_Greater)
{
    int result;
    errno_t ret = strcmp_s("Zebra", 16, "Apple", 16, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_GT(result, 0);
}

TEST(SecurecTest, StrchrS_Found)
{
    size_t pos;
    errno_t ret = strchr_s("Hello", 16, 'e', &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 1);
}

TEST(SecurecTest, StrchrS_NotFound)
{
    size_t pos;
    errno_t ret = strchr_s("Hello", 16, 'z', &pos);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrrchrS_Found)
{
    size_t pos;
    errno_t ret = strrchr_s("Hello", 16, 'l', &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 3);
}

TEST(SecurecTest, StrstrS_Found)
{
    size_t pos;
    errno_t ret = strstr_s("Hello World", 16, "World", 16, &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 6);
}

TEST(SecurecTest, StrstrS_NotFound)
{
    size_t pos;
    errno_t ret = strstr_s("Hello World", 16, "xyz", 16, &pos);
    EXPECT_EQ(ret, EINVAL);
}

// ==================== 字符串标记函数测试 ====================

TEST(SecurecTest, StrtokS_Basic)
{
    char str[] = "Hello,World,Test";
    char *context;
    char *token = strtok_s(str, ",", &context);
    EXPECT_STREQ(token, "Hello");

    token = strtok_s(NULL, ",", &context);
    EXPECT_STREQ(token, "World");

    token = strtok_s(NULL, ",", &context);
    EXPECT_STREQ(token, "Test");

    token = strtok_s(NULL, ",", &context);
    EXPECT_EQ(token, nullptr);
}

// ==================== 格式化输出函数测试 ====================

TEST(SecurecTest, SprintfS_Basic)
{
    char dest[16];
    int ret = sprintf_s(dest, sizeof(dest), "%s %d", "Test", 42);
    EXPECT_GT(ret, 0);
    EXPECT_STREQ(dest, "Test 42");
}

TEST(SecurecTest, SprintfS_NullDest)
{
    int ret = sprintf_s(NULL, 16, "Test");
    EXPECT_EQ(ret, -1);
}

TEST(SecurecTest, SnprintfS_Truncate)
{
    char dest[16];
    int ret = snprintf_s(dest, sizeof(dest), 5, "Hello World");
    EXPECT_GT(ret, 0);
    EXPECT_EQ(std::string(dest).length(), 5);  // 截断到 5 个字符
}

// ==================== 格式化输入函数测试 ====================

TEST(SecurecTest, SscanfS_Basic)
{
    const char *buffer = "42 3.14";
    int i;
    double d;
    int ret = sscanf_s(buffer, strlen(buffer) + 1, "%d %lf", &i, &d);
    EXPECT_EQ(ret, 2);
    EXPECT_EQ(i, 42);
    EXPECT_DOUBLE_EQ(d, 3.14);
}
