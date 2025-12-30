#include "silog_mpsc.h"

#include <stdlib.h>
#include <string.h>

#include "silog_adapter.h"
#include "silog_error.h"

typedef struct {
    uint8_t *buffer;      // 环形缓冲区
    uint32_t elementSize; // 每个元素大小
    uint32_t capacity;    // 队列容量（必须是 2 的幂）
    atomic_uint writePos; // 多生产者写指针
    atomic_uint readPos;  // 单消费者读指针
} SiLogMpscQueue;

SiLogMpscQueue g_LogQueue = {0};

int32_t SilogMpscQueueInit(uint32_t elementSize, uint32_t capacity)
{
    if (elementSize == 0 || capacity == 0) {
        return SILOG_INVALID_ARG;
    }

    // capacity 必须是 2 的幂
    if ((capacity & (capacity - 1)) != 0) {
        return SILOG_INVALID_ARG;
    }

    g_LogQueue.buffer = SiMalloc(elementSize * capacity);
    if (!g_LogQueue.buffer) {
        return SILOG_OUT_OF_MEMORY;
    }

    g_LogQueue.elementSize = elementSize;
    g_LogQueue.capacity = capacity;

    atomic_store(&g_LogQueue.writePos, 0);
    atomic_store(&g_LogQueue.readPos, 0);
    return SILOG_OK;
}

void SilogMpscQueueDestroy()
{
    if (g_LogQueue.buffer) {
        free(g_LogQueue.buffer);
        g_LogQueue.buffer = NULL;
    }
}

int32_t SilogMpscQueuePush(const void *element)
{
    unsigned int write = atomic_fetch_add(&g_LogQueue.writePos, 1);
    unsigned int read = atomic_load(&g_LogQueue.readPos);

    // 队列满：丢弃 newest
    if (write - read >= g_LogQueue.capacity) {
        atomic_fetch_sub(&g_LogQueue.writePos, 1); // 回滚
        return SILOG_TRANS_QUEUE_FULL;
    }

    int32_t index = write & (g_LogQueue.capacity - 1);
    memcpy(g_LogQueue.buffer + index * g_LogQueue.elementSize, element, g_LogQueue.elementSize);

    return SILOG_OK;
}

int32_t SilogMpscQueuePop(void *outElement)
{
    unsigned int read = atomic_load(&g_LogQueue.readPos);
    unsigned int write = atomic_load(&g_LogQueue.writePos);

    if (read == write) {
        return SILOG_TRANS_QUEUE_EMPTY; // 队列空
    }

    uint32_t index = read & (g_LogQueue.capacity - 1);
    memcpy(outElement, (char *)g_LogQueue.buffer + index * g_LogQueue.elementSize, g_LogQueue.elementSize);

    atomic_store(&g_LogQueue.readPos, read + 1);
    return SILOG_OK;
}
