# SiLog 文档

## 文档列表

| 文档 | 说明 |
|------|------|
| [USER_GUIDE.md](USER_GUIDE.md) | 详细用户指南，包含使用说明、API 文档、故障排除 |

## 快速参考

### 启动系统

```bash
# 后台启动守护进程
./silog_exe/silog start -d

# 查看状态
./silog_exe/silog status
```

### 发送日志

```c
#include "silog.h"

SILOG_I("MyApp", "Hello World");
SILOG_D("MyApp", "Debug value: %d", 42);
SILOG_E("MyApp", "Error: %s", strerror(errno));
```

### 查看日志

```bash
# 本地查看
tail -f /tmp/silog/silog.log

# 远程实时查看
./silog_cat/silogcat
```

### 停止系统

```bash
./silog_exe/silog stop
```

## 更多信息

请参阅 [USER_GUIDE.md](USER_GUIDE.md) 获取完整文档。
