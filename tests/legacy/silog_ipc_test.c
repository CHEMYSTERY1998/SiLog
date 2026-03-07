#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "silog_error.h"
#include "silog_ipc.h"
#include "silog_logger.h"

void *SilogIpcServerTestInit(void *arg)
{
    (void)arg;
    printf("server thread started\n");
    SilogIpcInit(SILOG_IPC_TYPE_UNIX_DGRAM);
    int ret = SilogIpcServerInit();
    if (ret != SILOG_OK) {
        printf("SilogIpcServerInit failed: %d\n", ret);
        return NULL;
    }
    printf("SilogIpcServerInit success\n");
    logEntry_t entry;
    while (1) {
        if (SilogIpcServerRecv(&entry, sizeof(entry)) > 0) {
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
    int ret = pthread_create(&tid, NULL, SilogIpcServerTestInit, NULL);
    if (ret != 0) {
        printf("pthread_create failed: %d\n", ret);
        return -1;
    }
    pthread_detach(tid);

    usleep(1000 * 1000); // 100ms
    ret = SilogIpcClientInit();
    if (ret != SILOG_OK) {
        printf("SilogIpcClientInit failed: %d\n", ret);
        return -1;
    }
    printf("SilogIpcClientInit success\n");
    logEntry_t entry;
    entry.ts = 123456789;
    ret = SilogIpcClientSend(&entry, sizeof(entry));
    if (ret != SILOG_OK) {
        printf("SilogIpcClientSend failed: %d\n", ret);
        return -1;
    }
    printf("send success\n");
    sleep(1);

    return 0;
}
