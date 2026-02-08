#include <stdio.h>
#include <unistd.h>

#include "silog_daemon.h"
#include "silog_error.h"

int main(void)
{
    int32_t ret = SilogDaemonInit();
    if (ret != SILOG_OK) {
        fprintf(stderr, "SilogDaemonInit failed: %d\n", ret);
        return ret;
    }

    printf("SiLog Daemon Started\n");

    /* 主事件循环 */
    while (1) {
        sleep(1);
    }

    return 0;
}
