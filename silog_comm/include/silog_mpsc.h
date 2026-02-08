#ifndef SILOG_MPSC_H
#define SILOG_MPSC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#include <atomic>
using atomic_uint = std::atomic_uint;
extern "C" {
#else
#include <stdatomic.h>
#endif

typedef struct {
    uint8_t *buffer;      // 环形缓冲区
    uint32_t elementSize; // 每个元素大小
    uint32_t capacity;    // 队列容量（必须是 2 的幂）
    atomic_uint writePos; // 多生产者写指针
    atomic_uint readPos;  // 单消费者读指针
} SiLogMpscQueue;

// 初始化队列（capacity 必须是 2 的幂）
int32_t SilogMpscQueueInit(SiLogMpscQueue *logQueue, uint32_t elementSize, uint32_t capacity);

// 销毁队列
void SilogMpscQueueDestroy(SiLogMpscQueue *logQueue);

// push：多生产者（永不阻塞）
int32_t SilogMpscQueuePush(SiLogMpscQueue *logQueue, const void *element);

// pop：单消费者
int32_t SilogMpscQueuePop(SiLogMpscQueue *logQueue, void *outElement);

#ifdef __cplusplus
}
#endif

#endif // SILOG_MPSC_H
