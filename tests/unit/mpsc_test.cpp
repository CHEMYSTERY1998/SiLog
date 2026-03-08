#include "silog_mpsc.h"

#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

// ==================== MPSC Queue 测试 ====================

class MpscQueueTest : public ::testing::Test {
  protected:
    void SetUp() override { ASSERT_EQ(0, SilogMpscQueueInit(&queue, sizeof(TestItem), 1024)); }

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

    for (auto &t : producers) {
        t.join();
    }

    EXPECT_GT(popCount, 0);
    EXPECT_EQ(popCount, totalPushed);
}

TEST_F(MpscQueueTest, NullArguments)
{
    TestItem item = {42};
    TestItem out;

    // Push with NULL queue
    EXPECT_NE(SilogMpscQueuePush(NULL, &item), 0);

    // Push with NULL element
    EXPECT_NE(SilogMpscQueuePush(&queue, NULL), 0);

    // Pop with NULL queue
    EXPECT_NE(SilogMpscQueuePop(NULL, &out), 0);

    // Pop with NULL out element
    EXPECT_NE(SilogMpscQueuePop(&queue, NULL), 0);
}

TEST_F(MpscQueueTest, QueueFull)
{
    // 填满队列（容量 1024）
    int pushed = 0;
    for (int i = 0; i < 1024; i++) {
        TestItem item = {i};
        if (SilogMpscQueuePush(&queue, &item) == 0) {
            pushed++;
        } else {
            break;
        }
    }

    // 队列已满，再推应该失败
    TestItem item = {9999};
    EXPECT_NE(SilogMpscQueuePush(&queue, &item), 0);

    // 弹出一个
    TestItem out;
    EXPECT_EQ(SilogMpscQueuePop(&queue, &out), 0);

    // 现在可以推入了
    EXPECT_EQ(SilogMpscQueuePush(&queue, &item), 0);
}

TEST_F(MpscQueueTest, InitInvalidArgs)
{
    SiLogMpscQueue q;

    // NULL queue
    EXPECT_NE(SilogMpscQueueInit(NULL, sizeof(TestItem), 1024), 0);

    // Zero element size
    EXPECT_NE(SilogMpscQueueInit(&q, 0, 1024), 0);

    // Zero capacity
    EXPECT_NE(SilogMpscQueueInit(&q, sizeof(TestItem), 0), 0);

    // Non-power-of-2 capacity
    EXPECT_NE(SilogMpscQueueInit(&q, sizeof(TestItem), 100), 0);
}

TEST_F(MpscQueueTest, DestroyNull)
{
    // 销毁 NULL 队列应该安全
    SilogMpscQueueDestroy(NULL);
}

TEST_F(MpscQueueTest, SingleProducerSingleConsumer)
{
    const int count = 500;
    std::thread producer([this, count]() {
        for (int i = 0; i < count; i++) {
            TestItem item = {i};
            while (SilogMpscQueuePush(&queue, &item) != 0) {
                std::this_thread::yield();
            }
        }
    });

    int received = 0;
    int expected = 0;
    while (expected < count) {
        TestItem out;
        if (SilogMpscQueuePop(&queue, &out) == 0) {
            EXPECT_EQ(out.value, expected);
            expected++;
            received++;
        }
    }

    producer.join();
    EXPECT_EQ(received, count);
}

TEST_F(MpscQueueTest, PopFromEmptyThenPush)
{
    TestItem out;

    // 先尝试弹出（空队列）
    EXPECT_NE(SilogMpscQueuePop(&queue, &out), 0);

    // 然后推入
    TestItem item = {42};
    EXPECT_EQ(SilogMpscQueuePush(&queue, &item), 0);

    // 现在可以弹出
    EXPECT_EQ(SilogMpscQueuePop(&queue, &out), 0);
    EXPECT_EQ(out.value, 42);
}

TEST_F(MpscQueueTest, WrapAround)
{
    // 测试环形缓冲区的回绕
    const int capacity = 1024;

    // 填满队列
    for (int i = 0; i < capacity; i++) {
        TestItem item = {i};
        ASSERT_EQ(SilogMpscQueuePush(&queue, &item), 0);
    }

    // 弹出 512 个
    TestItem out;
    for (int i = 0; i < 512; i++) {
        ASSERT_EQ(SilogMpscQueuePop(&queue, &out), 0);
        EXPECT_EQ(out.value, i);
    }

    // 再推入 512 个（应该回绕）
    for (int i = 0; i < 512; i++) {
        TestItem item = {10000 + i};
        ASSERT_EQ(SilogMpscQueuePush(&queue, &item), 0);
    }

    // 验证顺序
    for (int i = 512; i < capacity; i++) {
        ASSERT_EQ(SilogMpscQueuePop(&queue, &out), 0);
        EXPECT_EQ(out.value, i);
    }
    for (int i = 0; i < 512; i++) {
        ASSERT_EQ(SilogMpscQueuePop(&queue, &out), 0);
        EXPECT_EQ(out.value, 10000 + i);
    }
}
