#include <stdint.h>
#include <unistd.h>

#include "silog_test_comm.h"
#include "silog_time.h"

int main()
{
    char buffer[32];
    SilogFormatWallClockMs(SilogGetNowMs(), buffer, sizeof(buffer));
    PRINTF("Current Wall Clock Time: %s\n", buffer);

    usleep(1500000); // 1.5s
    SilogFormatWallClockMs(SilogGetNowMs(), buffer, sizeof(buffer));
    PRINTF("Current Wall Clock Time: %s\n", buffer);

    usleep(1500); // 1.5ms
    SilogFormatWallClockMs(SilogGetNowMs(), buffer, sizeof(buffer));
    PRINTF("Current Wall Clock Time: %s\n", buffer);

    usleep(3000); // 3ms
    SilogFormatWallClockMs(SilogGetNowMs(), buffer, sizeof(buffer));
    PRINTF("Current Wall Clock Time: %s\n", buffer);

    return 0;
}
