# 编译运行指南

本文档详细说明 HID 配置工具的两种版本（Windows 专用 WPF 版和跨平台 Avalonia 版）的编译、运行和发布方法。

---

## 环境要求

- **.NET SDK**：.NET 10.0 或更高版本
- **操作系统**：
  - WPF 版：Windows 10 1809+ / Windows 11
  - Avalonia 版：Windows 10+ / macOS 10.15+ / Linux

检查 .NET 版本：

```bash
dotnet --version
```

---

## 项目结构

```
pc_tool/
├── HidConfigTool.slnx              # 解决方案文件
├── shared/                         # 共享层
│   ├── HidConfigTool.Core/         # 核心模型、接口、协议（net10.0）
│   ├── HidConfigTool.Hid/          # HID驱动封装（net10.0）
│   └── HidConfigTool.ViewModels/   # 共享视图模型（net10.0）
├── windows/                        # Windows 专用
│   └── HidConfigTool.App/          # WPF 应用（net10.0-windows10.0.19041.0）
├── desktop/                        # 跨平台
│   └── HidConfigTool.Desktop/      # Avalonia 应用（net10.0）
└── tests/                          # 单元测试
    └── HidConfigTool.Tests/
```

---

## 一、Windows 专用版 (WPF)

### 1.1 编译

```bash
cd pc_tool

# 编译 WPF 版（Debug）
dotnet build windows/HidConfigTool.App/HidConfigTool.App.csproj

# 编译 WPF 版（Release）
dotnet build windows/HidConfigTool.App/HidConfigTool.App.csproj -c Release
```

### 1.2 运行

```bash
cd pc_tool
dotnet run --project windows/HidConfigTool.App/HidConfigTool.App.csproj
```

### 1.3 发布（生成独立可执行文件）

```bash
cd pc_tool

# 发布 Windows x64 单文件
dotnet publish windows/HidConfigTool.App/HidConfigTool.App.csproj `
  -c Release `
  -r win-x64 `
  --self-contained true `
  -p:PublishSingleFile=true `
  -p:IncludeNativeLibrariesForSelfExtract=true

# 输出目录：windows/HidConfigTool.App/bin/Release/net10.0-windows10.0.19041.0/win-x64/publish/
```

### 1.4 特点

- 使用 Windows 原生 P/Invoke HID 驱动，性能更好
- 支持 Windows 系统托盘、开机自启动、OSD 提示等平台特性
- 仅支持 Windows 系统

---

## 二、跨平台版 (Avalonia)

### 2.1 编译

```bash
cd pc_tool

# 编译 Avalonia 版（Debug）
dotnet build desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj

# 编译 Avalonia 版（Release）
dotnet build desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj -c Release
```

### 2.2 运行

```bash
cd pc_tool
dotnet run --project desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj
```

### 2.3 发布（生成独立可执行文件）

#### Windows

```bash
cd pc_tool
dotnet publish desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj `
  -c Release `
  -r win-x64 `
  --self-contained true `
  -p:PublishSingleFile=true

# 输出目录：desktop/HidConfigTool.Desktop/bin/Release/net10.0/win-x64/publish/
```

#### macOS (Intel)

```bash
cd pc_tool
dotnet publish desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj `
  -c Release `
  -r osx-x64 `
  --self-contained true `
  -p:PublishSingleFile=true
```

#### macOS (Apple Silicon)

```bash
cd pc_tool
dotnet publish desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj `
  -c Release `
  -r osx-arm64 `
  --self-contained true `
  -p:PublishSingleFile=true
```

#### Linux

```bash
cd pc_tool
dotnet publish desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj `
  -c Release `
  -r linux-x64 `
  --self-contained true `
  -p:PublishSingleFile=true
```

### 2.4 特点

- 使用 HidSharp 跨平台 HID 库
- 支持 Windows、macOS、Linux
- 平台特定功能（宏录制、应用感知等）为基础实现，后续可完善

---

## 三、编译整个解决方案

```bash
cd pc_tool

# 编译所有项目（Debug）
dotnet build HidConfigTool.slnx

# 编译所有项目（Release）
dotnet build HidConfigTool.slnx -c Release
```

> **注意**：如果遇到 .baml 缓存相关的编译错误（BG1002），请使用 `--no-restore` 参数：
> ```bash
> dotnet build HidConfigTool.slnx --no-restore
> ```

---

## 四、单元测试

```bash
cd pc_tool

# 运行所有测试
dotnet test tests/HidConfigTool.Tests/HidConfigTool.Tests.csproj

# 运行测试并生成详细输出
dotnet test tests/HidConfigTool.Tests/HidConfigTool.Tests.csproj -v normal
```

---

## 五、常见问题

### 5.1 编译错误：BG1002 无法解析类型

**原因**：WPF 项目的 .baml 编译缓存问题。

**解决**：清理 obj 目录后重新编译：

```bash
cd pc_tool
Remove-Item -Recurse -Force windows/HidConfigTool.App/obj
dotnet restore HidConfigTool.slnx
dotnet build HidConfigTool.slnx
```

### 5.2 运行时找不到设备

**原因**：HID 设备权限问题或驱动未正确安装。

**解决**：
- Windows：以管理员身份运行
- macOS：在"系统设置 > 隐私与安全性 > 输入监控"中授权应用
- Linux：添加 udev 规则或使用 sudo 运行

### 5.3 Avalonia 版在 macOS 上无法打开

**原因**：macOS 安全限制，未签名的应用会被阻止。

**解决**：右键点击应用，选择"打开"，或在终端中运行：

```bash
xattr -cr HidConfigTool.Desktop.app
```

---

## 六、开发说明

### 6.1 架构

本项目采用"共享核心层 + 独立UI层"架构：

- **Core**：数据模型、平台抽象接口（15个）、设备通信协议
- **Hid**：HID 驱动封装
- **ViewModels**：所有 UI 逻辑，14个共享 ViewModel
- **Windows (WPF)**：实现15个平台接口，WPF UI
- **Desktop (Avalonia)**：实现15个平台接口，Avalonia UI

### 6.2 平台接口列表

| 接口 | 说明 |
|------|------|
| IDialogService | 消息对话框 |
| ITimerService | 定时器 |
| IUiThreadService | UI线程调度 |
| IFileDialogService | 文件对话框 |
| IInputDialogService | 输入对话框 |
| IKeyPickerService | 按键选择对话框 |
| IHelpWindowService | 帮助窗口 |
| ITrayIconService | 系统托盘 |
| IThemeService | 主题切换 |
| ILanguageService | 语言切换 |
| IOsdService | OSD屏幕提示 |
| IAppAwarenessService | 应用感知 |
| IConfigProfileService | 配置文件管理 |
| IAutoStartService | 开机自启动 |
| IMacroRecorder | 宏录制 |

### 6.3 添加新功能

1. 在 Core 中定义数据模型和平台接口（如需要）
2. 在 ViewModels 中实现业务逻辑
3. 在 Windows 和 Desktop 项目中分别实现平台接口
4. 在两个 UI 项目中分别添加 UI 界面

---

## 七、版本信息

- **当前版本**：v0.1
- **.NET 版本**：.NET 10.0
- **WPF 版本**：兼容 Windows 10 1809+
- **Avalonia 版本**：11.3.2
