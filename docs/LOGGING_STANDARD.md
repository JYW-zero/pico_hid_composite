# 日志规范文档

**版本**: 1.0  
**最后更新**: 2026-08-24  
**适用范围**: Pico HID Composite 固件 + .NET 跨平台上位机

---

## 目录

1. [概述](#概述)
2. [固件日志规范](#固件日志规范)
3. [上位机日志规范](#上位机日志规范)
4. [日志级别定义](#日志级别定义)
5. [日志格式规范](#日志格式规范)
6. [日志最佳实践](#日志最佳实践)
7. [日志性能优化](#日志性能优化)

---

## 概述

本文档统一描述 Pico HID Composite 项目中的日志规范，包括：

- **固件日志**：通过串口输出的调试日志，用于开发和调试
- **上位机日志**：应用程序内部日志，用于问题排查和用户反馈
- **固件错误日志**：存储在 Flash 中的错误记录，上位机可读取并显示

所有日志遵循统一的级别定义和格式规范，确保跨层日志的一致性和可读性。

---

## 固件日志规范

### 日志接口

**定义位置**: `firmware/include/app/log.h`  
**输出方式**: `printf`（串口输出）  
**编译控制**: 通过 `-DLOG_LEVEL=N` 控制日志输出级别

### 日志宏

| 宏 | 级别 | 格式前缀 | 说明 |
|----|------|---------|------|
| `LOG_ERROR_PRINT(fmt, ...)` | ERROR | `[ERR]` | 错误信息，必须输出 |
| `LOG_WARN_PRINT(fmt, ...)` | WARN | `[WARN]` | 警告信息，默认输出 |
| `LOG_INFO_PRINT(fmt, ...)` | INFO | `[INFO]` | 一般信息，默认输出 |
| `LOG_DEBUG_PRINT(fmt, ...)` | DEBUG | `[DBG]` | 调试信息，仅调试版本输出 |

### 日志级别控制

```c
// 编译时控制日志级别
// 0 = NONE  (无输出)
// 1 = ERROR (仅错误)
// 2 = INFO  (错误+信息+警告，默认)
// 3 = DEBUG (错误+信息+警告+调试)

#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif
```

### 使用示例

```c
#include "app/log.h"

// 错误日志
LOG_ERROR_PRINT("[CONFIG] ❌ Flash 写入失败: %d\n", ret);

// 警告日志
LOG_WARN_PRINT("[SENSOR] ⚠️ 传感器重新初始化（第%d次）\n", retry_count);

// 信息日志
LOG_INFO_PRINT("[CONFIG] ✅ 配置已保存到 Flash (DPI=%d)\n", config.dpi);

// 调试日志
LOG_DEBUG_PRINT("[HID] 收到 Report ID=%d, 长度=%d\n", report_id, length);
```

### 模块名约定

日志消息中应包含模块名，格式为 `[模块名]`，便于过滤和定位：

| 模块名 | 说明 |
|--------|------|
| `[MAIN]` | 主程序 |
| `[CONFIG]` | 配置管理 |
| `[HID]` | HID 协议处理 |
| `[USB]` | USB 回调 |
| `[SENSOR]` | 光学传感器 |
| `[KEYBOARD]` | 键盘处理 |
| `[ENCODER]` | 编码器处理 |
| `[JOYSTICK]` | 摇杆处理 |
| `[LED]` | LED 管理 |
| `[FLASH]` | Flash 存储 |
| `[IPC]` | 核间通信 |
| `[FAULT]` | 故障处理 |
| `[PERF]` | 性能监控 |

---

## 上位机日志规范

### 日志类型

上位机包含两种日志：

1. **应用程序日志**：用于开发调试，通过 `System.Diagnostics.Debug.WriteLine` 输出
2. **固件错误日志**：从设备读取的错误记录，通过 `ErrorLogEntry` 模型表示

### 应用程序日志

**输出方式**: `System.Diagnostics.Debug.WriteLine`  
**查看方式**: Visual Studio 输出窗口 / 调试器

### 使用示例

```csharp
// 错误日志
Debug.WriteLine($"[ERROR] [DeviceService] HID 写入失败: {ex.Message}");

// 警告日志
Debug.WriteLine($"[WARN] [DeviceService] 设备响应超时，准备重试（第{retry}次）");

// 信息日志
Debug.WriteLine($"[INFO] [ConfigService] 配置已保存到文件: {filePath}");

// 调试日志
Debug.WriteLine($"[DEBUG] [DeviceService] 发送命令: 0x{cmd:X2}, 参数: 0x{param:X4}");
```

### 固件错误日志

**模型定义**: `pc_tool/shared/HidConfigTool.Core/Models/ErrorLogEntry.cs`  
**存储位置**: 固件 Flash 中的错误日志区  
**读取方式**: 通过 `IDeviceService.GetAllErrorLogsAsync()` 读取

### ErrorLogEntry 结构

| 字段 | 类型 | 说明 |
|------|------|------|
| `Index` | byte | 日志索引 |
| `IsValid` | bool | 是否有效 |
| `TimestampMs` | uint | 时间戳（毫秒，从设备启动开始） |
| `TimeFormatted` | string | 格式化时间（mm:ss.fff） |
| `Level` | byte | 错误级别（0=INFO, 1=WARN, 2=ERROR, 3=FATAL） |
| `LevelName` | string | 级别名称 |
| `LevelColor` | string | 级别颜色（用于UI显示） |
| `ModuleLength` | byte | 模块名长度 |
| `Module` | string | 模块名 |
| `Message` | string | 消息内容 |

### 固件错误日志格式

固件错误日志的二进制格式：

```
| 字节偏移 | 字段 | 长度 | 说明 |
|----------|------|------|------|
| 0 | valid | 1 | 有效性标志（0=无效, 1=有效） |
| 1 | level | 1 | 错误级别 |
| 2 | timestamp | 4 | 时间戳（毫秒，小端） |
| 6 | module_len | 1 | 模块名长度 |
| 7 | module | N | 模块名（ASCII） |
| 7+N | message | M | 消息内容（ASCII，以\0结尾） |
```

---

## 日志级别定义

### 统一级别定义

固件和上位机使用统一的日志级别定义：

| 级别 | 值 | 名称 | 颜色 | 说明 | 示例 |
|------|-----|------|------|------|------|
| DEBUG | 3 | DEBUG | 灰色 | 详细调试信息，仅开发版本输出 | 变量值、函数调用轨迹 |
| INFO | 2 | INFO | 蓝色 | 一般信息，正常操作记录 | 配置保存成功、设备连接 |
| WARN | 1 | WARN | 橙色 | 警告信息，可能需要关注 | 传感器重新初始化、重试操作 |
| ERROR | 0 | ERROR | 红色 | 错误信息，操作失败 | Flash 写入失败、通信超时 |
| FATAL | - | FATAL | 紫色 | 致命错误，系统无法继续 | 栈溢出、硬件故障 |

### 级别使用原则

- **DEBUG**: 仅在开发调试时使用，发布版本应关闭
- **INFO**: 记录正常的关键操作，便于了解系统运行状态
- **WARN**: 记录非致命但需要关注的情况，可能预示潜在问题
- **ERROR**: 记录操作失败，需要用户或开发者介入
- **FATAL**: 记录系统级致命错误，通常伴随系统重启

---

## 日志格式规范

### 固件日志格式

```
[级别] [模块] 消息\n
```

**示例**：
```
[ERR] [CONFIG] ❌ Flash 写入失败: -1
[WARN] [SENSOR] ⚠️ 传感器重新初始化（第2次）
[INFO] [CONFIG] ✅ 配置已保存到 Flash (DPI=800)
[DBG] [HID] 收到 Report ID=5, 长度=64
```

### 上位机应用日志格式

```
[级别] [模块] 消息
```

**示例**：
```
[ERROR] [DeviceService] HID 写入失败: 设备未连接
[WARN] [DeviceService] 设备响应超时，准备重试（第1次）
[INFO] [ConfigService] 配置已保存到文件: config.json
[DEBUG] [DeviceService] 发送命令: 0x05, 参数: 0x0000
```

### 固件错误日志显示格式

上位机显示固件错误日志时的格式：

```
[mm:ss.fff] [级别] [模块] 消息
```

**示例**：
```
[00:15.234] [ERROR] [CONFIG] Flash 写入失败
[01:23.456] [WARN] [SENSOR] 传感器初始化超时
[02:34.567] [INFO] [MAIN] 系统启动完成
```

### 消息内容规范

1. **使用中文**：面向用户的日志消息使用中文
2. **包含关键信息**：错误日志应包含错误码、参数值等关键信息
3. **使用表情符号**：可使用 ✅ ❌ ⚠️ 等表情符号增强可读性
4. **避免敏感信息**：不要在日志中输出密码、密钥等敏感信息
5. **控制长度**：单条日志消息不超过 200 字符

---

## 日志最佳实践

### 1. 何时记录日志

**应该记录**：
- 系统启动和关闭
- 配置保存和加载
- 设备连接和断开
- 错误和异常情况
- 重要状态变化
- 重试操作

**不应该记录**：
- 高频循环中的普通操作（如每次 HID 报告处理）
- 敏感信息（密码、密钥）
- 可从其他日志推断的冗余信息
- 调试完成后不再需要的临时日志

### 2. 日志消息质量

**好的日志**：
```c
LOG_ERROR_PRINT("[CONFIG] ❌ Flash 写入失败: ret=%d, addr=0x%08X\n", ret, addr);
LOG_INFO_PRINT("[CONFIG] ✅ 配置已保存 (DPI=%d, 死区=%d)\n", config.dpi, config.deadzone);
```

**差的日志**：
```c
LOG_ERROR_PRINT("error\n");  // 无模块名、无错误码、无上下文
LOG_INFO_PRINT("ok\n");       // 无意义的消息
```

### 3. 错误日志规范

错误日志应包含：
- 错误发生的位置（模块名）
- 错误码或错误描述
- 相关参数值
- 可能的原因或建议

**示例**：
```c
LOG_ERROR_PRINT("[CONFIG] ❌ CRC 校验失败: 计算=0x%08X, 期望=0x%08X\n",
                calc_crc, config.crc32);
```

### 4. 性能敏感代码中的日志

在高频调用的函数中，应使用 `LOG_DEBUG_PRINT`，并确保发布版本中 `LOG_LEVEL <= 2`，避免影响性能。

**示例**：
```c
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, ...)
{
    // 调试日志，发布版本中不输出
    LOG_DEBUG_PRINT("[HID] set_report: id=%d, len=%d\n", report_id, len);
    
    // ... 处理逻辑
}
```

---

## 日志性能优化

### 固件端优化

1. **编译时级别控制**：通过 `LOG_LEVEL` 宏在编译时移除高级别日志，避免运行时开销
2. **使用 `do-while(0)`**：确保宏在 `if/else` 中行为正确
3. **避免字符串拼接**：使用 `printf` 格式字符串，避免运行时字符串拼接
4. **限制日志长度**：单条日志不超过 200 字符，避免串口缓冲区溢出
5. **异步输出**：考虑使用 DMA 或环形缓冲区实现异步日志输出，减少阻塞

### 上位机端优化

1. **条件编译**：使用 `#if DEBUG` 包裹调试日志
2. **使用 `Debug.WriteLine`**：仅在调试版本中输出，发布版本自动移除
3. **避免频繁日志**：在高频循环中不要记录日志
4. **使用结构化日志**：考虑使用 `Microsoft.Extensions.Logging` 实现结构化日志

### 日志存储优化

固件错误日志存储在 Flash 中时：
1. **使用环形缓冲区**：避免 Flash 磨损均衡问题
2. **限制日志数量**：最多存储 32 条错误日志
3. **压缩存储**：使用二进制格式，避免文本存储浪费空间
4. **批量写入**：积累多条日志后批量写入 Flash，减少写入次数

---

## 附录

### A. 相关文件

| 文件 | 说明 |
|------|------|
| `firmware/include/app/log.h` | 固件日志宏定义 |
| `firmware/src/middleware/fault.c` | 固件错误日志记录 |
| `pc_tool/shared/HidConfigTool.Core/Models/ErrorLogEntry.cs` | 上位机错误日志模型 |
| `pc_tool/shared/HidConfigTool.Core/Services/IDeviceService.cs` | 错误日志读取接口 |
| `pc_tool/windows/HidConfigTool.App/Views/ErrorLogPage.xaml` | 错误日志显示页面 |

### B. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-08-24 | 初始版本，包含固件日志规范、上位机日志规范、日志级别定义、格式规范、最佳实践和性能优化 |

---

**文档结束**
