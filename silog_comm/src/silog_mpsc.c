#include "silog_mpsc.h"

#include <stdlib.h>
#include <string.h>

#include "silog_adapter.h"
#include "silog_error.h"

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
        return SILOG_OUT_OF_MEMORY;
    }

    logQueue->capacity = capacity;
    logQueue->elementSize = elementSize;

    atomic_store(&logQueue->writePos, 0);
    atomic_store(&logQueue->readPos, 0);

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
}

int32_t SilogMpscQueuePush(SiLogMpscQueue *logQueue, const void *element)
{
    if (logQueue == NULL || element == NULL) {
        return SILOG_INVALID_ARG;
    }

    if (logQueue->buffer == NULL || logQueue->capacity == 0) {
        return SILOG_TRANS_NOT_INIT;
    }

    unsigned int write = atomic_fetch_add(&logQueue->writePos, 1);
    unsigned int read = atomic_load(&logQueue->readPos);

    // 队列满：丢弃 newest
    if (write - read >= logQueue->capacity) {
        atomic_fetch_sub(&logQueue->writePos, 1); // 回滚
        return SILOG_TRANS_QUEUE_FULL;
    }

    int32_t index = write & (logQueue->capacity - 1);
    memcpy(logQueue->buffer + index * logQueue->elementSize, element, logQueue->elementSize);

    return SILOG_OK;
}

int32_t SilogMpscQueuePop(SiLogMpscQueue *logQueue, void *outElement)
{
    if (logQueue == NULL) {
        return SILOG_INVALID_ARG;
    }
    unsigned int read = atomic_load(&logQueue->readPos);
    unsigned int write = atomic_load(&logQueue->writePos);

    if (read == write) {
        return SILOG_TRANS_QUEUE_EMPTY; // 队列空
    }

    uint32_t index = read & (logQueue->capacity - 1);
    memcpy(outElement, (char *)logQueue->buffer + index * logQueue->elementSize, logQueue->elementSize);

    atomic_store(&logQueue->readPos, read + 1);
    return SILOG_OK;
}
