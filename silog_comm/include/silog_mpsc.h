/**
 * @file silog_mpsc.h
 * @brief SiLog 多生产者单消费者（MPSC）无锁队列
 */

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

/**
 * @brief MPSC 无锁队列
 *
 * 多生产者单消费者队列，用于高并发场景下的线程安全数据传递
 * @note 队列容量必须是 2 的幂
 */
typedef struct {
    uint8_t *buffer;      ///< 环形缓冲区
    uint32_t elementSize; ///< 每个元素大小
    uint32_t capacity;    ///< 队列容量（必须是 2 的幂）
    atomic_uint writePos; ///< 多生产者写指针
    atomic_uint readPos;  ///< 单消费者读指针
} SiLogMpscQueue;

/**
 * @brief 初始化 MPSC 队列
 * @param logQueue 队列指针
 * @param elementSize 每个元素大小
 * @param capacity 队列容量（必须是 2 的幂）
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogMpscQueueInit(SiLogMpscQueue *logQueue, uint32_t elementSize, uint32_t capacity);

/**
 * @brief 销毁 MPSC 队列
 * @param logQueue 队列指针
 */
void SilogMpscQueueDestroy(SiLogMpscQueue *logQueue);

/**
 * @brief 向队列推送元素（多生产者，永不阻塞）
 * @param logQueue 队列指针
 * @param element 要推送的元素指针
 * @return 成功返回 SILOG_OK，队列满返回 SILOG_TRANS_QUEUE_FULL
 */
int32_t SilogMpscQueuePush(SiLogMpscQueue *logQueue, const void *element);

/**
 * @brief 从队列弹出元素（单消费者）
 * @param logQueue 队列指针
 * @param outElement 输出元素缓冲区
 * @return 成功返回 SILOG_OK，队列空返回 SILOG_TRANS_QUEUE_EMPTY
 */
int32_t SilogMpscQueuePop(SiLogMpscQueue *logQueue, void *outElement);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_MPSC_H */
