#include "silog_utils.h"

#include <stdint.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "silog.h"
#include "silog_adapter.h"

pid_t getTid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

const char *SilogLevelToName(silogLevel level)
{
    static const char *names[] = {
        [SILOG_DEBUG] = "DEBUG", [SILOG_INFO] = "INFO",   [SILOG_WARN] = "WARN",
        [SILOG_ERROR] = "ERROR", [SILOG_FATAL] = "FATAL",
    };

    if (level < 0 || level >= (int)(sizeof(names) / sizeof(names[0])) || !names[level]) {
        return "UNKNOWN";
    }
    return names[level];
}