#include "silog_mpsc.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

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
