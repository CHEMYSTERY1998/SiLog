#include "silog_cat.h"

#include <stdint.h>

#include "silog_prelog.h"

int32_t SilogDaemonClientInit(void)
{
    /* 初始化预日志模块 */
    SilogPrelogConfig_t prelogConfig = {
        .path = "/tmp/silog_cat.txt",
        .minLevel = SILOG_PRELOG_LEVEL_DEBUG,
        .enableStdout = false,
    };
    (void)SilogPrelogInit(&prelogConfig);

    SILOG_PRELOG_I(SILOG_PRELOG_CAT, "SilogCat initialized");
    return 0;
}
