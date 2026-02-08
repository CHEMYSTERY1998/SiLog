/**
 * @file silog_interface.h
 * @brief SiLog 接口定义（预留）
 */

#ifndef SILOG_INTERFACE_H
#define SILOG_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ================ Daemon 服务端接口 ================ */
/* Daemon 服务端初始化接口 */
/* Daemon 服务端启动接口 */
/* Daemon 服务端配置接口：配置缓冲区大小、日志文件路径、日志切割策略、等参数 */
/* Daemon 服务端停止接口 */
/* Daemon 服务端状态查询接口: 查询当前日志文件大小、已用缓冲区大小等状态信息、外部连接客户端信息 */

/* ================ Daemon 客户端接口 ================ */
/* Daemon 客户端初始化接口 */
/* Daemon 客户端配置接口-->调用服务端配置接口 */
/* Daemon 客户端注册日志信息回调接口 */

#ifdef __cplusplus
}
#endif

#endif /* SILOG_INTERFACE_H */
