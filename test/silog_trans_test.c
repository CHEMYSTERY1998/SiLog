#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "silog_entry.h"
#include "silog_error.h"
#include "silog_trans.h"

void *SilogTransServerTestInit(void *arg)
{
    (void)arg;
    printf("server thread started\n");
    SilogTransInit(TRAN_TYPE_UDP);
    int ret = SilogTransServerInit();
    if (ret != SILOG_OK) {
        printf("SilogTransServerInit failed: %d\n", ret);
        return NULL;
    }
    printf("SilogTransServerInit success\n");
    logEntry_t entry;
    while (1) {
        if (SilogTransServerRecv(&entry, sizeof(entry)) > 0) {
            printf("receive success, time: %lu\n", entry.ts);
        } else {
            usleep(1000); // 1ms
            printf("waiting for data...\n");
        }
    }
    return NULL;
}

int main()
{
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, SilogTransServerTestInit, NULL);
    if (ret != 0) {
        printf("pthread_create failed: %d\n", ret);
        return -1;
    }
    pthread_detach(tid);

    usleep(1000 * 1000); // 100ms
    ret = SilogTransClientInit();
    if (ret != SILOG_OK) {
        printf("SilogTransClientInit failed: %d\n", ret);
        return -1;
    }
    printf("SilogTransClientInit success\n");
    logEntry_t entry;
    entry.ts = 123456789;
    ret = SilogTransClientSend(&entry, sizeof(entry));
    if (ret != SILOG_OK) {
        printf("SilogTransClientSend failed: %d\n", ret);
        return -1;
    }
    printf("send success\n");
    sleep(1);

    return 0;
}