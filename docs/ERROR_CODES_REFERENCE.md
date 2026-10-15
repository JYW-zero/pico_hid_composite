# 错误码参考文档

**版本**: 1.0  
**最后更新**: 2026-08-24  
**适用范围**: Pico HID Composite 固件 + .NET 跨平台上位机

---

## 目录

1. [概述](#概述)
2. [固件配置错误码](#固件配置错误码)
3. [上位机错误类型](#上位机错误类型)
4. [Win32 HID 错误码映射](#win32-hid-错误码映射)
5. [错误码对照表](#错误码对照表)
6. [错误处理最佳实践](#错误处理最佳实践)
7. [常见问题排查](#常见问题排查)

---

## 概述

本文档统一描述 Pico HID Composite 项目中的所有错误码，包括：

- **固件配置错误码**：固件通过 HID ACK 包返回给上位机的配置操作错误码
- **上位机错误类型**：上位机内部错误分类，用于日志记录和用户提示
- **Win32 HID 错误码**：Windows HID API 返回的系统错误码，上位机通过 `HidErrorMapper` 映射为用户可读信息

所有错误码在固件和上位机之间保持一一对应，确保跨层错误传递的一致性。

---

## 固件配置错误码

**定义位置**: `firmware/include/protocol/config_hid.h`  
**类型**: `config_hid_error_t`（uint8_t）  
**传递方式**: HID ACK 包的第4字节（error_code）

| 错误码 | 名称 | 值 | 严重程度 | 说明 | 常见原因 | 处理建议 |
|--------|------|-----|---------|------|---------|---------|
| `CONFIG_HID_OK` | 操作成功 | 0 | - | 命令执行成功 | - | 无需处理 |
| `CONFIG_HID_ERR_INVALID_CMD` | 无效命令 | 1 | Medium | 收到未定义的命令字节 | 上位机发送了固件不支持的命令；协议版本不匹配 | 检查上位机与固件版本兼容性；更新固件或上位机 |
| `CONFIG_HID_ERR_INVALID_PARAM` | 无效参数 | 2 | Medium | 命令参数超出有效范围 | DPI 值超出 100-6400；摇杆死区超出 0-4095；DPI 档位超出 0-3 | 检查参数范围；使用配置验证服务校验 |
| `CONFIG_HID_ERR_CONFIG_LOCKED` | 配置已锁定 | 3 | Low | 配置处于锁定状态，拒绝修改 | 用户未先发送解锁命令；自动超时锁定已触发 | 先发送 `CMD_UNLOCK_CONFIG` 解锁；检查锁定超时时间 |
| `CONFIG_HID_ERR_CRC_MISMATCH` | CRC 校验失败 | 4 | High | 配置数据 CRC32 校验不通过 | 配置数据在传输中损坏；魔数错误；上位机计算 CRC 方式与固件不一致 | 重新发送配置块；检查上位机 CRC 计算逻辑；验证配置魔数 |
| `CONFIG_HID_ERR_INCOMPLETE` | 配置块不完整 | 5 | Medium | 未收到全部3个配置块就尝试应用配置 | 上位机只发送了部分配置块；USB 传输丢包 | 重新发送全部3个配置块（BLOCK0/BLOCK1/BLOCK2）；检查 USB 连接稳定性 |
| `CONFIG_HID_ERR_FLASH_WRITE` | Flash 写入失败 | 6 | High | 配置保存到 Flash 时失败 | Flash 硬件故障；写入地址越界；Flash 擦除失败 | 重试保存；检查 Flash 硬件；如果持续失败，可能需要更换芯片 |
| `CONFIG_HID_ERR_TIMEOUT` | 操作超时 | 7 | Medium | 命令执行超时 | Core1 无响应；IPC 通信超时；传感器初始化超时 | 检查 Core1 心跳状态；重启设备；检查传感器连接 |
| `CONFIG_HID_ERR_BUSY` | 设备忙 | 8 | Low | 设备正在处理其他命令，无法接受新命令 | 上一个命令尚未完成；Flash 写入进行中 | 等待当前操作完成后重试；实现命令队列 |
| `CONFIG_HID_ERR_NOT_SUPPORTED` | 功能不支持 | 9 | Low | 当前固件版本不支持该功能 | 上位机发送了新功能命令，但固件版本较旧 | 更新固件到支持该功能的版本；检查功能兼容性矩阵 |
| `CONFIG_HID_ERR_VERSION_MISMATCH` | 版本不兼容 | 10 | Medium | 配置版本号高于固件支持的版本 | 上位机生成了新版本配置，但固件版本较旧 | 更新固件；使用配置迁移服务降级配置版本 |
| `CONFIG_HID_ERR_UNKNOWN` | 未知错误 | 0xFF | High | 发生了未分类的错误 | 固件内部异常；未处理的边界情况 | 查看固件日志；重启设备；如果可复现，提交 bug 报告 |

### 错误码使用示例

```c
// 固件端：返回错误码
config_hid_send_ack(CMD_SAVE_CONFIG, false, CONFIG_HID_ERR_CRC_MISMATCH, 0);

// 上位机端：解析错误码
byte errorCode = ack[3];
if (errorCode == 0) {
    // 成功
} else {
    // 根据错误码显示对应提示
    string message = errorCode switch {
        1 => "无效命令",
        2 => "无效参数",
        3 => "配置已锁定，请先解锁",
        4 => "CRC 校验失败，请重新发送配置",
        5 => "配置块不完整，请重新发送全部配置",
        6 => "Flash 写入失败，请重试",
        7 => "操作超时，请检查设备连接",
        8 => "设备忙，请稍后重试",
        9 => "功能不支持，请更新固件",
        10 => "版本不兼容，请更新固件",
        _ => $"未知错误（错误码: {errorCode}）"
    };
}
```

---

## 上位机错误类型

**定义位置**: `pc_tool/shared/HidConfigTool.Core/Services/DeviceService.cs`  
**类型**: `ErrorType`（枚举）  
**用途**: 错误日志记录、用户提示分类、错误统计

| 错误类型 | 说明 | 典型场景 | 严重程度 |
|---------|------|---------|---------|
| `Communication` | 通信错误 | HID 读写失败、超时、设备断开 | High |
| `Protocol` | 协议错误 | Report ID 不匹配、数据长度异常、CRC 校验失败 | High |
| `Config` | 配置错误 | 配置结构不一致、字段偏移错误、魔数不匹配 | Medium |
| `Command` | 命令错误 | 命令执行失败、ACK 返回错误、未知命令 | Medium |
| `Dfu` | DFU 错误 | 升级失败、校验失败、固件无效 | High |
| `Device` | 设备错误 | 设备未连接、设备模式不匹配、版本不兼容 | Medium |
| `Internal` | 内部错误 | 空引用、数组越界、异常未捕获 | High |

### 错误记录结构

```csharp
public class ErrorRecord
{
    public DateTime Timestamp { get; set; }    // 错误发生时间
    public ErrorType Type { get; set; }         // 错误类型
    public string Location { get; set; }        // 发生位置（方法名）
    public string Message { get; set; }         // 错误消息
    public string? Details { get; set; }        // 详细信息（可选）
}
```

---

## Win32 HID 错误码映射

**定义位置**: `pc_tool/shared/HidConfigTool.Core/Utils/HidErrorMapper.cs`  
**用途**: 将 Windows HID API 返回的 Win32 错误码映射为用户可读的中文信息

### 通用错误

| 错误码 | 说明 | 可重试 |
|--------|------|--------|
| 0 | 操作成功 | - |
| 1 | 函数不正确 | 否 |
| 2 | 系统找不到指定的文件 | 否 |
| 3 | 系统找不到指定的路径 | 否 |
| 5 | 拒绝访问（设备可能被其他程序占用） | 否 |
| 6 | 句柄无效 | 否 |
| 8 | 存储空间不足，无法处理此命令 | 否 |
| 13 | 数据无效 | 否 |
| 21 | 设备未就绪 | 是 |
| 31 | 连到系统上的设备没有发挥作用 | 否 |
| 32 | 另一个程序正在使用此文件，进程无法访问 | 是 |
| 33 | 另一个程序已锁定文件的一部分，进程无法访问 | 是 |
| 50 | 不支持该请求 | 否 |
| 55 | 指定的网络资源或设备不再可用 | 是 |
| 59 | 发生了意外的网络错误 | 是 |
| 87 | 参数错误 | 否 |

### HID 特定错误

| 错误码 | 说明 | 可重试 |
|--------|------|--------|
| 121 | 信号灯超时时间已到（设备响应超时） | 是 |
| 995 | 由于线程退出或应用程序请求，已中止 I/O 操作 | 否 |
| 996 | 重叠 I/O 事件不在信号状态中 | 否 |
| 997 | 重叠 I/O 操作在进行中 | 否 |
| 1004 | 无效的标志 | 否 |
| 1167 | 设备未连接或已断开 | 是 |
| 1450 | 系统资源不足，无法完成请求的服务 | 是 |
| 1451 | 系统资源不足，无法完成请求的服务 | 否 |
| 1452 | 系统资源不足，无法完成请求的服务 | 否 |
| 1453 | 配额不足，无法完成请求的服务 | 否 |
| 1454 | 配额不足，无法完成请求的服务 | 否 |
| 1455 | 页面文件太小，无法完成操作 | 否 |

### USB 相关错误（自定义扩展）

| 错误码 | 说明 | 可重试 |
|--------|------|--------|
| 433 | 指定的设备不存在或未连接 | 否 |
| 434 | 设备连接已断开 | 否 |
| 435 | 设备忙，请稍后重试 | 是 |
| 436 | 设备通信超时 | 是 |
| 437 | 设备返回数据格式错误 | 否 |
| 438 | 设备固件版本不兼容 | 否 |
| 439 | 设备配置锁定，需要先解锁 | 否 |
| 440 | 设备 CRC 校验失败 | 否 |

### 使用示例

```csharp
// 获取错误消息
string message = HidErrorMapper.GetErrorMessage(errorCode);

// 判断是否可重试
if (HidErrorMapper.IsRetryable(errorCode)) {
    // 执行重试逻辑
}

// 获取最后一个 Win32 错误
string lastError = HidErrorMapper.GetLastErrorMessage();
```

---

## 错误码对照表

### 固件错误码 → 上位机错误类型

| 固件错误码 | 固件错误名称 | 上位机错误类型 | 上位机提示消息 |
|-----------|-------------|---------------|---------------|
| 0 | CONFIG_HID_OK | - | 操作成功 |
| 1 | CONFIG_HID_ERR_INVALID_CMD | Command | 无效命令，请检查固件版本兼容性 |
| 2 | CONFIG_HID_ERR_INVALID_PARAM | Config | 无效参数，请检查配置值范围 |
| 3 | CONFIG_HID_ERR_CONFIG_LOCKED | Config | 配置已锁定，请先点击解锁按钮 |
| 4 | CONFIG_HID_ERR_CRC_MISMATCH | Protocol | CRC 校验失败，请重新发送配置 |
| 5 | CONFIG_HID_ERR_INCOMPLETE | Protocol | 配置块不完整，请重新发送全部配置 |
| 6 | CONFIG_HID_ERR_FLASH_WRITE | Device | Flash 写入失败，请重试或重启设备 |
| 7 | CONFIG_HID_ERR_TIMEOUT | Communication | 操作超时，请检查设备连接 |
| 8 | CONFIG_HID_ERR_BUSY | Device | 设备忙，请稍后重试 |
| 9 | CONFIG_HID_ERR_NOT_SUPPORTED | Device | 功能不支持，请更新固件到最新版本 |
| 10 | CONFIG_HID_ERR_VERSION_MISMATCH | Device | 版本不兼容，请更新固件或上位机 |
| 0xFF | CONFIG_HID_ERR_UNKNOWN | Internal | 未知错误，请查看日志或重启设备 |

### Win32 错误码 → 上位机错误类型

| Win32 错误码 | 上位机错误类型 | 说明 |
|-------------|---------------|------|
| 5, 32, 33 | Communication | 设备被占用或拒绝访问 |
| 21, 55, 59, 1167 | Communication | 设备未就绪或已断开 |
| 121, 436 | Communication | 通信超时 |
| 13, 87, 437 | Protocol | 数据无效或格式错误 |
| 1450-1455 | Internal | 系统资源不足 |

---

## 错误处理最佳实践

### 1. 固件端错误处理

**原则**：
- 所有可能失败的操作都必须返回错误码
- 错误码必须通过 ACK 包明确告知上位机
- 严重错误（如 Flash 写入失败）应记录到固件日志

**示例**：
```c
// 正确：检查返回值并返回错误码
int ret = config_save(&config);
if (ret != 0) {
    LOG_ERROR_PRINT("[CONFIG] Flash 写入失败: %d\n", ret);
    config_hid_send_ack(cmd, false, CONFIG_HID_ERR_FLASH_WRITE, 0);
    return;
}
config_hid_send_ack(cmd, true, CONFIG_HID_OK, 0);

// 错误：忽略返回值
config_save(&config);  // 可能失败但未处理
config_hid_send_ack(cmd, true, CONFIG_HID_OK, 0);
```

### 2. 上位机端错误处理

**原则**：
- 所有 HID 读写操作都必须捕获异常
- 根据错误类型决定是否重试
- 向用户显示清晰的中文错误提示
- 记录详细错误信息到日志

**示例**：
```csharp
try
{
    byte[] ack = await SendCommandAsync(cmd, data, timeoutMs);
    byte errorCode = ack.Length > 3 ? ack[3] : (byte)0;
    
    if (errorCode != 0)
    {
        string message = GetConfigErrorMessage(errorCode);
        _logger.LogError("命令 {Cmd} 失败: {Message} (错误码: {ErrorCode})", cmd, message, errorCode);
        throw new DeviceCommandException(message, errorCode);
    }
}
catch (IOException ex) when (HidErrorMapper.IsRetryable(ex))
{
    _logger.LogWarning(ex, "HID 通信失败，准备重试");
    // 执行重试逻辑
}
catch (TimeoutException ex)
{
    _logger.LogError(ex, "命令 {Cmd} 超时", cmd);
    throw new DeviceCommunicationException("设备响应超时，请检查连接", ex);
}
```

### 3. 重试策略

| 错误类型 | 是否重试 | 最大重试次数 | 退避策略 |
|---------|---------|-------------|---------|
| Communication（超时） | 是 | 3 | 指数退避（100ms, 200ms, 400ms） |
| Communication（设备断开） | 否 | - | 提示用户重新连接 |
| Protocol（CRC 失败） | 是 | 2 | 立即重试 |
| Config（参数错误） | 否 | - | 提示用户修正参数 |
| Device（Flash 写入失败） | 是 | 1 | 等待 500ms 后重试 |
| Internal | 否 | - | 记录日志，提示用户重启 |

### 4. 错误日志规范

**日志格式**：
```
[时间戳] [错误类型] [位置] 错误消息
详细信息: ...
```

**示例**：
```
[14:30:25] [Communication] [DeviceService.SendCommandAsync] HID 写入失败
详细信息: Win32 错误码 1167 - 设备未连接或已断开
```

---

## 常见问题排查

### Q1: 上位机提示"CRC 校验失败"

**可能原因**：
1. 配置数据在 USB 传输中损坏
2. 上位机计算 CRC 的方式与固件不一致
3. 配置魔数错误

**排查步骤**：
1. 检查 USB 连接是否稳定，尝试更换 USB 线或端口
2. 确认上位机使用 `crc32_calc()` 计算 CRC，且忽略最后4字节（crc32 字段本身）
3. 检查配置魔数是否为 `0x5A5A5A5A`
4. 查看固件日志，确认收到的配置数据是否完整

### Q2: 上位机提示"配置已锁定"

**可能原因**：
1. 用户未先发送解锁命令
2. 自动超时锁定已触发（默认5分钟无操作）

**排查步骤**：
1. 点击上位机的"解锁配置"按钮
2. 检查锁定超时设置，必要时调整超时时间
3. 确认解锁命令（`CMD_UNLOCK_CONFIG`）是否成功发送

### Q3: 上位机提示"设备未连接"但设备已插入

**可能原因**：
1. 设备被其他程序占用
2. USB 驱动问题
3. 设备固件未正常运行

**排查步骤**：
1. 关闭其他可能占用 HID 设备的程序（如其他配置工具、游戏等）
2. 在设备管理器中检查 HID 设备是否正常识别
3. 尝试重新插拔设备
4. 如果设备进入 DFU 模式，需要先退出 DFU 模式

### Q4: 固件日志显示"Flash 写入失败"

**可能原因**：
1. Flash 硬件故障
2. 写入地址越界
3. Flash 擦除失败

**排查步骤**：
1. 重启设备后重试
2. 检查配置大小是否超过 Flash 扇区大小
3. 使用调试器检查 Flash 写入函数的返回值
4. 如果持续失败，可能需要更换芯片

### Q5: 上位机提示"版本不兼容"

**可能原因**：
1. 上位机版本过新，生成了固件不支持的配置版本
2. 固件版本过旧，不支持新功能

**排查步骤**：
1. 更新固件到最新版本
2. 或使用旧版本上位机
3. 使用配置迁移服务将配置降级到固件支持的版本

---

## 附录

### A. 相关文件

| 文件 | 说明 |
|------|------|
| `firmware/include/protocol/config_hid.h` | 固件配置错误码定义 |
| `firmware/src/protocol/config_hid.c` | 固件配置命令处理 |
| `pc_tool/shared/HidConfigTool.Core/Services/DeviceService.cs` | 上位机错误类型定义 |
| `pc_tool/shared/HidConfigTool.Core/Utils/HidErrorMapper.cs` | Win32 错误码映射 |
| `pc_tool/shared/HidConfigTool.Core/Models/ErrorLogEntry.cs` | 错误日志条目模型 |

### B. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-08-24 | 初始版本，包含固件配置错误码、上位机错误类型、Win32 HID 错误码映射 |

---

**文档结束**
