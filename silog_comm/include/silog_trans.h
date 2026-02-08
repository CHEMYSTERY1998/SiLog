/**
 * @file silog_trans.h
 * @brief SiLog 传输层抽象（UDP/TCP）
 */

#ifndef SILOG_TRANS_H
#define SILOG_TRANS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 传输类型枚举
 */
typedef enum {
    SILOG_TRAN_TYPE_UDP = 0, ///< UDP 传输
    SILOG_TRAN_TYPE_TCP      ///< TCP 传输
} SilogTransType_t;

/**
 * @brief 初始化传输层
 * @param type 传输类型（UDP/TCP）
 */
void SilogTransInit(SilogTransType_t type);

/**
 * @brief 客户端初始化
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogTransClientInit(void);

/**
 * @brief 客户端发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogTransClientSend(const void *data, uint32_t len);

/**
 * @brief 关闭客户端连接
 */
void SilogTransClientClose(void);

/**
 * @brief 服务端初始化
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogTransServerInit(void);

/**
 * @brief 服务端接收数据
 * @param data 接收缓冲区
 * @param len 缓冲区大小
 * @return 成功返回接收字节数，失败返回负值
 */
int32_t SilogTransServerRecv(void *data, uint32_t len);

/**
 * @brief 关闭服务端
 */
void SilogTransServerClose(void);

#ifdef __cplusplus
}
#endif

#endif /* SILOG_TRANS_H */
