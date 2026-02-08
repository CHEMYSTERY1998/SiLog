#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "silog_mpsc.h" // 你的头文件

#define PRODUCER_COUNT  3
#define PER_THREAD_PUSH 1000

typedef struct {
    int value;
} TestItem;

typedef struct {
    SiLogMpscQueue *queue;
    int threadId;
} ProducerArg;

void *producerThread(void *arg)
{
    ProducerArg *pa = (ProducerArg *)arg;
    TestItem item;

    for (int i = 0; i < PER_THREAD_PUSH; i++) {
        item.value = pa->threadId * 1000000 + i; // 唯一值
        int ret = SilogMpscQueuePush(pa->queue, &item);
        if (ret != 0) {
            // 队列满时可能失败，这里不算错误
            printf("Producer %d: Queue full, item %d dropped\n", pa->threadId, item.value);
        } else {
            // printf("Producer %d: Pushed item %d\n", pa->threadId, item.value);
        }
    }
    return NULL;
}

int main()
{
    printf("=== SilogMpscQueue 测试开始 ===\n");
    printf("Push 总数: %d\n", PRODUCER_COUNT * PER_THREAD_PUSH);

    SiLogMpscQueue queue;
    uint32_t elementSize = sizeof(TestItem);
    uint32_t capacity = 1024 * 128 * 8 * 8; // 小容量，方便测试满队列情况

    // 初始化
    if (SilogMpscQueueInit(&queue, elementSize, capacity) != 0) {
        printf("初始化失败\n");
        return -1;
    }
    printf("初始化成功\n");

    // 启动多个生产者线程
    pthread_t producers[PRODUCER_COUNT];
    ProducerArg args[PRODUCER_COUNT];

    for (int i = 0; i < PRODUCER_COUNT; i++) {
        args[i].queue = &queue;
        args[i].threadId = i;
        pthread_create(&producers[i], NULL, producerThread, &args[i]);
    }

    // 消费线程（主线程）
    int popCount = 0;
    TestItem item;

    // 等待生产者结束前持续 pop
    int joined[PRODUCER_COUNT] = {0};
    while (1) {
        if (SilogMpscQueuePop(&queue, &item) == 0) {
            popCount++;
        } else {
            printf("队列空，等待生产者...\n");
            usleep(100000); // 100ms
        }

        int alive = 0;
        for (int i = 0; i < PRODUCER_COUNT; i++) {
            if (joined[i]) {
                continue;
            }
            int rc = pthread_tryjoin_np(producers[i], NULL);
            if (rc == 0) {
                joined[i] = 1; // 记录已回收
            } else if (rc == EBUSY) {
                alive = 1;
            }
        }

        if (!alive)
            break;
    }
    printf("Pop 总数: %d\n", popCount);

    // 销毁
    SilogMpscQueueDestroy(&queue);
    printf("销毁成功\n");

    printf("=== 测试结束 ===\n");
    return 0;
}
