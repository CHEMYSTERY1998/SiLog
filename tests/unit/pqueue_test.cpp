#include "silog_pqueue.h"

#include <gtest/gtest.h>
#include <cstring>

// ==================== Priority Queue 测试 ====================

class PQueueTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        memset(&pqueue, 0, sizeof(pqueue));
        ASSERT_EQ(0,
                  SilogPQueueInit(&pqueue, sizeof(int), 16,
                                 [](const void* a, const void* b) -> int {
                                     return (*(int*)a - *(int*)b);
                                 }));
    }

    void TearDown() override { SilogPQueueDestroy(&pqueue); }

    SiLogPQueue pqueue;
};

TEST_F(PQueueTest, InitDestroy)
{
    // 初始化和销毁在 SetUp/TearDown 中完成
    EXPECT_EQ(pqueue.capacity, 16);
    EXPECT_EQ(pqueue.size, 0);
}

TEST_F(PQueueTest, PushPop)
{
    int value = 42;
    ASSERT_EQ(0, SilogPQueuePush(&pqueue, &value));

    EXPECT_EQ(pqueue.size, 1);

    int out;
    ASSERT_EQ(0, SilogPQueuePop(&pqueue, &out));
    EXPECT_EQ(out, 42);
    EXPECT_EQ(pqueue.size, 0);
}

TEST_F(PQueueTest, MaxHeapOrder)
{
    // 最大堆：大元素优先
    int values[] = {5, 3, 7, 1, 9};
    for (int v : values) {
        SilogPQueuePush(&pqueue, &v);
    }

    int out;
    SilogPQueuePop(&pqueue, &out);
    EXPECT_EQ(out, 9); // 最大值

    SilogPQueuePop(&pqueue, &out);
    EXPECT_EQ(out, 7);

    SilogPQueuePop(&pqueue, &out);
    EXPECT_EQ(out, 5);
}

TEST_F(PQueueTest, PopEmpty)
{
    int out;
    EXPECT_NE(0, SilogPQueuePop(&pqueue, &out));
}
