#ifndef SILOG_MPSC_H
#define SILOG_MPSC_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化队列（capacity 必须是 2 的幂）
int32_t SilogMpscQueueInit(uint32_t elementSize, uint32_t capacity);

// 销毁队列
void SilogMpscQueueDestroy();

// push：多生产者（永不阻塞）
int32_t SilogMpscQueuePush(const void *element);

// pop：单消费者
int32_t SilogMpscQueuePop(void *outElement);

#ifdef __cplusplus
}
#endif

#endif // SILOG_MPSC_H
