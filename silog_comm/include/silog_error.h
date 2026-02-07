#ifndef SILOG_ERROR_H
#define SILOG_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SILOG_OK = 0, // 没有错误

    // 通用错误（1000 ~ 1999）
    SILOG_UNKNOWN = 1000,  // 未知错误
    SILOG_INVALID_ARG,     // 参数非法
    SILOG_NULL_PTR,        // 空指针
    SILOG_OUT_OF_MEMORY,   // 内存不足
    SILOG_TIMEOUT,         // 超时
    SILOG_NOT_IMPLEMENTED, // 未实现
    SILOG_BUSY,            // 资源忙

    // 文件相关错误（2000 ~ 2999）
    SILOG_FILE_OPEN = 2000, // 文件打开失败
    SILOG_FILE_READ,        // 文件读取失败
    SILOG_FILE_WRITE,       // 文件写入失败
    SILOG_FILE_NOT_EXIST,   // 文件不存在
    SILOG_FILE_PERMISSION,  // 权限不足

    // 网络相关错误（3000 ~ 3999）
    SILOG_NET_CONNECT = 3000, // 连接失败
    SILOG_NET_FILE_CREATE,    // 网络文件创建失败
    SILOG_NET_FILE_OPEN,      // 网络文件打开失败
    SILOG_NET_FILE_ERROR,     // 网络文件错误
    SILOG_NET_SEND,           // 发送失败
    SILOG_NET_RECV,           // 接收失败
    SILOG_NET_TIMEOUT,        // 网络超时
    SILOG_NET_CLOSED,         // 连接已关闭
    SILOG_NET_UNREACHABLE,    // 网络不可达

    // 系统相关错误（4000 ~ 4999）
    SILOG_SYS_CALL = 4000, // 系统调用失败
    SILOG_SYS_RESOURCE,    // 系统资源不足
    SILOG_THREAD_CREATE,   // 线程创建失败
    SILOG_THREAD_JOIN,     // 线程等待失败
    SILOG_MUTEX_LOCK,      // 互斥锁加锁失败

    // 字符串处理错误（5000 ~ 5999）
    SILOG_STR_TOO_LONG = 5000, // 字符串过长
    SILOG_STR_EMPTY,           // 字符串为空
    SILOG_STR_ERR,             // 字符串处理错误

    // 自定义
    SILOG_TRANS_NOT_INIT,
    SILOG_TRANS_QUEUE_FULL,
    SILOG_TRANS_QUEUE_EMPTY,

    SILOG_FILE_MANAGER_NOT_INIT,

} ErrorCode;

#ifdef __cplusplus
}
#endif

#endif // SILOG_ERROR_H
