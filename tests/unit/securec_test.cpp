#include "silog_securec.h"

#include <cstring>
#include <gtest/gtest.h>
#include <string>

// ==================== 内存操作函数测试 ====================

TEST(SecurecTest, MemsetS_Basic)
{
    char buffer[16] = {0}; // 初始化为 0
    errno_t ret = memset_s(buffer, sizeof(buffer), 'A', 5);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(buffer[0], 'A');
    EXPECT_EQ(buffer[4], 'A');
    EXPECT_EQ(buffer[5], 0); // 未设置的部分保持原值（0）
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
    EXPECT_EQ(std::string(dest).length(), 5); // 截断到 5 个字符
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

// ==================== 额外测试以提高覆盖率 ====================

TEST(SecurecTest, StrncmpS_Basic)
{
    int result;
    errno_t ret = strncmp_s("Hello", 16, "Helium", 16, 3, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(result, 0);
}

TEST(SecurecTest, StrncmpS_Different)
{
    int result;
    errno_t ret = strncmp_s("Apple", 16, "Banana", 16, 5, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_LT(result, 0);
}

TEST(SecurecTest, StrncmpS_NullResult)
{
    errno_t ret = strncmp_s("Hello", 16, "World", 16, 3, NULL);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrncmpS_BothNull)
{
    int result;
    errno_t ret = strncmp_s(NULL, 0, NULL, 0, 3, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(result, 0);
}

TEST(SecurecTest, MemcpyS_OverlapForward)
{
    char buffer[] = "Hello World";
    // src < dest && src + count > dest 的情况
    errno_t ret = memcpy_s(buffer + 6, 5, buffer, 5);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(strncmp(buffer + 6, "Hello", 5), 0);
}

TEST(SecurecTest, MemcpyS_OverlapBackward)
{
    char buffer[] = "Hello World";
    // dest < src && dest + destMax > src 的情况
    errno_t ret = memcpy_s(buffer, 11, buffer + 6, 5);
    EXPECT_EQ(ret, EOK);
}

TEST(SecurecTest, MemmoveS_NullDest)
{
    char src[] = "Hello";
    errno_t ret = memmove_s(NULL, 16, src, 5);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, MemmoveS_NullSrc)
{
    char dest[16];
    errno_t ret = memmove_s(dest, sizeof(dest), NULL, 5);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, MemmoveS_Overflow)
{
    char src[] = "Hello World!";
    char dest[5];
    errno_t ret = memmove_s(dest, sizeof(dest), src, strlen(src));
    EXPECT_EQ(ret, ERANGE);
}

TEST(SecurecTest, StrncpyS_NullSrc)
{
    char dest[16] = "initial";
    errno_t ret = strncpy_s(dest, sizeof(dest), NULL, 5);
    EXPECT_EQ(ret, EINVAL);
    EXPECT_EQ(dest[0], '\0');
}

TEST(SecurecTest, StrncpyS_Overflow)
{
    char dest[5];
    errno_t ret = strncpy_s(dest, sizeof(dest), "Hello World", 20);
    EXPECT_EQ(ret, ERANGE);
    EXPECT_EQ(dest[0], '\0');
}

TEST(SecurecTest, StrcatS_NullSrc)
{
    char dest[16] = "Hello";
    errno_t ret = strcat_s(dest, sizeof(dest), NULL);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrncatS_NullSrc)
{
    char dest[16] = "Hello";
    errno_t ret = strncat_s(dest, sizeof(dest), NULL, 3);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrcmpS_NullStr1)
{
    int result;
    errno_t ret = strcmp_s(NULL, 0, "Hello", 16, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_LT(result, 0);
}

TEST(SecurecTest, StrcmpS_NullStr2)
{
    int result;
    errno_t ret = strcmp_s("Hello", 16, NULL, 0, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_GT(result, 0);
}

TEST(SecurecTest, StrcmpS_NullResult)
{
    errno_t ret = strcmp_s("Hello", 16, "World", 16, NULL);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrchrS_NullStr)
{
    size_t pos;
    errno_t ret = strchr_s(NULL, 16, 'a', &pos);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrrchrS_NullStr)
{
    size_t pos;
    errno_t ret = strrchr_s(NULL, 16, 'a', &pos);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrrchrS_NotFound)
{
    size_t pos;
    errno_t ret = strrchr_s("Hello", 16, 'z', &pos);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrstrS_NullStr)
{
    size_t pos;
    errno_t ret = strstr_s(NULL, 16, "sub", 16, &pos);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrstrS_NullSubstr)
{
    size_t pos;
    errno_t ret = strstr_s("Hello", 16, NULL, 16, &pos);
    EXPECT_EQ(ret, EINVAL);
}

TEST(SecurecTest, StrstrS_EmptySubstr)
{
    size_t pos;
    errno_t ret = strstr_s("Hello", 16, "", 0, &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 0);
}

TEST(SecurecTest, StrtokS_NullContext)
{
    char str[] = "Hello";
    char *token = strtok_s(str, ",", NULL);
    EXPECT_EQ(token, nullptr);
}

TEST(SecurecTest, StrtokS_NullDelimit)
{
    char str[] = "Hello";
    char *context;
    char *token = strtok_s(str, NULL, &context);
    EXPECT_EQ(token, nullptr);
}

TEST(SecurecTest, StrtokS_EmptyString)
{
    char str[] = "";
    char *context;
    char *token = strtok_s(str, ",", &context);
    EXPECT_EQ(token, nullptr);
}

TEST(SecurecTest, StrtokS_AllDelimiters)
{
    char str[] = ",,,";
    char *context;
    char *token = strtok_s(str, ",", &context);
    EXPECT_EQ(token, nullptr);
}

TEST(SecurecTest, SprintfS_NullFormat)
{
    char dest[16];
    int ret = sprintf_s(dest, sizeof(dest), NULL);
    EXPECT_EQ(ret, -1);
}

TEST(SecurecTest, SprintfS_Overflow)
{
    char dest[5];
    int ret = sprintf_s(dest, sizeof(dest), "Hello World");
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(dest[0], '\0');
}

TEST(SecurecTest, SnprintfS_NullFormat)
{
    char dest[16];
    int ret = snprintf_s(dest, sizeof(dest), 10, NULL);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(dest[0], '\0');
}

TEST(SecurecTest, SnprintfS_ZeroCount)
{
    char dest[16] = "initial";
    int ret = snprintf_s(dest, sizeof(dest), 0, "Hello");
    EXPECT_GE(ret, 0);
}

TEST(SecurecTest, SscanfS_NullBuffer)
{
    int ret = sscanf_s(NULL, 16, "%d");
    EXPECT_EQ(ret, EOF);
}

TEST(SecurecTest, SscanfS_NullFormat)
{
    char buffer[] = "42";
    int ret = sscanf_s(buffer, sizeof(buffer), NULL);
    EXPECT_EQ(ret, EOF);
}

TEST(SecurecTest, MemsetS_ZeroCount)
{
    char buffer[16] = "Hello";
    errno_t ret = memset_s(buffer, sizeof(buffer), 'A', 0);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(buffer[0], 'H');
}

TEST(SecurecTest, MemsetS_DestMaxZero)
{
    char buffer[16];
    errno_t ret = memset_s(buffer, 0, 'A', 5);
    EXPECT_EQ(ret, EINVAL);
}

// ==================== 文件操作函数测试 ====================

TEST(SecurecTest, FscanfS_NullStream)
{
    int ret = fscanf_s(NULL, "%d");
    EXPECT_EQ(ret, EOF);
}

TEST(SecurecTest, FscanfS_NullFormat)
{
    FILE *fp = tmpfile();
    ASSERT_NE(fp, nullptr);
    int ret = fscanf_s(fp, NULL);
    EXPECT_EQ(ret, EOF);
    fclose(fp);
}

TEST(SecurecTest, ScanfS_NullFormat)
{
    int ret = scanf_s(NULL);
    EXPECT_EQ(ret, EOF);
}

TEST(SecurecTest, SscanfS_NormalUse)
{
    const char *buffer = "42 test";
    int i;
    char str[16];
    int ret = sscanf_s(buffer, strlen(buffer) + 1, "%d %s", &i, str, sizeof(str));
    EXPECT_EQ(ret, 2);
    EXPECT_EQ(i, 42);
}

TEST(SecurecTest, SscanfS_IntegerOnly)
{
    const char *buffer = "123";
    int i;
    int ret = sscanf_s(buffer, strlen(buffer) + 1, "%d", &i);
    EXPECT_EQ(ret, 1);
    EXPECT_EQ(i, 123);
}

TEST(SecurecTest, VsscanfS_NullBuffer)
{
    int ret = vsscanf_s(NULL, 16, "%d", nullptr);
    EXPECT_EQ(ret, EOF);
}

TEST(SecurecTest, VsscanfS_NullFormat)
{
    char buffer[] = "42";
    int ret = vsscanf_s(buffer, sizeof(buffer), NULL, nullptr);
    EXPECT_EQ(ret, EOF);
}

TEST(SecurecTest, VfscanfS_NullStream)
{
    int ret = vfscanf_s(NULL, "%d", nullptr);
    EXPECT_EQ(ret, EOF);
}

TEST(SecurecTest, VfscanfS_NullFormat)
{
    FILE *fp = tmpfile();
    ASSERT_NE(fp, nullptr);
    int ret = vfscanf_s(fp, NULL, nullptr);
    EXPECT_EQ(ret, EOF);
    fclose(fp);
}

TEST(SecurecTest, VscanfS_NullFormat)
{
    int ret = vscanf_s(NULL, nullptr);
    EXPECT_EQ(ret, EOF);
}

TEST(SecurecTest, VsprintfS_NullDest)
{
    va_list args;
    int ret = vsprintf_s(NULL, 16, "%s", args);
    EXPECT_EQ(ret, -1);
}

TEST(SecurecTest, VsprintfS_NullFormat)
{
    char dest[16];
    va_list args;
    int ret = vsprintf_s(dest, sizeof(dest), NULL, args);
    EXPECT_EQ(ret, -1);
}

TEST(SecurecTest, VsnprintfS_NullDest)
{
    va_list args;
    int ret = vsnprintf_s(NULL, 16, 5, "%s", args);
    EXPECT_EQ(ret, -1);
}

TEST(SecurecTest, VsnprintfS_NullFormat)
{
    char dest[16];
    va_list args;
    int ret = vsnprintf_s(dest, sizeof(dest), 5, NULL, args);
    EXPECT_EQ(ret, -1);
}

// ==================== fscanf/vfscanf 测试 ====================


// ==================== vsprintf/vsnprintf 测试 ====================


TEST(SecurecTest, StrncpyS_CountZero)
{
    char dest[16] = "initial";
    errno_t ret = strncpy_s(dest, sizeof(dest), "Hello", 0);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(dest[0], '\0');
}

TEST(SecurecTest, StrncpyS_ExactFit)
{
    char dest[6];
    errno_t ret = strncpy_s(dest, sizeof(dest), "Hello", 5);
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello");
}

TEST(SecurecTest, StrncmpS_EqualStrings)
{
    int result;
    errno_t ret = strncmp_s("Hello", 16, "Hello", 16, 10, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(result, 0);
}

TEST(SecurecTest, StrncmpS_OneNull)
{
    int result;
    errno_t ret = strncmp_s(NULL, 0, "Hello", 16, 5, &result);
    EXPECT_EQ(ret, EOK);
    EXPECT_LT(result, 0);
}

TEST(SecurecTest, StrcatS_EmptyDest)
{
    char dest[16] = "";
    errno_t ret = strcat_s(dest, sizeof(dest), "Hello");
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello");
}

TEST(SecurecTest, StrcatS_EmptySrc)
{
    char dest[16] = "Hello";
    errno_t ret = strcat_s(dest, sizeof(dest), "");
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello");
}

TEST(SecurecTest, StrncatS_CountLargerThanSrc)
{
    char dest[16] = "Hello";
    errno_t ret = strncat_s(dest, sizeof(dest), "World", 100);
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "HelloWorld");
}

TEST(SecurecTest, MemcpyS_ExactSize)
{
    char src[] = "Hello";
    char dest[6];
    errno_t ret = memcpy_s(dest, sizeof(dest), src, strlen(src) + 1);
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(dest, "Hello");
}

TEST(SecurecTest, MemmoveS_SamePointer)
{
    char buffer[] = "Hello";
    errno_t ret = memmove_s(buffer, sizeof(buffer), buffer, strlen(buffer) + 1);
    EXPECT_EQ(ret, EOK);
    EXPECT_STREQ(buffer, "Hello");
}

TEST(SecurecTest, StrchrS_FirstChar)
{
    size_t pos;
    errno_t ret = strchr_s("Hello", 16, 'H', &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 0);
}

TEST(SecurecTest, StrrchrS_LastChar)
{
    size_t pos;
    errno_t ret = strrchr_s("Hello", 16, 'o', &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 4);
}

TEST(SecurecTest, StrrchrS_MultipleOccurrences)
{
    size_t pos;
    errno_t ret = strrchr_s("lollipop", 16, 'l', &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 3);
}

TEST(SecurecTest, StrstrS_AtEnd)
{
    size_t pos;
    errno_t ret = strstr_s("Hello World", 16, "World", 16, &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 6);
}

TEST(SecurecTest, StrstrS_AtBeginning)
{
    size_t pos;
    errno_t ret = strstr_s("Hello World", 16, "Hello", 16, &pos);
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(pos, 0);
}

TEST(SecurecTest, StrtokS_SingleToken)
{
    char str[] = "Hello";
    char *context;
    char *token = strtok_s(str, ",", &context);
    EXPECT_STREQ(token, "Hello");
    EXPECT_EQ(strtok_s(NULL, ",", &context), nullptr);
}

TEST(SecurecTest, StrtokS_EmptyContext)
{
    char str[] = "Hello,World";
    char *context = NULL;
    // 第一次调用，context 为 NULL
    char *token = strtok_s(str, ",", &context);
    EXPECT_STREQ(token, "Hello");
    // 继续分割
    token = strtok_s(NULL, ",", &context);
    EXPECT_STREQ(token, "World");
}

TEST(SecurecTest, SprintfS_EmptyString)
{
    char dest[16];
    int ret = sprintf_s(dest, sizeof(dest), "%s", "");
    EXPECT_GE(ret, 0);
    EXPECT_STREQ(dest, "");
}

TEST(SecurecTest, SprintfS_Integers)
{
    char dest[32];
    int ret = sprintf_s(dest, sizeof(dest), "%d %u %x", -42, 42U, 0xAB);
    EXPECT_GT(ret, 0);
    EXPECT_NE(strstr(dest, "-42"), nullptr);
}

TEST(SecurecTest, SnprintfS_TruncateExact)
{
    char dest[6];
    int ret = snprintf_s(dest, sizeof(dest), 5, "Hello");
    EXPECT_GE(ret, 0);
    EXPECT_EQ(strlen(dest), 5);
}
