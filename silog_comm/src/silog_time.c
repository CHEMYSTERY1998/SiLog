#include "silog_time.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "silog_adapter.h"
#include "silog_prelog.h"
#include "silog_securec.h"

// ================ 常量定义 ================

// 时间单位转换
#define MS_PER_SEC 1000ULL    // 毫秒每秒
#define US_PER_SEC 1000000ULL // 微秒每秒
#define NS_PER_MS  10000ULL   // 纳秒每毫秒
#define NS_PER_US  1000ULL    // 纳秒每微秒

// 时间偏移
#define YEAR_BASE_OFFSET  1900 // tm_year 基准年份
#define MONTH_BASE_OFFSET 1    // tm_mon 基准月份

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
    return (ui.QuadPart - EPOCH_DIFF) / NS_PER_MS; /* → ms */
#elif defined(SILOG_LINUX)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "clock_gettime(CLOCK_REALTIME) failed: %s", strerror(errno));
        return 0;
    }

    return (uint64_t)ts.tv_sec * MS_PER_SEC + (uint64_t)ts.tv_nsec / (NS_PER_US * US_PER_SEC);
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
    return (uint64_t)(counter.QuadPart * MS_PER_SEC / freq.QuadPart);
#elif defined(SILOG_LINUX)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "clock_gettime(CLOCK_MONOTONIC) failed: %s", strerror(errno));
        return 0;
    }

    return (uint64_t)ts.tv_sec * MS_PER_SEC + (uint64_t)ts.tv_nsec / (NS_PER_US * US_PER_SEC);
#endif
}

void SilogFormatWallClockMs(uint64_t inputMs, char *buffer, uint32_t bufferLen)
{
    if (!buffer || bufferLen == 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "FormatWallClockMs failed: invalid argument");
        return;
    }
    buffer[0] = '\0';

    uint64_t sec64 = inputMs / MS_PER_SEC;
    uint32_t msec = (uint32_t)(inputMs % MS_PER_SEC);
    if (sec64 > (uint64_t)INT64_MAX) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "FormatWallClockMs failed: timestamp overflow");
        return;
    }

    time_t sec = (time_t)sec64;
    struct tm tm_info;

#if defined(SILOG_WINDOWS)
    if (localtime_s(&tm_info, &sec) != 0) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "FormatWallClockMs failed: localtime_s error");
        return; /* localtime_s 失败 */
    }
#else
    if (localtime_r(&sec, &tm_info) == NULL) {
        SILOG_PRELOG_E(SILOG_PRELOG_COMM, "FormatWallClockMs failed: localtime_r error: %s", strerror(errno));
        return; /* localtime_r 失败 */
    }
#endif

    int n = snprintf_s(buffer, bufferLen, bufferLen - 1, "%04d-%02d-%02d %02d:%02d:%02d.%04u",
                       tm_info.tm_year + YEAR_BASE_OFFSET, tm_info.tm_mon + MONTH_BASE_OFFSET, tm_info.tm_mday,
                       tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, msec);
    if (n < 0) {
        buffer[bufferLen - 1] = '\0';
    }
}
