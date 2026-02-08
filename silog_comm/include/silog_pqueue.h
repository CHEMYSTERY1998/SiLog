/**
 * @file silog_pqueue.h
 * @brief SiLog 优先队列（堆）实现
 */

#ifndef SILOG_PQUEUE_H
#define SILOG_PQUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 优先队列比较函数类型
 * @param a 第一个元素
 * @param b 第二个元素
 * @return < 0: a < b, = 0: a == b, > 0: a > b
 * @note 对于大根堆：返回 a - b（a 大则返回正）
 * @note 对于小根堆：返回 b - a（a 小则返回正）
 */
typedef int (*SiLogPQueueCompareFunc)(const void *a, const void *b);

/**
 * @brief 优先队列结构
 */
typedef struct {
    uint8_t *buffer;                ///< 动态分配的缓冲区
    uint32_t elementSize;           ///< 每个元素大小（字节）
    uint32_t capacity;              ///< 队列容量
    uint32_t size;                  ///< 当前元素个数
    SiLogPQueueCompareFunc compare; ///< 比较函数
} SiLogPQueue;

/**
 * @brief 初始化优先队列
 * @param queue 队列指针
 * @param elementSize 每个元素大小
 * @param capacity 队列容量
 * @param compare 比较函数
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogPQueueInit(SiLogPQueue *queue, uint32_t elementSize, uint32_t capacity, SiLogPQueueCompareFunc compare);

/**
 * @brief 销毁优先队列
 * @param queue 队列指针
 */
void SilogPQueueDestroy(SiLogPQueue *queue);

/**
 * @brief 清空队列
 * @param queue 队列指针
 */
void SilogPQueueClear(SiLogPQueue *queue);

/**
 * @brief 判断队列是否为空
 * @param queue 队列指针
 * @return true 队列为空，false 队列非空
 */
bool SilogPQueueIsEmpty(const SiLogPQueue *queue);

/**
 * @brief 判断队列是否已满
 * @param queue 队列指针
 * @return true 队列已满，false 队列未满
 */
bool SilogPQueueIsFull(const SiLogPQueue *queue);

/**
 * @brief 获取队列中元素个数
 * @param queue 队列指针
 * @return uint32_t 元素个数
 */
uint32_t SilogPQueueSize(const SiLogPQueue *queue);

/**
 * @brief 获取队列容量
 * @param queue 队列指针
 * @return uint32_t 队列容量
 */
uint32_t SilogPQueueCapacity(const SiLogPQueue *queue);

/**
 * @brief 插入元素
 * @param queue 队列指针
 * @param element 要插入的元素
 * @return 成功返回 SILOG_OK，队列满返回 SILOG_BUSY
 */
int32_t SilogPQueuePush(SiLogPQueue *queue, const void *element);

/**
 * @brief 删除并返回堆顶元素
 * @param queue 队列指针
 * @param outElement 输出元素缓冲区
 * @return 成功返回 SILOG_OK，队列空返回 SILOG_TRANS_QUEUE_EMPTY
 */
int32_t SilogPQueuePop(SiLogPQueue *queue, void *outElement);

/**
 * @brief 获取堆顶元素（不删除）
 * @param queue 队列指针
 * @param outElement 输出元素缓冲区
 * @return 成功返回 SILOG_OK，队列空返回 SILOG_TRANS_QUEUE_EMPTY
 */
int32_t SilogPQueuePeek(const SiLogPQueue *queue, void *outElement);

/**
 * @brief 扩容
 * @param queue 队列指针
 * @param newCapacity 新容量
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogPQueueReserve(SiLogPQueue *queue, uint32_t newCapacity);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_PQUEUE_H */
