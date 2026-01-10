#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "silog_logger.h"

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