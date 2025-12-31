#include "silog_utils.h"

uint64_t silogGetCurTimeMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

pid_t getTid(void)
{
    return (pid_t)syscall(SYS_gettid);
}