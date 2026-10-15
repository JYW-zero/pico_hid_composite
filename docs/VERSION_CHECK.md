# 设备版本检查与兼容性管理

## 一、概述

在设备连接后自动进行版本检查，防止不兼容的固件版本导致配置读写错误或设备异常。支持协议版本兼容性检查、固件版本过旧提示、配置读写权限控制等功能。

## 二、功能特性

### 2.1 自动版本检查

设备连接成功后，自动读取设备信息报告（Report ID 6），解析以下版本信息：

| 字段 | 说明 | 示例 |
|------|------|------|
| 固件版本 | bcdDevice 格式 | 4.0.0 |
| 配置版本 | 配置结构版本号 | 7 |
| 协议版本 | HID 通信协议版本 | 2.0 |
| 配置大小 | 配置结构总字节数 | 1492 |
| 层数 | 支持的按键层数 | 4 |
| 宏数 | 支持的宏数量 | 8 |
| 最大 DPI | 最大 DPI 档位索引 | 64 |

### 2.2 兼容性检查

根据读取到的版本信息，自动进行兼容性检查：

| 检查项 | 条件 | 结果 |
|--------|------|------|
| 协议主版本匹配 | 设备协议主版本 == 上位机支持的协议主版本 | 必须匹配 |
| 最低固件版本 | 固件版本 >= 3.0.0 | 必须满足 |
| 推荐固件版本 | 固件版本 >= 4.0.0 | 建议满足 |

### 2.3 兼容性结果

```csharp
public enum CompatibilityResult
{
    Compatible,           // 完全兼容
    FirmwareOutdated,     // 协议兼容，但固件版本过旧（建议升级）
    ProtocolIncompatible, // 协议不兼容（主版本不同，必须升级）
    Unknown               // 无法读取版本信息
}
```

### 2.4 配置读写权限控制

根据兼容性检查结果，自动控制配置读写权限：

| 兼容性结果 | 允许读取 | 允许写入 | 说明 |
|-----------|---------|---------|------|
| Compatible | ✅ | ✅ | 完全兼容，正常使用 |
| FirmwareOutdated | ✅ | ✅ | 协议兼容，允许读写但提示升级 |
| ProtocolIncompatible | ❌ | ❌ | 协议不兼容，禁止所有配置操作 |
| Unknown | ❌ | ❌ | 未知状态，禁止配置操作 |

### 2.5 UI 提示

- **状态栏**：显示版本检查结果和提示消息
- **版本警告**：协议不兼容或固件过旧时显示警告
- **操作禁用**：不兼容时自动禁用配置保存按钮

## 三、核心实现

### 3.1 VersionComparer（版本比较工具类）

位置：`shared/HidConfigTool.Core/Utils/VersionComparer.cs`

主要功能：

```csharp
// 从设备信息报告数据解析版本信息
DeviceVersionInfo ParseDeviceInfo(byte[] data);

// 检查设备版本兼容性
CompatibilityResult CheckCompatibility(DeviceVersionInfo deviceInfo);

// 获取兼容性检查的用户可读提示
string GetCompatibilityMessage(CompatibilityResult result, DeviceVersionInfo? deviceInfo);

// 判断是否允许配置读写操作
bool IsConfigAccessAllowed(CompatibilityResult result);

// 比较两个版本号
int CompareVersions(Version v1, Version v2);

// 解析版本字符串
Version? ParseVersionString(string versionString);
```

### 3.2 DeviceVersionInfo（版本信息模型）

```csharp
public class DeviceVersionInfo
{
    public Version FirmwareVersion { get; set; }    // 固件版本
    public int ConfigVersion { get; set; }            // 配置版本
    public Version ProtocolVersion { get; set; }      // 协议版本
    public int ConfigSize { get; set; }               // 配置大小
    public int LayerCount { get; set; }               // 层数
    public int MacroCount { get; set; }               // 宏数
    public int MaxDpi { get; set; }                   // 最大 DPI
    public bool IsValid { get; }                       // 是否有效
}
```

### 3.3 IDeviceService 接口扩展

```csharp
public interface IDeviceService : IDisposable
{
    // 版本信息
    DeviceVersionInfo? VersionInfo { get; }

    // 兼容性检查结果
    CompatibilityResult Compatibility { get; }

    // 是否允许配置读写
    bool IsConfigAccessAllowed { get; }

    // 版本检查完成事件
    event EventHandler<CompatibilityResult>? VersionCheckCompleted;
}
```

### 3.4 DeviceService 实现

**连接时版本检查流程**：

```
设备连接成功
    │
    ▼
读取设备信息报告 (Report ID 6)
    │
    ├─ 成功 → 解析版本信息 → 兼容性检查 → 触发 VersionCheckCompleted 事件
    │           │
    │           ├─ 协议不兼容 → 状态栏显示 ⚠️ 警告
    │           ├─ 固件过旧 → 状态栏显示 💡 提示
    │           └─ 完全兼容 → 正常使用
    │
    └─ 失败 → 标记为 Unknown → 触发 VersionCheckCompleted 事件
```

