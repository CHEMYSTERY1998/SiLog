#include <stdio.h>
#include <unistd.h>

#include "silog_daemon.h"
#include "silog_error.h"
#include "silog_prelog.h"

int main(void)
{
    /* 初始化预日志模块 */
    SilogPrelogConfig_t prelogConfig = {
        .path = "/tmp/silog_exe.txt",
        .minLevel = SILOG_PRELOG_LEVEL_DEBUG,
        .enableStdout = true,
    };
    (void)SilogPrelogInit(&prelogConfig);

    SILOG_PRELOG_I(SILOG_PRELOG_EXE, "SiLogExe starting");

    int32_t ret = SilogDaemonInit();
    if (ret != SILOG_OK) {
        SILOG_PRELOG_E(SILOG_PRELOG_EXE, "SilogDaemonInit failed: %d", ret);
        return ret;
    }

    SILOG_PRELOG_I(SILOG_PRELOG_EXE, "SiLog Daemon Started");

    /* 主事件循环 */
    while (1) {
        sleep(1);
    }

    return 0;
}
