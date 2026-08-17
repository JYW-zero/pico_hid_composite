# HID配置工具

HID 复合设备配置工具，支持 Windows (WPF) 和跨平台 (Avalonia) 两种 UI。

## 功能特性

- 设备识别与连接
- 按键映射配置（普通层 + Fn层）
- 鼠标DPI配置
- 摇杆死区配置
- 编码器方向配置
- 宏功能配置与录制
- 性能监控
- 错误日志查看
- 按键统计

## 项目结构

```
pc_tool/
├── HidConfigTool.slnx          # 解决方案文件
├── shared/                     # 共享层（所有UI项目引用）
│   ├── HidConfigTool.Core/     # 核心模型、接口、协议
│   ├── HidConfigTool.Hid/      # HID驱动封装（基于HidSharp）
│   └── HidConfigTool.ViewModels/  # 共享视图模型
├── windows/                    # Windows 专用 UI
│   └── HidConfigTool.App/      # WPF 应用
├── desktop/                    # 跨平台 UI
│   └── HidConfigTool.Desktop/  # Avalonia 应用
└── tests/                      # 单元测试
    └── HidConfigTool.Tests/
```

## 编译与运行

详细说明请查看 [编译运行指南](docs/BUILD.md)。

### Windows 专用版 (WPF)

```bash
cd pc_tool
dotnet build windows/HidConfigTool.App/HidConfigTool.App.csproj
dotnet run --project windows/HidConfigTool.App/HidConfigTool.App.csproj
```

### 跨平台版 (Avalonia，支持 Windows/macOS/Linux)

```bash
cd pc_tool
dotnet build desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj
dotnet run --project desktop/HidConfigTool.Desktop/HidConfigTool.Desktop.csproj
```

### 编译整个解决方案

```bash
cd pc_tool
dotnet build HidConfigTool.slnx
```

### 运行单元测试

```bash
cd pc_tool
dotnet test tests/HidConfigTool.Tests/HidConfigTool.Tests.csproj
```

## 文档

- [编译运行指南](docs/BUILD.md)
- [开发日志](../docs/DEVLOG.md)
- 完整文档请查看项目根目录的 [docs/](../docs/) 文件夹

## 架构说明

本项目采用"共享核心层 + 独立UI层"架构：

- **Core**：纯 .NET 类库，包含数据模型、平台抽象接口、设备通信协议
- **Hid**：HID 驱动封装，基于 HidSharp 跨平台库
- **ViewModels**：共享视图模型，所有 UI 逻辑在此实现，不依赖任何 UI 框架
- **Windows (WPF)**：Windows 专用 UI，实现平台接口
- **Desktop (Avalonia)**：跨平台 UI，实现平台接口

修改 UI 只需修改对应 UI 项目，接口驱动统一在共享层维护。

## 联系作者

- **作者**：JYW
- **邮箱**：[J.YW@outlook.com](mailto:J.YW@outlook.com)