**配置写入前检查**：

```csharp
private async Task<bool> WriteConfigToDeviceAsync(DeviceConfig config)
{
    // 版本兼容性检查：协议不兼容时禁止写入
    if (!IsConfigAccessAllowed)
    {
        string compatMsg = VersionComparer.GetCompatibilityMessage(Compatibility, VersionInfo);
        OnOperationStatusChanged($"❌ 无法写入配置：{compatMsg}");
        return false;
    }

    // 继续正常写入流程...
}
```

### 3.5 MainViewModel UI 绑定

```csharp
public partial class MainViewModel : ObservableObject
{
    // 兼容性检查结果
    [ObservableProperty]
    private CompatibilityResult _compatibility = CompatibilityResult.Unknown;

    // 设备版本信息
    [ObservableProperty]
    private DeviceVersionInfo? _versionInfo;

    // 版本检查提示消息
    [ObservableProperty]
    private string _versionMessage = string.Empty;

    // 是否显示版本警告
    [ObservableProperty]
    private bool _showVersionWarning;
}
```

## 四、版本配置

### 4.1 上位机支持的版本

```csharp
// 上位机支持的最低固件版本
public static readonly Version MinimumSupportedFirmwareVersion = new(3, 0, 0);

// 上位机推荐的最低固件版本
public static readonly Version RecommendedFirmwareVersion = new(4, 0, 0);

// 上位机支持的协议版本（主版本必须匹配）
public static readonly Version SupportedProtocolVersion = new(2, 0);
```

### 4.2 设备信息报告格式

Report ID 6，16 字节：

| 偏移 | 字段 | 大小 | 说明 |
|------|------|------|------|
| 0 | report_id | 1 | = 6 |
| 1-2 | firmware_version | 2 | bcdDevice 格式（小端） |
| 3-4 | config_version | 2 | 配置结构版本（小端） |
| 5-6 | protocol_version | 2 | 高字节=主版本，低字节=次版本 |
| 7-8 | config_size | 2 | 配置结构大小（小端） |
| 9 | layer_count | 1 | 支持的按键层数 |
| 10 | macro_count | 1 | 支持的宏数量 |
| 11 | max_dpi | 1 | 最大 DPI 档位索引 |
| 12-15 | reserved | 4 | 保留 |

## 五、修改文件清单

| 类型 | 文件 | 说明 |
|------|------|------|
| 新建 | `shared/HidConfigTool.Core/Utils/VersionComparer.cs` | 版本比较工具类 |
| 修改 | `shared/HidConfigTool.Core/Interfaces/IDeviceService.cs` | 增加版本检查属性和事件 |
| 修改 | `shared/HidConfigTool.Core/Services/DeviceService.cs` | 实现版本检查逻辑 |
| 修改 | `shared/HidConfigTool.ViewModels/MainViewModel.cs` | UI 绑定版本检查结果 |

## 六、验证

- ✅ 设备连接后自动读取版本信息
- ✅ 协议不兼容时禁止配置读写
- ✅ 固件过旧时提示建议升级
- ✅ 版本检查结果在 UI 中显示
- ✅ 状态栏显示版本检查提示
- ✅ 配置写入前进行兼容性检查
- ✅ 设备断开时重置版本信息
- ✅ 编译通过（0 错误）
- ✅ 向后兼容（旧固件标记为 Unknown，禁止配置操作）

## 七、使用建议

### 7.1 固件升级提示

当检测到固件版本过旧时，应引导用户到固件升级页面：

```csharp
if (Compatibility == CompatibilityResult.FirmwareOutdated)
{
    // 显示升级提示，引导用户到 FirmwareUpdate 页面
    StatusMessage = "💡 固件版本过旧，建议升级到最新版本";
}
```

### 7.2 协议不兼容处理

当检测到协议不兼容时，应明确告知用户必须升级：

```csharp
if (Compatibility == CompatibilityResult.ProtocolIncompatible)
{
    // 显示严重警告，禁止所有配置操作
    StatusMessage = "⚠️ 协议版本不兼容，请升级固件或使用兼容版本的上位机";
}
```

### 7.3 调试技巧

- 查看日志：版本检查结果会记录到 `%AppData%\HIDConfigTool\Logs\device_service_*.log`
- 版本信息：日志中会记录完整的设备版本信息（固件、协议、配置版本等）
- 兼容性判断：日志中会记录兼容性检查结果和原因

## 八、后续优化方向

1. **自动升级检测**：连接时检查是否有新固件版本可用，提示用户升级
2. **版本回退保护**：防止用户刷入过旧的固件版本
3. **功能特性检测**：根据固件版本动态显示/隐藏不支持的功能
4. **多语言支持**：版本提示消息支持多语言
5. **版本历史记录**：记录设备连接历史和版本变化
