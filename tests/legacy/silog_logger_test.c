#include "silog.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "silog_error.h"
#include "silog_logger.h"
#include "silog_test_comm.h"
#include "silog_time.h"
#include "silog_ipc.h"
#include "silog_utils.h"

void printEntry(const logEntry_t *entry)
{
    if (!entry) {
        return;
    }
    char timebuf[32];
    SilogFormatWallClockMs(entry->ts, timebuf, sizeof(timebuf));
    printf("[%s][%s][pid:%d tid:%d][%s][%s:%u] %s\n", timebuf, SilogUtilsLevelToName(entry->level), entry->pid, entry->tid,
           entry->tag, entry->file, entry->line, entry->msg);
}

void *entryHandle(void *arg)
{
    (void)arg;
    SilogIpcInit(SILOG_IPC_TYPE_UNIX_DGRAM);
    int ret = SilogIpcServerInit();
    if (ret != SILOG_OK) {
        PRINTF("SilogIpcServerInit failed: %d", ret);
        return NULL;
    }
    PRINTF("SilogIpcServerInit success");

    logEntry_t entry;
    while (1) {
        if (SilogIpcServerRecv(&entry, sizeof(entry)) > 0) {
            printEntry(&entry);
        }
    }
    SilogIpcServerClose();
    return NULL;
}

void createTestInitThread()
{
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, entryHandle, NULL);
    if (ret != 0) {
        PRINTF("pthread_create failed: %d", ret);
        return;
    }

    pthread_detach(tid);
}

// 随机usleep 100 ~ 5000 微秒
void randomSleep()
{
    // 添加种子
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    int usec = 100 + rand() % 4901;
    usleep(usec);
}

int main()
{
    createTestInitThread();
    usleep(1000); // 1ms
    SILOG_I("DemoClient", "SiLog demo client started");

    for (int i = 0; i < 5; i++) {
        randomSleep();
        SILOG_D("DemoClient", "Debug message %d", i);
        randomSleep();
        SILOG_I("DemoClient", "Info message %d", i);
        randomSleep();
        SILOG_W("DemoClient", "Warning message %d", i);
        randomSleep();
        SILOG_E("DemoClient", "Error message %d", i);
        randomSleep();
        SILOG_F("DemoClient", "Fatal message %d", i);
    }

    randomSleep();
    SILOG_I("DemoClient", "SiLog demo client finished");
    sleep(1);
    return 0;
}
