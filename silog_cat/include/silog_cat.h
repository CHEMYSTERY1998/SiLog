/**
 * @file silog_cat.h
 * @brief SiLog 日志查看工具（客户端）
 */

#ifndef SILOG_CAT_H
#define SILOG_CAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Daemon 客户端初始化接口
 * @return 成功返回 SILOG_OK，失败返回错误码
 */
int32_t SilogDaemonClientInit(void);

/* Daemon 客户端配置接口-->调用服务端配置接口 */
/* Daemon 客户端注册日志信息回调接口 */

#ifdef __cplusplus
}
#endif

#endif /* SILOG_CAT_H */
