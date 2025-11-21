#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "silog_trans.h"

typedef struct {
    int32_t level;
    char tag[32];
    char msg[128];
} logEntry_t;

#define SOCK_PATH    "/tmp/silog_demo.sock"

int main()
{
    int32_t fd = silogTransClientInit(SOCK_PATH);
    if (fd < 0) {
        perror("connect");
        return 1;
    }

    printf("[CLIENT] connected to server\n");

    logEntry_t e;
    memset(&e, 0, sizeof(e));

    e.level = 3;
    strcpy(e.tag, "demo");
    strcpy(e.msg, "hello silog!");

    if (silogTransClientSend(fd, &e, sizeof(e)) == 0) {
        printf("[CLIENT] send success\n");
    } else {
        printf("[CLIENT] send failed\n");
    }

    close(fd);
    return 0;
}
