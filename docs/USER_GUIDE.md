# SiLog 用户指南

## 目录

- [简介](#简介)
- [系统架构](#系统架构)
- [编译安装](#编译安装)
- [快速开始](#快速开始)
- [命令行工具](#命令行工具)
  - [silog - 守护进程管理](#silog---守护进程管理)
  - [silogcat - 远程日志查看](#silogcat---远程日志查看)
  - [silog_test - 测试程序](#silog_test---测试程序)
- [API 使用](#api-使用)
  - [C/C++ API](#cc-api)
  - [日志宏](#日志宏)
- [配置说明](#配置说明)
- [日志文件格式](#日志文件格式)
- [故障排除](#故障排除)

## 简介

SiLog 是一个高性能的分布式日志系统，采用客户端-服务器架构。它支持：

- **本地日志记录**：通过 Unix Domain Socket 高效传输
- **远程日志查看**：通过 TCP 连接实时查看日志
- **多级别日志**：DEBUG、INFO、WARN、ERROR、FATAL
- **自动日志轮转**：支持按大小和时间的日志切割
- **远程广播**：支持跨主机日志广播

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                        SiLog 架构                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐     IPC (Unix Socket)    ┌──────────────┐│
│  │   应用进程    │ ═══════════════════════► │   silogd     ││
│  │  SILOG_I()   │                          │   守护进程    ││
│  └──────────────┘                          └──────┬───────┘│
│                                                   │        │
│                          ┌────────────────────────┘        │
│                          │                                  │
│                          ▼                                  │
│                   ┌──────────────┐                         │
│                   │  日志文件系统 │                         │
│                   │ /tmp/silog/  │                         │
│                   └──────────────┘                         │
│                          │                                  │
│                          │ TCP (Port 9090)                 │
│                          ▼                                  │
│                   ┌──────────────┐                         │
│                   │   silogcat   │                         │
│                   │  远程客户端   │                         │
│                   └──────────────┘                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 模块说明

| 模块 | 说明 | 输出 |
|------|------|------|
| `silog_exe` | 守护进程启动器 | 可执行文件 `silog` |
| `silog_dae` | 守护进程核心 | 共享库 `libsilog_dae.so` |
| `silog_logger` | 日志客户端库 | 共享库 `libsilog_logger.so` |
| `silog_cat` | 远程日志查看工具 | 可执行文件 `silogcat` |
| `silog_comm` | 公共工具库 | 静态库 `libsilog_common.a` |

## 编译安装

### 环境要求

- Linux 操作系统（支持 Unix Domain Socket）
- GCC 编译器（支持 C11）
- CMake 3.10+
- pthread 库

### 编译步骤

```bash
# 进入项目根目录
cd SiLog

# 创建构建目录
mkdir -p build && cd build

# 配置 CMake
cmake ..

# 编译
make -j4

# 运行测试
make test
```

### 编译输出

编译完成后，输出文件位于 `build/` 目录：

```
build/
├── silog_exe/silog              # 守护进程管理工具
├── silog_cat/silogcat           # 远程日志查看工具
├── silog_logger/libsilog_logger.so
├── silog_dae/libsilog_dae.so
└── test/                        # 测试程序
    ├── silog_unittest
    ├── silog_integrationtest
    └── silog_test               # 示例测试程序
```

## 快速开始

### 1. 启动守护进程

```bash
# 后台启动守护进程
./build/silog_exe/silog start -d

# 查看状态
./build/silog_exe/silog status
```

输出示例：
```
SiLog Daemon Status:
  Running: Yes
  PID: 12345
  Remote clients: 0

SiLog is ready to receive logs.
Use SILOG_I(), SILOG_D(), etc. macros to send logs.
```

### 2. 发送测试日志

```bash
# 使用 silog 发送测试日志
./build/silog_exe/silog test

# 或运行测试程序
./build/tests/legacy/silog_test
```

### 3. 查看日志文件

```bash
# 查看本地日志文件
tail -f /tmp/silog/silog.log
```

### 4. 远程查看日志（可选）

```bash
# 在另一个终端启动 silogcat
./build/silog_cat/silogcat
```

### 5. 停止守护进程

```bash
./build/silog_exe/silog stop
```

## 命令行工具

### silog - 守护进程管理

`silog` 是 SiLog 守护进程的管理工具。

#### 命令列表

| 命令 | 说明 | 示例 |
|------|------|------|
| `start` | 前台启动守护进程 | `silog start` |
| `start -d` | 后台启动守护进程 | `silog start -d` |
| `stop` | 停止守护进程 | `silog stop` |
| `status` | 查看守护进程状态 | `silog status` |
| `test` | 发送测试日志 | `silog test` |
| `-h, --help` | 显示帮助 | `silog --help` |

#### 使用示例

**前台启动（调试模式）**

```bash
$ ./silog start
SiLog Daemon is running (PID: 12345)
Press Ctrl+C to stop

# 按 Ctrl+C 停止
```

**后台启动**

```bash
$ ./silog start -d
Starting SiLog Daemon...
SiLog Daemon started successfully (PID: 12345)
```

**查看状态**

```bash
$ ./silog status
SiLog Daemon Status:
  Running: Yes
  PID: 12345
  Remote clients: 0

SiLog is ready to receive logs.
Use SILOG_I(), SILOG_D(), etc. macros to send logs.
```

**发送测试日志**

```bash
$ ./silog test
Sending test logs to SiLog Daemon...

[DEBUG] Test message sent
[INFO] Test message sent
[WARN] Test message sent
[ERROR] Test message sent

Test logs sent successfully.
Check the log file: /tmp/silog/silog.log
```

**停止守护进程**

```bash
$ ./silog stop
SiLog Daemon stopped
```

### silogcat - 远程日志查看

`silogcat` 用于通过网络连接到守护进程，实时查看日志输出。

#### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-s, --server <addr>` | 服务器地址 | `127.0.0.1` |
| `-p, --port <port>` | 服务器端口 | `9090` |
| `-t, --tag <tag>` | 过滤标签 | - |
| `-l, --level <level>` | 最小日志级别 | `DEBUG` |
| `-c, --color` | 彩色输出 | - |
| `-h, --help` | 显示帮助 | - |

#### 使用示例

**基本使用**

```bash
# 连接到本地守护进程
./silogcat

# 输出示例：
# 03-07 14:30:25.123  1234  5678 I MyApp: Hello World (test.cpp:42)
```

**连接远程服务器**

```bash
./silogcat -s 192.168.1.100 -p 9090
```

**过滤标签**

```bash
# 只显示标签包含 "Network" 的日志
./silogcat -t Network
```

**过滤级别**

```bash
# 只显示 WARN 及以上级别
./silogcat -l WARN
```

**彩色输出**

```bash
./silogcat -c
```

### silog_test - 测试程序

`silog_test` 是一个示例程序，演示如何使用 SILOG 宏发送日志。

```bash
./build/tests/legacy/silog_test
```

输出：
```
SiLog Test Program
==================

Testing different log levels...
Log levels test completed.

Testing different module tags...
Module tags test completed.

...

All tests completed!
```

## API 使用

### C/C++ API

#### 头文件

```c
#include "silog.h"  // 包含所有日志宏
```

#### 日志宏

| 宏 | 级别 | 说明 |
|----|------|------|
| `SILOG_D(tag, fmt, ...)` | DEBUG | 调试信息 |
| `SILOG_I(tag, fmt, ...)` | INFO | 普通信息 |
| `SILOG_W(tag, fmt, ...)` | WARN | 警告信息 |
| `SILOG_E(tag, fmt, ...)` | ERROR | 错误信息 |
| `SILOG_F(tag, fmt, ...)` | FATAL | 致命错误 |

#### 使用示例

```c
#include "silog.h"
#include <stdio.h>

#define TAG "MyApplication"

int main(void)
{
    // 不同级别的日志
    SILOG_D(TAG, "Debug message, value=%d", 42);
    SILOG_I(TAG, "Application started");
    SILOG_W(TAG, "Low memory warning: %d%%", 15);
    SILOG_E(TAG, "Failed to open file: %s", "config.txt");
    SILOG_F(TAG, "Critical error, aborting!");

    // 格式化字符串
    SILOG_I(TAG, "User %s logged in from %s", "alice", "192.168.1.100");
    SILOG_D(TAG, "Coordinates: (%.2f, %.2f)", 3.14, 2.71);

    return 0;
}
```

#### 编译链接

```bash
# 编译时需要链接 silog_logger 库
gcc -o myapp myapp.c -I/path/to/SiLog/interface \
    -L/path/to/SiLog/build/silog_logger \
    -lsilog_logger -lpthread
```

#### 设置日志级别

```c
#include "silog_logger.h"

// 设置最小日志级别为 INFO（低于 INFO 的日志将被忽略）
silogSetLevel(SILOG_INFO);
```

可选级别：
- `SILOG_DEBUG` - 显示所有日志
- `SILOG_INFO` - 显示 INFO 及以上
- `SILOG_WARN` - 显示 WARN 及以上
- `SILOG_ERROR` - 显示 ERROR 及以上
- `SILOG_FATAL` - 只显示 FATAL

## 配置说明

### 日志文件位置

| 文件 | 说明 |
|------|------|
| `/tmp/silog/silog.log` | 主日志文件 |
| `/tmp/silogd.pid` | 守护进程 PID 文件 |
| `/tmp/silog_exe.txt` | silog_exe 预日志 |
| `/tmp/silog_daemon.txt` | 守护进程预日志 |

### 远程服务配置

默认配置：
- TCP 端口：`9090`
- 最大客户端数：`10`

通过代码配置（高级）：

```c
#include "silog_daemon.h"

SilogDaemonRemoteConfig config = {
    .listenPort = 9090,    // 监听端口
    .maxClients = 10,      // 最大客户端数
    .enable = true         // 启用远程服务
};

SilogDaemonRemoteInit(&config);
```

## 日志文件格式

### 格式说明

```
[时间戳][日志级别][进程ID 线程ID][标签][文件名:行号] 消息内容
```

### 示例

```
[2026-03-07 23:48:52.0000][INFO][pid:103131 tid:103131][MyApp][main.c:45] Application started
[2026-03-07 23:48:52.0123][ERROR][pid:103131 tid:103132][Network][net.c:120] Connection failed: Connection refused
```

### 字段说明

| 字段 | 说明 | 示例 |
|------|------|------|
| 时间戳 | 年月日 时分秒.毫秒 | `2026-03-07 23:48:52.0000` |
| 日志级别 | DEBUG/INFO/WARN/ERROR/FATAL | `[INFO]` |
| 进程ID | 发送日志的进程 ID | `pid:103131` |
| 线程ID | 发送日志的线程 ID | `tid:103131` |
| 标签 | 模块标签 | `[MyApp]` |
| 文件名 | 源文件名 | `[main.c:45]` |
| 消息 | 日志内容 | `Application started` |

## 故障排除

### 常见问题

#### 1. 守护进程无法启动

**症状**：
```bash
$ ./silog start -d
Failed to start SiLog Daemon
```

**排查步骤**：

```bash
# 1. 检查是否有残留进程
ps aux | grep silog

# 2. 检查端口占用
lsof -i :9090

# 3. 查看预日志
cat /tmp/silog_exe.txt

# 4. 前台启动查看详细错误
./silog start
```

#### 2. 无法发送日志

**症状**：程序调用 SILOG_I 但日志文件无内容

**排查步骤**：

```bash
# 1. 检查守护进程是否运行
./silog status

# 2. 检查 IPC socket
ls -la /tmp/logd.sock

# 3. 检查日志文件权限
ls -la /tmp/silog/

# 4. 查看守护进程预日志
cat /tmp/silog_daemon.txt
```

#### 3. silogcat 无法连接

**症状**：
```
Connecting to 127.0.0.1:9090...
[Disconnected, reconnecting...]
```

**排查步骤**：

```bash
# 1. 检查守护进程状态
./silog status

# 2. 检查端口监听
netstat -tlnp | grep 9090

# 3. 检查防火墙
iptables -L | grep 9090
```

### 日志级别不生效

确保在调用日志宏之前设置级别：

```c
// 正确
silogSetLevel(SILOG_WARN);
SILOG_I(TAG, "This will not show");  // 被过滤
SILOG_W(TAG, "This will show");      // 显示

// 错误 - 顺序颠倒
SILOG_I(TAG, "This will show");  // 先发送了
silogSetLevel(SILOG_WARN);       // 后设置级别
```

### 性能问题

如果日志发送影响性能：

1. **提高日志级别**：只记录 WARN 及以上级别
   ```c
   silogSetLevel(SILOG_WARN);
   ```

2. **减少日志量**：避免在循环中记录日志

3. **异步发送**：SiLog 默认使用异步发送，一般不会阻塞

### 获取帮助

查看预日志文件：

```bash
# silog_exe 预日志
cat /tmp/silog_exe.txt

# 守护进程预日志
cat /tmp/silog_daemon.txt

# logger 预日志
cat /tmp/silog_logger.txt
```

运行测试：

```bash
cd build
make test
```

---

**版本**: SiLog 1.0
**最后更新**: 2026-03-07
