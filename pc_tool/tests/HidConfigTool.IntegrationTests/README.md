# HID 设备模拟器与集成测试

## 概述

本目录包含 HID 设备模拟器和上位机集成测试，用于在没有真实硬件的情况下测试上位机的完整功能流程。

## 项目结构

```
HidConfigTool.IntegrationTests/
├── HidConfigTool.IntegrationTests.csproj  # 测试项目文件
├── Simulator/
│   ├── SimulatedHidDevice.cs              # 模拟设备核心（状态管理、配置存储、命令处理）
│   ├── SimulatedHidDriver.cs              # 模拟 HID 驱动（实现 IHidDriver 接口）
│   └── DeviceBehavior.cs                  # 设备行为配置（正常、超时、错误响应等）
└── Tests/
    ├── DeviceConnectionTests.cs           # 设备连接测试（发现、连接、断开、重连）
    ├── DeviceConfigReadWriteTests.cs      # 配置读写测试（读取、修改、保存、重置）
    └── FirmwareUpgradeTests.cs            # 固件升级测试（DFU 模式、重启、设备重置）
```

## 模拟器架构

### SimulatedHidDevice

模拟设备核心类，负责：

- **设备状态管理**：连接状态、配置锁定状态、性能监控开关、DPI 档位、摇杆死区等
- **配置存储**：1510 字节配置数据（v8），支持分块读写
- **命令处理**：处理所有 HID 控制命令（解锁、保存、重置、重启、DFU 等）
- **故障日志**：记录设备运行过程中的错误和警告
- **操作日志**：记录所有 HID 操作，用于测试验证
- **固件升级**：支持 DFU 模式和固件数据回显

### SimulatedHidDriver

实现 `IHidDriver` 接口，将上位机的 HID 调用转发到模拟设备：

- `FindDevicesAsync`：返回模拟设备列表（支持断开行为）
- `OpenAsync` / `Close`：打开/关闭设备
- `SendFeatureReportAsync`：发送 Feature 报告到模拟设备
- `GetFeatureReportAsync`：从模拟设备读取 Feature 报告
- 支持 `CancellationToken` 取消操作
- 支持延迟模拟（超时行为）

### DeviceBehavior

设备行为配置，支持多种故障模拟：

| 行为模式 | 说明 |
|---------|------|
| `Normal` | 正常响应 |
| `Timeout` | 超时（延迟超过超时时间） |
| `ErrorResponse` | 返回错误数据（CRC 错误、数据不完整、全零、随机数据、null） |
| `Disconnected` | 设备断开（所有操作失败） |
| `Intermittent` | 间歇性故障（随机失败） |

## 使用方法

### 基本使用

```csharp
// 创建模拟设备
var device = new SimulatedHidDevice();

// 创建模拟驱动
var driver = new SimulatedHidDriver(device);

// 创建设备服务
var deviceService = new DeviceService(driver);

// 查找设备
var devices = await deviceService.GetDevicesAsync();

// 连接设备
await deviceService.ConnectAsync(devices[0]);

// 读取配置
var config = deviceService.CurrentConfig;

// 修改配置
config.Dpi = 1600;

// 保存配置
await deviceService.SaveConfigAsync(config);
```

### 故障模拟

```csharp
// 模拟设备断开
device.Behavior.Mode = DeviceBehaviorMode.Disconnected;

// 模拟超时
device.Behavior.Mode = DeviceBehaviorMode.Timeout;
device.Behavior.TimeoutDelayMs = 5000; // 5秒延迟

// 模拟 CRC 错误
device.Behavior.Mode = DeviceBehaviorMode.ErrorResponse;
device.Behavior.ErrorType = ErrorResponseType.CrcMismatch;

// 模拟间歇性故障（30% 失败率）
device.Behavior.Mode = DeviceBehaviorMode.Intermittent;
device.Behavior.IntermittentFailureProbability = 0.3;

// 恢复正常
device.Behavior.Reset();
```

### 直接测试模拟设备

