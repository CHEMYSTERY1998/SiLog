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
#include "silog_trans.h"
#include "silog_entry.h"

static const char *level_to_name(silogLevel level)
{
    switch (level) {
    case SILOG_DEBUG:
        return "DEBUG";
    case SILOG_INFO:
        return "INFO";
    case SILOG_WARN:
        return "WARN";
    case SILOG_ERROR:
        return "ERROR";
    case SILOG_FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}
void printEntry(const logEntry_t *entry)
{
    if (!entry)
        return;
    /* 假设 ts 为毫秒时间戳 */
    time_t sec = entry->ts / 1000;
    long msec = entry->ts % 1000;
    struct tm tm;
    localtime_r(&sec, &tm);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm);

    printf("[%s.%03ld][%s][pid:%d tid:%d][%s][%s:%u] %s\n", timebuf, msec, level_to_name(entry->level), entry->pid,
           entry->tid, entry->tag, entry->file, entry->line, entry->msg);
}

void *entryHandle(void *arg)
{
    (void)arg;
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
            printEntry(&entry);
        }
    }
    SilogTransServerClose();
    return NULL;
}

void createTestInitThread()
{
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, entryHandle, NULL);
    if (ret != 0) {
        printf("pthread_create failed: %d\n", ret);
        return;
    }

    pthread_detach(tid);
}

int main()
{
    createTestInitThread();
    usleep(1000); // 1ms
    SILOG_I("DemoClient", "SiLog demo client started");

    for (int i = 0; i < 5; i++) {
        SILOG_D("DemoClient", "Debug message %d", i);
        SILOG_I("DemoClient", "Info message %d", i);
        SILOG_W("DemoClient", "Warning message %d", i);
        SILOG_E("DemoClient", "Error message %d", i);
        SILOG_F("DemoClient", "Fatal message %d", i);
    }

    SILOG_I("DemoClient", "SiLog demo client finished");
    sleep(1);
    return 0;
}
