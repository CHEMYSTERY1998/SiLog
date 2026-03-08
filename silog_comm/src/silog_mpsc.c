#include "silog_mpsc.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "silog_adapter.h"
#include "silog_error.h"
#include "silog_prelog.h"
#include "silog_securec.h"

int32_t SilogMpscQueueInit(SiLogMpscQueue *logQueue, uint32_t elementSize, uint32_t capacity)
{
    if (logQueue == NULL || elementSize == 0 || capacity == 0) {
        return SILOG_INVALID_ARG;
    }

    if ((capacity & (capacity - 1)) != 0) {
        return SILOG_INVALID_ARG;
    }

    logQueue->buffer = SiMalloc(elementSize * capacity);
    if (!logQueue->buffer) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "MPSC queue memory allocation failed");
        return SILOG_OUT_OF_MEMORY;
    }

    logQueue->seq = SiMalloc(sizeof(atomic_uint) * capacity);
    if (!logQueue->seq) {
        SiFree(logQueue->buffer);
        logQueue->buffer = NULL;
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "MPSC queue seq allocation failed");
        return SILOG_OUT_OF_MEMORY;
    }

    logQueue->capacity = capacity;
    logQueue->capacityMask = capacity - 1;
    logQueue->elementSize = elementSize;

    atomic_store(&logQueue->writePos, 0);
    atomic_store(&logQueue->readPos, 0);

    // 初始化顺序标记：seq[i] 表示第 i 个槽位期望被第 seq[i] 个元素写入
    for (uint32_t i = 0; i < capacity; i++) {
        atomic_store(&logQueue->seq[i], i);
    }

    return SILOG_OK;
}

void SilogMpscQueueDestroy(SiLogMpscQueue *logQueue)
{
    if (logQueue == NULL) {
        return;
    }
    if (logQueue->buffer) {
        SiFree(logQueue->buffer);
        logQueue->buffer = NULL;
    }
    if (logQueue->seq) {
        SiFree(logQueue->seq);
        logQueue->seq = NULL;
    }
}

int32_t SilogMpscQueuePush(SiLogMpscQueue *logQueue, const void *element)
{
    if (logQueue == NULL || element == NULL) {
        return SILOG_INVALID_ARG;
    }

    if (logQueue->buffer == NULL || logQueue->capacity == 0 || logQueue->seq == NULL) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "MPSC queue not initialized");
        return SILOG_TRANS_NOT_INIT;
    }

    unsigned int write = atomic_fetch_add(&logQueue->writePos, 1);
    unsigned int read = atomic_load(&logQueue->readPos);

    // 队列满：丢弃 newest
    if (write - read >= logQueue->capacity) {
        atomic_fetch_sub(&logQueue->writePos, 1); // 回滚
        return SILOG_TRANS_QUEUE_FULL;
    }

    uint32_t index = write & logQueue->capacityMask;

    // 等待消费者读取完该槽位（seq[index] == write 表示可写入）
    unsigned int expected = write;
    unsigned int desired = write + logQueue->capacity;
    while (!atomic_compare_exchange_weak(&logQueue->seq[index], &expected, desired)) {
        if (expected == desired) {
            break; // 已经写入过了（不应该发生）
        }
        expected = write;
    }

    // 复制数据到缓冲区
    (void)memcpy_s(logQueue->buffer + index * logQueue->elementSize, logQueue->elementSize, element,
                   logQueue->elementSize);

    // 标记数据已就绪：seq[index] = write + 1 表示消费者可以读取
    atomic_store(&logQueue->seq[index], write + 1);

    return SILOG_OK;
}

int32_t SilogMpscQueuePop(SiLogMpscQueue *logQueue, void *outElement)
{
    if (logQueue == NULL || outElement == NULL) {
        return SILOG_INVALID_ARG;
    }

    if (logQueue->buffer == NULL || logQueue->seq == NULL) {
        return SILOG_TRANS_NOT_INIT;
    }

    unsigned int read = atomic_load(&logQueue->readPos);
    uint32_t index = read & logQueue->capacityMask;

    // 检查该槽位的数据是否已就绪（seq[index] == read + 1）
    unsigned int seq = atomic_load(&logQueue->seq[index]);
    if (seq != read + 1) {
        return SILOG_TRANS_QUEUE_EMPTY;
    }

    // 复制数据
    (void)memcpy_s(outElement, logQueue->elementSize, (char *)logQueue->buffer + index * logQueue->elementSize,
                   logQueue->elementSize);

    // 更新 seq 标记该槽位可再次使用
    atomic_store(&logQueue->seq[index], read + logQueue->capacity);

    // 更新读指针
    atomic_store(&logQueue->readPos, read + 1);

    return SILOG_OK;
}