```csharp
// 直接发送控制命令
device.HandleSendFeatureReport(0x07, new byte[] { 0x0D, 0x00, 0x00 }); // 解锁配置

// 直接读取 Feature 报告
byte[]? deviceInfo = device.HandleGetFeatureReport(0x06); // 读取设备信息

// 模拟按键按下
device.SimulateKeyPress(0); // 按下键 0

// 检查操作日志
Assert.Contains(device.OperationLog, log => log.Contains("UNLOCK"));

// 检查故障日志
var logs = device.GetFaultLogs();
```

## 运行测试

### 运行所有集成测试

```bash
cd pc_tool
dotnet test tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj
```

### 运行特定测试类

```bash
dotnet test tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj --filter "FullyQualifiedName~DeviceConnectionTests"
```

### 运行特定测试方法

```bash
dotnet test tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj --filter "FullyQualifiedName~ConnectAsync_WithValidDevice_ShouldSucceed"
```

### 查看详细测试输出

```bash
dotnet test tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj --verbosity normal
```

## 测试覆盖范围

### DeviceConnectionTests（20 个测试）

- 设备发现：匹配 VID/PID、不匹配 VID/PID、设备断开、设备信息验证
- 设备连接：正常连接、连接失败、断开连接
- 设备信息读取：固件版本、配置数据
- 重连测试：断开后重连、多次连接断开
- 设备状态：连接前状态、设备名称

### DeviceConfigReadWriteTests（25 个测试）

- 配置读取：有效配置、默认 DPI、默认死区
- 配置写入：有效配置、设备故障
- 配置重置：重置成功、恢复默认值
- 性能监控：开启/关闭
- 摇杆死区：实时设置、归零
- DPI 切换：循环切换、指定档位（直接测试模拟设备）
- 故障模拟：设备断开、错误响应
- 模拟设备直接测试：解锁配置、重置配置、操作日志、按键统计

### FirmwareUpgradeTests（12 个测试）

- DFU 模式：进入 DFU、清除固件数据、添加故障日志（直接测试模拟设备）
- 重启：不抛出异常、断开设备、重连（直接测试模拟设备）
- 固件升级流程：完整流程、设备故障、设备断开（直接测试模拟设备）
- 设备重置：恢复默认状态、清除操作日志
- 设备行为配置：设置模式、重置、错误类型

## 扩展模拟器

### 添加新的 Report ID 处理

在 `SimulatedHidDevice.cs` 的 `HandleGetFeatureReport` 和 `HandleSendFeatureReport` 方法中添加新的 case：

```csharp
case REPORT_ID_CUSTOM:
    return HandleCustomReport(data);
```

### 添加新的控制命令

在 `HandleControlCommand` 方法中添加新的命令处理：

```csharp
case CMD_CUSTOM:
    return HandleCustomCommand(parameter);
```

### 添加新的设备行为

在 `DeviceBehaviorMode` 枚举中添加新模式，并在 `ShouldFail()` 和 `GetDelayMs()` 方法中处理：

```csharp
public enum DeviceBehaviorMode
{
    Normal,
    Timeout,
    ErrorResponse,
    Disconnected,
    Intermittent,
    Custom // 新模式
}
```

## 注意事项

1. **VID/PID 匹配**：模拟器使用的 VID/PID 必须与 `DeviceService.DeviceVendorId` 和 `DeviceService.DeviceProductId` 一致（当前为 0x1209 / 0x4004）
2. **UsagePage 过滤**：`DeviceService` 只返回 `UsagePage == 0xFF00` 的设备，模拟器已正确设置
3. **配置大小**：模拟器配置大小为 1510 字节（v8），与 `DeviceService.CONFIG_SIZE` / 固件 `device_config_t` 一致
4. **线程安全**：模拟器设计为单线程测试使用，不保证多线程安全
5. **延迟模拟**：超时行为通过 `Task.Delay` 实现，会增加测试执行时间

## 相关文档

- [上位机单元测试](../HidConfigTool.Tests/README.md)
- [开发日志](../../../../docs/DEVLOG.md)
