# 快速上手指南

本文档帮助你快速上手 Pico HID Composite 项目。

---

## 🚀 5 分钟快速开始

### 1. 克隆仓库

```bash
git clone <repository-url>
cd pico_hid_composite
```

### 2. 编译固件

**方式一：一键脚本（推荐）**

```powershell
# Windows PowerShell
.\build.ps1 -Target Firmware
```

**方式二：VS Code + Pico 扩展**

1. 用 VS Code 打开 `firmware/` 目录
2. 安装 Raspberry Pi Pico 扩展
3. 点击底部状态栏的 `Compile Project` 按钮

编译完成后，固件位于：
```
build/pico_hid_composite.uf2
```

### 3. 烧录固件

**方式一：UF2 拖拽（最简单）**

1. 按住 Pico 板上的 **BOOTSEL** 键
2. 插入 USB 线
3. 电脑会出现一个名为 `RPI-RP2` 的 U 盘
4. 将 `build/pico_hid_composite.uf2` 拖进去
5. 设备自动重启，烧录完成

**方式二：VS Code 一键烧录**

1. 按住 Pico 板上的 **BOOTSEL** 键
2. 插入 USB 线
3. 点击底部状态栏的 `Run Project` 按钮
4. 自动编译 + 烧录 + 运行

**方式二：picotool 命令行**

```powershell
# 按住 BOOTSEL 插入后执行
picotool load build\pico_hid_composite.uf2 -f
```

### 4. 编译上位机

```powershell
.\build.ps1 -Target PcTool
```

或：

```bash
cd pc_tool
dotnet build
```

### 5. 运行上位机

```bash
dotnet run --project pc_tool/windows/HidConfigTool.App
```

---

## 📦 项目结构速览

```
pico_hid_composite/
├── firmware/          # 🔧 固件代码（C 语言）
│   ├── include/       # 头文件（六层架构）
│   ├── src/           # 源文件
│   └── CMakeLists.txt
│
├── pc_tool/           # 🖥️ 上位机（C# / WPF + Avalonia）
│   ├── HidConfigTool.slnx
│   ├── shared/         # 共享层
│   │   ├── HidConfigTool.Core/      # 核心模型、接口、协议
│   │   ├── HidConfigTool.Hid/       # HID 驱动封装
│   │   └── HidConfigTool.ViewModels/ # 共享视图模型
│   ├── windows/        # Windows 专用 UI (WPF)
│   │   └── HidConfigTool.App/
│   ├── desktop/        # 跨平台 UI (Avalonia)
│   │   └── HidConfigTool.Desktop/
│   └── tests/          # 单元测试
│       └── HidConfigTool.Tests/
│
├── docs/              # 📚 文档
│   ├── QUICKSTART.md  # 本文档
│   ├── SETUP.md       # 开发环境配置
│   ├── FEATURES.md    # 功能清单
│   ├── HID_PROTOCOL.md # HID 协议文档
│   ├── ROADMAP.md     # 开发路线图
│   └── ...
│
├── .github/           # ⚙️ GitHub 配置
│   └── workflows/     # CI/CD
│
└── build.ps1          # 🚀 一键构建脚本
```

---

## 🎯 常见场景

### 场景 1：我只想用固件

1. 下载 Release 中的 `.uf2` 文件
2. 按 BOOTSEL 插入 Pico
3. 拖拽烧录
4. 完成！设备就是一个 USB HID 复合设备

### 场景 2：我想修改固件

1. 用 VS Code 打开 `firmware/` 目录
2. 修改代码
3. 点击 Build 编译
4. 烧录测试

**常用文件位置**：
- 引脚定义：`firmware/include/board/pins.h`
- HID 描述符：`firmware/src/protocol/usb_descriptors.c`
- 主函数：`firmware/src/app/main.c`
- 配置管理：`firmware/src/middleware/config.c`

### 场景 3：我想修改上位机

1. 用 Visual Studio / Rider 打开 `pc_tool/HidConfigTool.slnx`
2. 修改代码
3. 按 F5 运行调试

### 场景 4：我想了解 HID 协议

阅读 [HID 协议文档](HID_PROTOCOL.md)

---

## 🔧 开发环境

### 必备工具

| 工具 | 用途 | 备注 |
|------|------|------|
| VS Code | 固件开发 | 需装 Pico 扩展 |
| .NET 10 SDK | 上位机开发 | |
| Git | 版本控制 | |

### 可选工具

| 工具 | 用途 | 备注 |
|------|------|------|
| Visual Studio 2022 | 上位机开发 | 社区版即可 |
| Rider | 上位机开发 | 商业软件 |
| Python 3.10+ | 脚本工具 | 可选 |
| OpenOCD | 调试 | Pico 扩展自带 |

---

## 📚 更多文档

| 文档 | 说明 |
|------|------|
| [SETUP.md](SETUP.md) | 完整的开发环境配置指南 |
| [FEATURES.md](FEATURES.md) | 详细的功能清单 |
| [HID_PROTOCOL.md](HID_PROTOCOL.md) | HID 协议详细说明 |
| [ROADMAP.md](ROADMAP.md) | 开发路线图 |
| [CODING-STANDARDS.md](CODING-STANDARDS.md) | 编码规范 |
| [wiring_diagram.md](wiring_diagram.md) | 硬件接线图 |
| [DEVLOG.md](DEVLOG.md) | 开发日志 |

---

## ❓ 常见问题

### Q: 编译报错找不到 Pico SDK？

**A**: 使用 `build.ps1` 脚本，它会自动检测并下载 SDK。

### Q: Pico 扩展不识别项目？

**A**: 单独打开 `firmware/` 目录，而不是项目根目录。

### Q: 上位机连不上设备？

**A**:
1. 确认固件已正确烧录
2. 确认设备已被系统识别（设备管理器中查看）
3. 检查 VID/PID 是否匹配（VID=0xCafe, PID=0x4004）

### Q: 如何恢复出厂固件？

**A**: 重新烧录 `.uf2` 文件即可，配置会自动重置为默认值。

---

## 🤝 遇到问题？

1. 先看 [常见问题](#-常见问题)
2. 查看 [开发日志](DEVLOG.md) 中的已知问题
3. 提交 Issue（使用 Bug 报告模板）

---

## 📝 下一步

- 阅读 [功能清单](FEATURES.md) 了解全部功能
- 阅读 [HID 协议文档](HID_PROTOCOL.md) 了解通信协议
- 阅读 [编码规范](CODING-STANDARDS.md) 了解代码风格
- 查看 [路线图](ROADMAP.md) 了解未来计划

---

*最后更新：2026-08-09*
