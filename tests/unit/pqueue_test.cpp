#include "silog_error.h"
#include "silog_pqueue.h"
#include "silog_securec.h"

#include <cstring>
#include <gtest/gtest.h>

// ==================== Priority Queue 测试 ====================

class PQueueTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        (void)memset_s(&pqueue, sizeof(pqueue), 0, sizeof(pqueue));
        ASSERT_EQ(SILOG_OK, SilogPQueueInit(&pqueue, sizeof(int), 16, [](const void *a, const void *b) -> int {
                      return (*(int *)a - *(int *)b);
                  }));
    }

    void TearDown() override { SilogPQueueDestroy(&pqueue); }

    SiLogPQueue pqueue;
};

TEST_F(PQueueTest, InitDestroy)
{
    /* 初始化和销毁在 SetUp/TearDown 中完成 */
    EXPECT_EQ(pqueue.capacity, 16);
    EXPECT_EQ(pqueue.size, 0);
}

TEST_F(PQueueTest, PushPop)
{
    int value = 42;
    ASSERT_EQ(SILOG_OK, SilogPQueuePush(&pqueue, &value));

    EXPECT_EQ(pqueue.size, 1);

    int out;
    ASSERT_EQ(SILOG_OK, SilogPQueuePop(&pqueue, &out));
    EXPECT_EQ(out, 42);
    EXPECT_EQ(pqueue.size, 0);
}

TEST_F(PQueueTest, MaxHeapOrder)
{
    /* 最大堆：大元素优先 */
    int values[] = {5, 3, 7, 1, 9};
    for (int v : values) {
        SilogPQueuePush(&pqueue, &v);
    }

    int out;
    SilogPQueuePop(&pqueue, &out);
    EXPECT_EQ(out, 9); /* 最大值 */

    SilogPQueuePop(&pqueue, &out);
    EXPECT_EQ(out, 7);

    SilogPQueuePop(&pqueue, &out);
    EXPECT_EQ(out, 5);
}

TEST_F(PQueueTest, PopEmpty)
{
    int out;
    EXPECT_NE(SILOG_OK, SilogPQueuePop(&pqueue, &out));
}

TEST_F(PQueueTest, Peek)
{
    int value = 42;
    ASSERT_EQ(SILOG_OK, SilogPQueuePush(&pqueue, &value));

    int out;
    ASSERT_EQ(SILOG_OK, SilogPQueuePeek(&pqueue, &out));
    EXPECT_EQ(out, 42);
    EXPECT_EQ(SilogPQueueSize(&pqueue), 1); // 元素还在
}

TEST_F(PQueueTest, PeekEmpty)
{
    int out;
    EXPECT_NE(SILOG_OK, SilogPQueuePeek(&pqueue, &out));
}

TEST_F(PQueueTest, IsEmpty)
{
    EXPECT_TRUE(SilogPQueueIsEmpty(&pqueue));
    int value = 1;
    SilogPQueuePush(&pqueue, &value);
    EXPECT_FALSE(SilogPQueueIsEmpty(&pqueue));
}

TEST_F(PQueueTest, IsFull)
{
    // 填满队列
    for (int i = 0; i < 16; i++) {
        EXPECT_FALSE(SilogPQueueIsFull(&pqueue));
        SilogPQueuePush(&pqueue, &i);
    }
    EXPECT_TRUE(SilogPQueueIsFull(&pqueue));
}

TEST_F(PQueueTest, Clear)
{
    int value = 42;
    SilogPQueuePush(&pqueue, &value);
    SilogPQueuePush(&pqueue, &value);
    EXPECT_EQ(SilogPQueueSize(&pqueue), 2);

    SilogPQueueClear(&pqueue);
    EXPECT_EQ(SilogPQueueSize(&pqueue), 0);
    EXPECT_TRUE(SilogPQueueIsEmpty(&pqueue));
}

TEST_F(PQueueTest, SizeAndCapacity)
{
    EXPECT_EQ(SilogPQueueCapacity(&pqueue), 16);
    EXPECT_EQ(SilogPQueueSize(&pqueue), 0);

    int value = 1;
    SilogPQueuePush(&pqueue, &value);
    EXPECT_EQ(SilogPQueueSize(&pqueue), 1);
}

