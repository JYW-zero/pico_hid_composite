# 配置 HID 协议规范（v2.0）

## 一、协议概述

配置 HID 协议用于上位机与固件之间的配置读写、设备控制、版本协商等功能。

- **协议版本**：2.0（主版本 2，次版本 0）
- **传输方式**：USB HID Feature Report（配置读写）+ Output Report（ACK 响应）
- **配置接口**：HID Interface 1（Vendor 定义）

## 二、Report ID 分配

| Report ID | 名称 | 类型 | 大小 | 说明 |
|-----------|------|------|------|------|
| 1 | Keyboard | Input | 8 | 标准键盘报告（6KRO） |
| 2 | Mouse | Input | 5 | 标准鼠标报告 |
| 3 | Consumer | Input | 2 | 多媒体键报告 |
| 4 | Gamepad | Input | 8 | 游戏手柄报告 |
| 5 | Config Block0 | Feature | 63 | 配置块 0（偏移 0-61） |
| 6 | Device Info | Feature | 16 | 设备信息（版本/配置大小等） |
| 7 | Control | Feature | 63 | 控制命令 |
| 8 | Config Block1 | Feature | 63 | 配置块 1（偏移 62-123） |
| 9 | Config Block2 | Feature | 63 | 配置块 2（偏移 124-185） |
| 10-13 | Key Stats | Feature | 33 | 按键统计（4块，每块16键） |
| 14 | Macro Config | Feature | 63 | 宏配置读写 |
| 15 | Perf System | Feature | 32 | 性能监控-系统状态 |
| 16 | Perf Task | Feature | 32 | 性能监控-任务统计 |
| 17 | Fault Info | Feature | 16 | 错误日志-信息 |
| 18 | Fault Log | Feature | 63 | 错误日志-读取 |
| 19 | Key State | Feature | 9 | 实时按键状态（64位bitmap） |
| 20 | Joystick State | Feature | 9 | 实时摇杆状态 |
| 21 | Config Ext | Feature | 19 | 配置扩展字段（偏移 1326+） |
| 22 | NKRO | Input/Output | 36 | NKRO 位图键盘报告 |
| 23 | Joystick Calib | Feature | 11 | 摇杆校准状态查询 |
| 24 | ACK | Output | 9 | ACK/NACK 响应（固件主动发送） |

## 三、设备信息报告（Report ID 6）

上位机通过 GET_REPORT 读取设备信息，用于识别设备和版本协商。

```c
typedef struct __attribute__((packed))
{
    uint8_t  report_id;        // = 6
    uint16_t firmware_version; // 固件版本（bcdDevice，如 0x0400 = 4.0.0）
    uint16_t config_version;   // 配置结构版本（如 0x0007 = v7）
    uint16_t protocol_version; // 协议版本（高字节=主版本，低字节=次版本）
    uint16_t config_size;      // 配置结构总大小（字节）
    uint8_t  layer_count;      // 支持的按键层数
    uint8_t  macro_count;      // 支持的宏数量
    uint8_t  max_dpi;          // 最大支持 DPI 档位索引
    uint8_t  reserved[3];      // 保留
} config_hid_device_info_t;
```

## 四、控制命令（Report ID 7）

上位机通过 SET_REPORT 发送控制命令，固件通过 ACK 报告（Report ID 24）返回执行结果。

### 4.1 命令格式

```
byte[0] = Report ID (7)
byte[1] = 命令码
byte[2-3] = 参数（小端，可选）
```

### 4.2 命令码列表

