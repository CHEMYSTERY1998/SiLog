#ifndef SILOG_TRANS_H
#define SILOG_TRANS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TRAN_TYPE_UDP = 0,
    TRAN_TYPE_TCP
} TransType;

int32_t SilogTransClientInit();
int32_t SilogTransClientSend(const void *data, uint32_t len);
void SilogTransClientClose();

int32_t SilogTransServerInit();
int32_t SilogTransServerRecv(void *data, uint32_t len);
void SilogTransServerClose();

void SilogTransInit(TransType type);

#ifdef __cplusplus
}
#endif

#endif // SILOG_TRANS_H