TEST_F(PQueueTest, Reserve)
{
    int value = 42;
    SilogPQueuePush(&pqueue, &value);

    ASSERT_EQ(SILOG_OK, SilogPQueueReserve(&pqueue, 32));
    EXPECT_EQ(SilogPQueueCapacity(&pqueue), 32);
    EXPECT_EQ(SilogPQueueSize(&pqueue), 1);

    // 验证元素还在
    int out;
    ASSERT_EQ(SILOG_OK, SilogPQueuePeek(&pqueue, &out));
    EXPECT_EQ(out, 42);
}

TEST_F(PQueueTest, ReserveSmallerCapacity)
{
    // 尝试缩小容量应该失败
    EXPECT_NE(SILOG_OK, SilogPQueueReserve(&pqueue, 8));
}

TEST_F(PQueueTest, PushFull)
{
    // 填满队列
    for (int i = 0; i < 16; i++) {
        SilogPQueuePush(&pqueue, &i);
    }

    // 尝试再推一个应该失败
    int value = 99;
    EXPECT_NE(SILOG_OK, SilogPQueuePush(&pqueue, &value));
}

TEST_F(PQueueTest, NullQueue)
{
    EXPECT_TRUE(SilogPQueueIsEmpty(NULL));
    EXPECT_TRUE(SilogPQueueIsFull(NULL));
    EXPECT_EQ(SilogPQueueSize(NULL), 0);
    EXPECT_EQ(SilogPQueueCapacity(NULL), 0);

    SiLogPQueue queue;
    EXPECT_NE(SILOG_OK, SilogPQueueInit(&queue, 0, 16, [](const void *a, const void *b) -> int {
                  return (*(int *)a - *(int *)b);
              }));
}

TEST_F(PQueueTest, MinHeapOrder)
{
    // 使用最小堆比较函数
    SiLogPQueue minQueue;
    ASSERT_EQ(SILOG_OK, SilogPQueueInit(&minQueue, sizeof(int), 16, [](const void *a, const void *b) -> int {
                  return (*(int *)b - *(int *)a); // 反向比较
              }));

    int values[] = {5, 3, 7, 1, 9};
    for (int v : values) {
        SilogPQueuePush(&minQueue, &v);
    }

    int out;
    SilogPQueuePop(&minQueue, &out);
    EXPECT_EQ(out, 1); // 最小值

    SilogPQueuePop(&minQueue, &out);
    EXPECT_EQ(out, 3);

    SilogPQueueDestroy(&minQueue);
}

TEST_F(PQueueTest, PushPopNull)
{
    int value = 42;
    int out;

    EXPECT_NE(SILOG_OK, SilogPQueuePush(NULL, &value));
    EXPECT_NE(SILOG_OK, SilogPQueuePush(&pqueue, NULL));
    EXPECT_NE(SILOG_OK, SilogPQueuePop(NULL, &out));
    EXPECT_NE(SILOG_OK, SilogPQueuePop(&pqueue, NULL));
    EXPECT_NE(SILOG_OK, SilogPQueuePeek(NULL, &out));
    EXPECT_NE(SILOG_OK, SilogPQueuePeek(&pqueue, NULL));
}

TEST_F(PQueueTest, SingleElement)
{
    // 测试只有一个元素的情况
    int value = 42;
    ASSERT_EQ(SILOG_OK, SilogPQueuePush(&pqueue, &value));

    int out;
    ASSERT_EQ(SILOG_OK, SilogPQueuePop(&pqueue, &out));
    EXPECT_EQ(out, 42);
    EXPECT_TRUE(SilogPQueueIsEmpty(&pqueue));
}

TEST_F(PQueueTest, DestroyNull)
{
    // 测试销毁空队列不崩溃
    SilogPQueueDestroy(NULL);
}

TEST_F(PQueueTest, ClearNull)
{
    // 测试清空空队列不崩溃
    SilogPQueueClear(NULL);
}