| 命令码 | 名称 | 参数 | 说明 | 需解锁 |
|--------|------|------|------|--------|
| 0x01 | SAVE_CONFIG | - | 保存配置到 Flash | 是 |
| 0x02 | RESET_CONFIG | - | 恢复出厂默认配置 | 是 |
| 0x03 | REBOOT | - | 重启设备（需3次确认） | 否 |
| 0x04 | ENTER_DFU | - | 进入 DFU 模式（需3次确认） | 否 |
| 0x05 | APPLY_CONFIG | - | 应用临时配置（写入 Flash 并生效） | 是 |
| 0x06 | RESET_STATS | - | 清零按键统计 | 是 |
| 0x07 | CLEAR_FAULT | - | 清除错误日志 | 是 |
| 0x08 | RESET_PERF | - | 重置性能统计 | 是 |
| 0x09 | MACRO_PLAY | 宏ID | 播放宏 | 是 |
| 0x0A | MACRO_STOP | 宏ID/0xFF | 停止宏（0xFF=停止所有） | 是 |
| 0x0B | SET_PERF_ENABLE | 1/0 | 设置性能监控开关 | 否 |
| 0x0C | SET_JOYSTICK_DZ_RT | 死区值 | 实时设置摇杆死区（不写Flash） | 否 |
| 0x0D | UNLOCK_CONFIG | - | 解锁配置写入（需连续3次，5秒内） | 否 |
| 0x0E | JOYSTICK_CALIBRATE | 采样次数 | 触发摇杆中心点校准 | 否 |
| 0x0F | SWITCH_DPI | 档位/0xFF | 循环切换 DPI 档位 | 否 |
| 0x10 | GET_VERSION | - | 获取版本信息（返回 ACK 携带协议版本） | 否 |
| 0x11 | NEGOTIATE_VERSION | 协议版本 | 版本协商 | 否 |
| 0x12 | SWITCH_LAYER | 层号(0-3) | 切换按键层 | 否 |
| 0x13 | GET_CAPABILITY_INFO | 能力ID索引 | 获取能力信息（返回能力版本） | 否 |
| 0x14 | ENTER_BOOTSEL | - | 进入官方 BOOTSEL UF2 模式（需3次确认，软件模拟按住BOOTSEL键上电） | 否 |

### 4.3 配置锁定机制

- 默认锁定，防止恶意程序篡改配置
- 解锁方式：连续发送 3 次 CMD_UNLOCK_CONFIG（0x0D），5 秒内完成
- 解锁后 30 秒无写操作自动重新锁定
- 锁定状态下，只有标记为"否"的命令可以执行

## 五、ACK/NACK 响应（Report ID 24）

固件在收到控制命令后，通过 Output 报告返回执行结果。上位机应等待此报告后再发送下一个命令。

```c
typedef struct __attribute__((packed))
{
    uint8_t  report_id;   // = 24
    uint8_t  cmd;         // 对应的命令码
    uint8_t  status;      // 0=ACK(成功), 1=NACK(失败)
    uint8_t  error_code;  // 错误码，成功时为 0
    uint16_t param;       // 附加参数（如当前 DPI、层号等）
    uint8_t  reserved[2]; // 保留
} config_hid_ack_t;
```

### 5.1 错误码定义

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0 | OK | 成功 |
| 1 | INVALID_CMD | 无效命令 |
| 2 | INVALID_PARAM | 无效参数 |
| 3 | CONFIG_LOCKED | 配置已锁定 |
| 4 | CRC_MISMATCH | CRC 校验失败 |
| 5 | INCOMPLETE | 配置块不完整 |
| 6 | FLASH_WRITE | Flash 写入失败 |
| 7 | TIMEOUT | 操作超时 |
| 8 | BUSY | 设备忙 |
| 9 | NOT_SUPPORTED | 功能不支持 |
| 10 | VERSION_MISMATCH | 版本不兼容 |
| 0xFF | UNKNOWN | 未知错误 |

## 六、版本协商流程

### 6.1 协商步骤

1. 上位机读取设备信息（GET_REPORT Report ID 6），获取固件协议版本
2. 上位机发送 CMD_NEGOTIATE_VERSION（0x11），参数为上位机支持的协议版本
3. 固件比较主版本号：
   - 相同：返回 ACK + 固件协议版本（兼容）
   - 不同：返回 NACK + VERSION_MISMATCH + 固件协议版本（不兼容）
4. 上位机根据结果决定是否继续操作

### 6.2 版本兼容性规则

- **主版本号相同**：向下兼容，次版本号高的一方可以使用低版本的所有功能
- **主版本号不同**：不兼容，上位机应提示用户升级固件或上位机

### 6.3 协议版本历史

| 版本 | 说明 |
|------|------|
| 1.0 | 初始版本，基础配置读写和控制命令 |
| 2.0 | 增加 ACK/NACK 机制、版本协商、4层按键映射、多档 DPI |

## 七、配置块传输

### 7.1 配置块分配

当前配置大小为 1492 字节（v7），使用以下 Report ID 传输：

| Report ID | 偏移范围 | 大小 |
|-----------|----------|------|
| 5 (Block0) | 0-61 | 62 |
| 8 (Block1) | 62-123 | 62 |
| 9 (Block2) | 124-185 | 62 |
| 21 (Config Ext) | 1326-1343 | 18 |

### 7.2 大配置分段传输方案

对于超过 186 字节的配置（当前 1492 字节），推荐使用以下方案：

**方案 A：扩展 Config Ext 报告（推荐）**

使用 Report ID 21 作为扩展块索引，通过参数指定块号：

