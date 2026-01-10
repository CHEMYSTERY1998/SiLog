#include "silog_utils.h"

#include <stdint.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "silog_adapter.h"

pid_t getTid(void)
{
    return (pid_t)syscall(SYS_gettid);
}
