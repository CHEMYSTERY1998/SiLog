#include "silog_daemon.h"


#include <stdint.h>

/*
    至少需要三个线程，
    1、日志接收线程：负责接收应用进程发送过来的日志数据，并存入缓冲区
    2、日志写入线程：负责从缓冲区读取日志数据，并写入文件
    3、日志传输线程：负责将日志数据通过网络传输到指定客户端
*/
int32_t SilogDaemonInit()
{
//     // 初始化日志接收线程
//     SilogDaemonRecvThreadInit();

//     // 初始化日志写入线程
//     SilogDaemonWriteThreadInit();

//     // 初始化日志传输线程
//     SilogDaemonTransThreadInit();
    return 0;
}

