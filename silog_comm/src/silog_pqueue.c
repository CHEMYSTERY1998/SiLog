#include "silog_pqueue.h"
#include "silog_adapter.h"
#include "silog_error.h"

#include <string.h>

// 初始化优先队列
int32_t SilogPQueueInit(SiLogPQueue *queue, uint32_t elementSize, uint32_t capacity,
                        SiLogPQueueCompareFunc compare)
{
    if (queue == NULL || elementSize == 0 || capacity == 0 || compare == NULL) {
        return SILOG_INVALID_ARG;
    }

    queue->buffer = (uint8_t *)SiMalloc(elementSize * capacity);
    if (queue->buffer == NULL) {
        return SILOG_OUT_OF_MEMORY;
    }

    queue->elementSize = elementSize;
    queue->capacity = capacity;
    queue->size = 0;
    queue->compare = compare;

    return SILOG_OK;
}

// 销毁优先队列
void SilogPQueueDestroy(SiLogPQueue *queue)
{
    if (queue != NULL) {
        SiFree(queue->buffer);
        queue->buffer = NULL;
        queue->size = 0;
        queue->capacity = 0;
    }
}

// 清空队列
void SilogPQueueClear(SiLogPQueue *queue)
{
    if (queue != NULL) {
        queue->size = 0;
    }
}

// 判断队列是否为空
bool SiLogPQueueIsEmpty(const SiLogPQueue *queue)
{
    return (queue == NULL) ? true : (queue->size == 0);
}

// 判断队列是否已满
bool SiLogPQueueIsFull(const SiLogPQueue *queue)
{
    return (queue == NULL) ? true : (queue->size >= queue->capacity);
}

// 获取队列中元素个数
uint32_t SiLogPQueueSize(const SiLogPQueue *queue)
{
    return (queue == NULL) ? 0 : queue->size;
}

// 获取队列容量
uint32_t SiLogPQueueCapacity(const SiLogPQueue *queue)
{
    return (queue == NULL) ? 0 : queue->capacity;
}

// 向上调整堆
STATIC void SiftUp(SiLogPQueue *queue, uint32_t index)
{
    uint8_t *buffer = queue->buffer;
    uint32_t elementSize = queue->elementSize;
    SiLogPQueueCompareFunc compare = queue->compare;

    // 临时保存当前元素
    uint8_t temp[elementSize];
    memcpy(temp, buffer + index * elementSize, elementSize);

    while (index > 0) {
        uint32_t parent = (index - 1) / 2;
        void *parentElem = buffer + parent * elementSize;

        // 如果当前元素不比父元素优先，停止
        if (compare(temp, parentElem) <= 0) {
            break;
        }

        // 父元素下移
        memcpy(buffer + index * elementSize, parentElem, elementSize);
        index = parent;
    }

    // 将当前元素放到最终位置
    memcpy(buffer + index * elementSize, temp, elementSize);
}

// 向下调整堆
STATIC void SiftDown(SiLogPQueue *queue, uint32_t index)
{
    uint8_t *buffer = queue->buffer;
    uint32_t elementSize = queue->elementSize;
    SiLogPQueueCompareFunc compare = queue->compare;
    uint32_t size = queue->size;

    // 临时保存当前元素
    uint8_t temp[elementSize];
    memcpy(temp, buffer + index * elementSize, elementSize);

    while (1) {
        uint32_t left = 2 * index + 1;
        uint32_t right = 2 * index + 2;
        uint32_t largest = index;

        if (left < size) {
            void *leftElem = buffer + left * elementSize;
            void *largestElem = buffer + largest * elementSize;
            if (compare(leftElem, largestElem) > 0) {
                largest = left;
            }
        }

        if (right < size) {
            void *rightElem = buffer + right * elementSize;
            void *largestElem = buffer + largest * elementSize;
            if (compare(rightElem, largestElem) > 0) {
                largest = right;
            }
        }

        if (largest == index) {
            break;
        }

        // 交换
        memcpy(buffer + index * elementSize, buffer + largest * elementSize, elementSize);
        index = largest;
    }

    // 将当前元素放到最终位置
    memcpy(buffer + index * elementSize, temp, elementSize);
}

// 插入元素
int32_t SilogPQueuePush(SiLogPQueue *queue, const void *element)
{
    if (queue == NULL || element == NULL) {
        return SILOG_INVALID_ARG;
    }

    if (queue->size >= queue->capacity) {
        return SILOG_BUSY;  // 队列已满
    }

    // 将新元素放到数组末尾
    memcpy(queue->buffer + queue->size * queue->elementSize, element, queue->elementSize);
    queue->size++;

    // 向上调整
    SiftUp(queue, queue->size - 1);

    return SILOG_OK;
}

// 删除并返回堆顶元素
int32_t SilogPQueuePop(SiLogPQueue *queue, void *outElement)
{
    if (queue == NULL || outElement == NULL) {
        return SILOG_INVALID_ARG;
    }

    if (queue->size == 0) {
        return SILOG_TRANS_QUEUE_EMPTY;
    }

    // 返回堆顶元素
    memcpy(outElement, queue->buffer, queue->elementSize);

    queue->size--;

    if (queue->size > 0) {
        // 将最后一个元素移到堆顶
        memcpy(queue->buffer, queue->buffer + queue->size * queue->elementSize, queue->elementSize);
        // 向下调整
        SiftDown(queue, 0);
    }

    return SILOG_OK;
}

// 获取堆顶元素（不删除）
int32_t SiLogPQueuePeek(const SiLogPQueue *queue, void *outElement)
{
    if (queue == NULL || outElement == NULL) {
        return SILOG_INVALID_ARG;
    }

    if (queue->size == 0) {
        return SILOG_TRANS_QUEUE_EMPTY;
    }

    memcpy(outElement, queue->buffer, queue->elementSize);
    return SILOG_OK;
}

// 扩容
int32_t SilogPQueueReserve(SiLogPQueue *queue, uint32_t newCapacity)
{
    if (queue == NULL || newCapacity <= queue->capacity) {
        return SILOG_INVALID_ARG;
    }

    uint8_t *newBuffer = (uint8_t *)realloc(queue->buffer, queue->elementSize * newCapacity);
    if (newBuffer == NULL) {
        return SILOG_OUT_OF_MEMORY;
    }

    queue->buffer = newBuffer;
    queue->capacity = newCapacity;

    return SILOG_OK;
}
