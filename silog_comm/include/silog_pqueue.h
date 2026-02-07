#ifndef SILOG_PQUEUE_H
#define SILOG_PQUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 比较函数类型
 * @param a 第一个元素
 * @param b 第二个元素
 * @return < 0: a < b, = 0: a == b, > 0: a > b
 *         对于大根堆：返回 a - b（a 大则返回正）
 *         对于小根堆：返回 b - a（a 小则返回正）
 */
typedef int (*SiLogPQueueCompareFunc)(const void *a, const void *b);

typedef struct {
    uint8_t *buffer;     // 动态分配的缓冲区
    uint32_t elementSize; // 每个元素大小（字节）
    uint32_t capacity;    // 队列容量
    uint32_t size;        // 当前元素个数
    SiLogPQueueCompareFunc compare; // 比较函数
} SiLogPQueue;

// 初始化优先队列
int32_t SilogPQueueInit(SiLogPQueue *queue, uint32_t elementSize, uint32_t capacity,
                        SiLogPQueueCompareFunc compare);

// 销毁优先队列
void SilogPQueueDestroy(SiLogPQueue *queue);

// 清空队列
void SilogPQueueClear(SiLogPQueue *queue);

// 判断队列是否为空
bool SilogPQueueIsEmpty(const SiLogPQueue *queue);

// 判断队列是否已满
bool SilogPQueueIsFull(const SiLogPQueue *queue);

// 获取队列中元素个数
uint32_t SilogPQueueSize(const SiLogPQueue *queue);

// 获取队列容量
uint32_t SilogPQueueCapacity(const SiLogPQueue *queue);

// 插入元素
int32_t SilogPQueuePush(SiLogPQueue *queue, const void *element);

// 删除并返回堆顶元素
int32_t SilogPQueuePop(SiLogPQueue *queue, void *outElement);

// 获取堆顶元素（不删除）
int32_t SilogPQueuePeek(const SiLogPQueue *queue, void *outElement);

// 扩容（如果需要）
int32_t SilogPQueueReserve(SiLogPQueue *queue, uint32_t newCapacity);

#ifdef __cplusplus
}
#endif

#endif // SILOG_PQUEUE_H
