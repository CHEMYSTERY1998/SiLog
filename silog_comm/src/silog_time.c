#include "silog_time.h"

#include <stdio.h>
#include <string.h>

#include "silog_adapter.h"

#if defined(SILOG_WINDOWS)
#include <windows.h>
#elif defined(SILOG_LINUX)
#include <sys/time.h>
#include <time.h>
#endif

uint64_t SilogGetNowMs(void)
{
#if defined(SILOG_WINDOWS)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER ui;
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;

    /* FILETIME: 100ns since 1601-01-01 */
    const uint64_t EPOCH_DIFF = 116444736000000000ULL; /* 1970-01-01 */
    if (ui.QuadPart < EPOCH_DIFF) {
        return 0;
    }
    return (ui.QuadPart - EPOCH_DIFF) / 10000ULL; /* → ms */
#elif defined(SILOG_LINUX)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }

    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

uint64_t SilogGetMonoMs(void)
{
#if defined(SILOG_WINDOWS)
    static LARGE_INTEGER freq = {0};
    static volatile LONG init = 0;

    if (InterlockedCompareExchange(&init, 1, 0) == 0) {
        QueryPerformanceFrequency(&freq);
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    /* 避免溢出：先除后乘 */
    return (uint64_t)(counter.QuadPart * 1000ULL / freq.QuadPart);
#elif defined(SILOG_LINUX)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

void SilogFormatWallClockMs(uint64_t inputMs, char *buffer, uint32_t bufferLen)
{
    if (!buffer || bufferLen == 0) {
        return;
    }
    buffer[0] = '\0';

    uint64_t sec64 = inputMs / 1000;
    uint32_t msec = (uint32_t)(inputMs % 1000);
    if (sec64 > (uint64_t)INT64_MAX) {
        return;
    }

    time_t sec = (time_t)sec64;
    struct tm tm_info;

#if defined(SILOG_WINDOWS)
    if (localtime_s(&tm_info, &sec) != 0) {
        return; /* localtime_s 失败 */
    }
#else
    if (localtime_r(&sec, &tm_info) == NULL) {
        return; /* localtime_r 失败 */
    }
#endif

    int n = snprintf(buffer, bufferLen, "%04d-%02d-%02d %02d:%02d:%02d.%04u", tm_info.tm_year + 1900,
                     tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, msec);
    if (n < 0 || (uint32_t)n >= bufferLen) {
        buffer[bufferLen - 1] = '\0';
    }
}
