#ifndef SILOG_TEST_COMM_H
#define SILOG_TEST_COMM_H
#include <stdio.h>

#define PRINTF(fmt, ...) printf(__FILE__ ":%d] " fmt "\n", __LINE__, ##__VA_ARGS__)

#endif // SILOG_TEST_COMM_H
