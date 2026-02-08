#include "silog_mpsc.h"
#include "silog_time.h"
#include "silog_pqueue.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

// ==================== MPSC Queue 测试 ====================

class MpscQueueTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(0, SilogMpscQueueInit(&queue, sizeof(TestItem), 1024));
    }

    void TearDown() override { SilogMpscQueueDestroy(&queue); }

    typedef struct {
        int value;
    } TestItem;

    SiLogMpscQueue queue;
};

TEST_F(MpscQueueTest, InitDestroy)
{
    // 初始化和销毁在 SetUp/TearDown 中完成
    EXPECT_EQ(queue.capacity, 1024);
    EXPECT_EQ(queue.elementSize, sizeof(TestItem));
}

TEST_F(MpscQueueTest, PushPop)
{
    TestItem item = {42};
    ASSERT_EQ(0, SilogMpscQueuePush(&queue, &item));

    TestItem out;
    ASSERT_EQ(0, SilogMpscQueuePop(&queue, &out));
    EXPECT_EQ(out.value, 42);
}

TEST_F(MpscQueueTest, PushPopMultiple)
{
    const int count = 100;
    for (int i = 0; i < count; i++) {
        TestItem item = {i};
        ASSERT_EQ(0, SilogMpscQueuePush(&queue, &item));
    }

    for (int i = 0; i < count; i++) {
        TestItem out;
        ASSERT_EQ(0, SilogMpscQueuePop(&queue, &out));
        EXPECT_EQ(out.value, i);
    }
}

TEST_F(MpscQueueTest, PopEmpty)
{
    TestItem out;
    EXPECT_NE(0, SilogMpscQueuePop(&queue, &out));
}

TEST_F(MpscQueueTest, MultiThread)
{
    const int producerCount = 3;
    const int perThread = 100;
    std::atomic<int> totalPushed{0};
    std::vector<std::thread> producers;

    // 生产者线程
    for (int i = 0; i < producerCount; i++) {
        producers.emplace_back([this, i, perThread, &totalPushed]() {
            for (int j = 0; j < perThread; j++) {
                TestItem item = {i * 1000000 + j};
                if (SilogMpscQueuePush(&queue, &item) == 0) {
                    totalPushed++;
                }
            }
        });
    }

    // 消费者线程（主线程）
    int popCount = 0;
    TestItem out;
    int consecutiveEmpty = 0;

    while (consecutiveEmpty < 100) { // 连续 100 次空则认为结束
        if (SilogMpscQueuePop(&queue, &out) == 0) {
            popCount++;
            consecutiveEmpty = 0;
        } else {
            consecutiveEmpty++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    for (auto& t : producers) {
        t.join();
    }

    EXPECT_GT(popCount, 0);
    EXPECT_EQ(popCount, totalPushed);
}

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

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