```
SET_REPORT Report ID 21:
  byte[0] = Report ID (21)
  byte[1] = 块索引 (0-24)
  byte[2-63] = 配置数据（62字节）
```

**方案 B：使用宏配置报告（Report ID 14）**

复用宏配置报告的分块传输机制，通过 block 参数指定偏移。

### 7.3 配置写入流程

1. 上位机发送 CMD_UNLOCK_CONFIG 3 次解锁配置
2. 上位机分块发送配置数据（SET_REPORT）
3. 上位机发送 CMD_APPLY_CONFIG（0x05）
4. 固件校验配置完整性和 CRC
5. 固件写入 Flash 并应用配置
6. 固件返回 ACK/NACK

## 八、上位机解析参考（C#）

### 8.1 读取设备信息

```csharp
public async Task<DeviceInfo?> GetDeviceInfoAsync(CancellationToken ct = default)
{
    byte[] buffer = await _hidDriver.GetFeatureReportAsync(6, ct);
    if (buffer.Length < 16) return null;

    return new DeviceInfo
    {
        FirmwareVersion = BitConverter.ToUInt16(buffer, 1),
        ConfigVersion = BitConverter.ToUInt16(buffer, 3),
        ProtocolVersion = BitConverter.ToUInt16(buffer, 5),
        ConfigSize = BitConverter.ToUInt16(buffer, 7),
        LayerCount = buffer[9],
        MacroCount = buffer[10],
        MaxDpi = buffer[11]
    };
}
```

### 8.2 发送控制命令并等待 ACK

```csharp
public async Task<AckResult?> SendControlCommandAsync(byte cmd, ushort param = 0,
    CancellationToken ct = default)
{
    // 发送命令
    byte[] data = new byte[3];
    data[0] = cmd;
    data[1] = (byte)(param & 0xFF);
    data[2] = (byte)((param >> 8) & 0xFF);
    await _hidDriver.SetFeatureReportAsync(7, data, ct);

    // 等待 ACK（超时 5 秒）
    using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
    cts.CancelAfter(5000);
    try
    {
        byte[] ack = await _hidDriver.ReadOutputReportAsync(24, cts.Token);
        if (ack.Length < 8) return null;
        return new AckResult
        {
            Cmd = ack[1],
            Success = ack[2] == 0,
            ErrorCode = ack[3],
            Param = BitConverter.ToUInt16(ack, 4)
        };
    }
    catch (OperationCanceledException)
    {
        return null; // 超时
    }
}
```

### 8.3 版本协商

```csharp
public async Task<bool> NegotiateVersionAsync(CancellationToken ct = default)
{
    var info = await GetDeviceInfoAsync(ct);
    if (info == null) return false;

    ushort hostVersion = (2 << 8) | 0; // 上位机协议版本 2.0
    var ack = await SendControlCommandAsync(0x11, hostVersion, ct);

    if (ack == null) return false;
    if (!ack.Success && ack.ErrorCode == 10) // VERSION_MISMATCH
    {
        Console.WriteLine($"版本不兼容：上位机 2.0，固件 {ack.Param >> 8}.{ack.Param & 0xFF}");
        return false;
    }
    return ack.Success;
}
```

## 九、向后兼容性

- 所有新增命令使用未占用的命令码（0x10+），不影响旧命令
- ACK 报告（Report ID 24）是新增的 Output 报告，旧上位机忽略即可
- 设备信息报告（Report ID 6）扩展了字段，但前 7 字节保持兼容
- 配置锁定机制默认开启，但旧上位机可以通过 CMD_UNLOCK_CONFIG 解锁
- 版本协商是可选的，旧上位机可以跳过协商直接使用

## 十、修改文件清单

| 类型 | 文件 | 说明 |
|------|------|------|
| 修改 | `include/protocol/config_hid.h` | 协议规范定义（错误码/ACK结构/命令码） |
| 修改 | `include/protocol/usb_descriptors.h` | 新增 REPORT_ID_ACK = 24 |
| 修改 | `src/protocol/usb_descriptors.c` | 新增 ACK Output 报告描述符 |
| 修改 | `src/protocol/config_hid.c` | ACK 发送、新命令处理、设备信息完善 |

## 十一、验证

- ✅ 固件编译通过
- ✅ ACK/NACK 响应机制
- ✅ 版本协商（CMD_GET_VERSION / CMD_NEGOTIATE_VERSION）
- ✅ 层切换命令（CMD_SWITCH_LAYER）
- ✅ 设备信息报告完善（固件/配置/协议版本）
- ✅ 错误码定义（12种错误码）
- ✅ 向后兼容性（旧命令/旧报告不受影响）
