# Pico HID Composite

> 基于 RP2350（Pico 2）的 USB HID 复合设备固件 + Windows 上位机配置工具

<div align="center">

[![Build Status](https://img.shields.io/badge/CI-Passing-brightgreen)](https://github.com/JYW-zero/pico_hid_composite/actions)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-RP2350-red)](https://www.raspberrypi.com/products/raspberry-pi-pico-2/)
[![SDK](https://img.shields.io/badge/Pico%20SDK-2.3.0-green)](https://github.com/raspberrypi/pico-sdk)
[![.NET](https://img.shields.io/badge/.NET-10.0-purple)](https://dotnet.microsoft.com/)

</div>

---

## ✨ 功能特性

### 固件（RP2350 / Pico 2）

**输入设备：**

| 功能 | 说明 |
|------|------|
| **64键 SPI 键盘矩阵** | 74HC165 级联，硬件扫描 + 软件消抖 |
| **PAW3395 光学鼠标** | SPI1 接口，4 档 DPI（400/800/1600/3200） |
| **ADC 摇杆** | X/Y 轴 + 按键，12 位精度 |
| **旋转编码器** | A/B 相 + 中键 |
| **多媒体键** | 音量、播放控制等 |
| **游戏手柄** | 标准 HID Gamepad 报告 |

**配置与监控：**

| 功能 | 说明 |
|------|------|
| **配置存储** | 3 个配置块，共 186 字节，双备份 + CRC32 |
| **宏功能** | 8 个宏，每个 32 个动作，6 种动作类型 |
| **按键统计** | 64 键使用次数统计，Flash 持久化 |
| **性能监控** | CPU 使用率、任务统计、超时告警 |
| **错误日志** | Flash 持久化存储，掉电不丢失 |
| **工厂测试模式** | 一键进入全按键/传感器测试 |

**可靠性设计：**

- ✅ **双备份配置存储** + CRC32 校验，自动故障恢复
- ✅ **分层看门狗** + 故障自动恢复机制
- ✅ **Flash 磨损均衡**，延长使用寿命
- ✅ **双核架构** - Core1 硬件扫描，Core0 业务处理

### HID 报告描述符

共 19 个 Report ID：

| Report ID | 名称 | 方向 | 说明 |
|-----------|------|------|------|
| 1 | Keyboard | IN | 标准键盘报告 |
| 2 | Mouse | IN | 标准鼠标报告 |
| 3 | Consumer Control | IN | 多媒体控制 |
| 4 | Gamepad | IN | 游戏手柄 |
| 5 | Config Block 0 | IN/OUT | 配置块 0（偏移 0-62） |
| 6 | Device Info | IN | 设备信息 |
| 7 | Control | OUT | 控制命令 |
| 8 | Config Block 1 | IN/OUT | 配置块 1（偏移 63-125） |
| 9 | Config Block 2 | IN/OUT | 配置块 2（偏移 126-188） |
| 10-13 | Key Stats 0-3 | IN | 按键统计（64 键分 4 块） |
| 14 | Macro Config | IN/OUT | 宏配置读写 |
| 15 | Perf System | IN | 性能监控 - 系统状态 |
| 16 | Perf Task | IN | 性能监控 - 任务统计 |
| 17 | Fault Info | IN | 错误日志 - 信息 |
| 18 | Fault Log | IN | 错误日志 - 读取日志 |

### 上位机（Windows / .NET 10 / WPF）

- 🎨 **深色主题**，Fluent 设计风格
- ⌨️ **按键设置** - 双手对称六向布局可视化配置
- 🖱️ **鼠标设置** - DPI 调节
- 🕹️ **摇杆设置** - 死区、曲线调节
- 🔄 **编码器设置** - 步长设置
- 🎬 **宏配置与录制** - 可视化宏编辑器
- 📊 **性能监控** - 实时 CPU 使用率、任务统计
- 📈 **按键统计** - 按键使用频率统计
- 📋 **错误日志查看** - 固件错误日志读取与分析
- 💾 **配置文件管理** - 导入/导出/备份

---

## 🔌 硬件接线

详细的接线说明请参考 [硬件接线图文档](docs/wiring_diagram.md)。

**快速总览：**

| 外设 | 接口 | 主要引脚 |
|------|------|---------|
| 64键键盘矩阵 | SPI0 | GP16/17/18/19 |
| PAW3395 鼠标 | SPI1 | GP10/11/12/13/14/15 |
| 双轴摇杆 | ADC | GP26/27/28 |
| 滚轮编码器 | GPIO | GP20/21/22 |

> 💡 所有外设均使用 3.3V 供电，请勿直接连接 5V。

---

## 🏗️ 项目架构

### 六层固件架构

```
┌─────────────────────────────────────────┐
│              App Layer                  │  应用逻辑（按键映射、宏、业务）
├─────────────────────────────────────────┤
│           Protocol Layer                │  🆕 USB HID 协议（描述符、报文）
├─────────────────────────────────────────┤
│          Middleware Layer               │  中间件（调度、电源、监控）
├─────────────────────────────────────────┤
│           Device Layer                  │  外设驱动（SPI、ADC、GPIO）
├─────────────────────────────────────────┤
│            Board Layer                  │  板级支持（引脚定义、硬件初始化）
├─────────────────────────────────────────┤
│          Hardware / SDK                 │  Pico SDK + TinyUSB
└─────────────────────────────────────────┘
```

### 目录结构

```
pico_hid_composite/
├── 📁 .github/                    # ⚙️ GitHub 配置
│   ├── 📁 ISSUE_TEMPLATE/         # Issue 模板
│   ├── 📁 workflows/              # CI/CD 工作流
│   ├── dependabot.yml             # 依赖自动更新
│   └── PULL_REQUEST_TEMPLATE.md   # PR 模板
│
├── 📁 .vscode/                    # 📝 VS Code 工作区配置
│
├── 📁 firmware/                   # 🔧 固件源码（C 语言）
│   ├── 📁 .vscode/                # 固件专用 VS Code 配置
│   ├── 📁 include/                # 头文件（分层）
│   │   ├── app/                   # 应用层
│   │   ├── board/                 # 板级支持
│   │   ├── device/                # 外设驱动
│   │   ├── middleware/            # 中间件
│   │   └── protocol/              # HID 协议层
│   ├── 📁 src/                    # 源文件（镜像 include 结构）
│   ├── 📁 lib/                    # SDK 版本锁定
│   ├── 📁 tests/                  # 单元测试（Unity 框架）
│   ├── 📁 scripts/                # 辅助脚本
│   ├── 📁 tools/                  # 工具脚本
│   ├── .clang-format              # 代码格式化配置
│   ├── CMakeLists.txt
│   └── README.md
│
├── 📁 pc_tool/                    # 🖥️ 上位机（C# / WPF）
│   ├── HidConfigTool.slnx         # 解决方案
│   └── 📁 src/
│       ├── HidConfigTool.App/     # UI 层（WPF / MVVM）
│       ├── HidConfigTool.Core/    # 核心业务层
│       └── HidConfigTool.Tests/   # 单元测试
│
├── 📁 docs/                       # 📚 项目文档
│   ├── FEATURES.md                # 功能详细说明
│   ├── ROADMAP.md                 # 开发路线图
│   ├── SETUP.md                   # 开发环境配置指南
│   ├── DEVLOG.md                  # 开发记录汇总
│   ├── CODING-STANDARDS.md        # 编码规范
│   ├── PROGRESS.md                # 项目进度报告
│   └── wiring_diagram.md          # 硬件接线图
│
├── 📁 tools/                      # 🛠️ 公共工具脚本
├── 📄 build.ps1                   # 🚀 一键构建脚本
├── 📄 .editorconfig               # 编辑器配置
├── 📄 .gitattributes              # Git 属性
├── 📄 .gitignore                  # Git 忽略规则
├── 📄 CHANGELOG.md                # 变更日志
├── 📄 LICENSE
└── 📄 README.md
```

---

## 🚀 快速开始

### 前置要求

- **Git**
- **PowerShell 5.1+**（Windows 自带）
- **ARM GNU Toolchain**（编译固件用，Pico 扩展会自动安装）
- **.NET 10 SDK**（编译上位机用）

### 一键构建

```powershell
# 克隆仓库
git clone https://github.com/JYW-zero/pico_hid_composite.git
cd pico_hid_composite

# 全部编译（固件 + 上位机）
.\build.ps1 -Target All

# 或只编译固件
.\build.ps1 -Target Firmware

# 或只编译上位机
.\build.ps1 -Target PcTool
```

> 💡 **智能 SDK 管理**：build.ps1 会自动检测 Pico SDK，找不到时自动从官方拉取指定版本，无需手动配置。

### 固件烧录

**方式一：picotool（推荐）**
```powershell
# 按住 BOOTSEL 键插入 Pico，然后执行
picotool load build\pico_hid_composite.uf2 -f
```

**方式二：UF2 拖拽**
1. 按住 BOOTSEL 键插入 Pico
2. 电脑会出现一个 U 盘
3. 将 `build/pico_hid_composite.uf2` 拖进去
4. 自动重启，烧录完成

### 上位机运行

```powershell
cd pc_tool
dotnet run --project src/HidConfigTool.App
```

---

## 📦 下载预编译版本

前往 [Releases](https://github.com/JYW-zero/pico_hid_composite/releases) 页面下载最新版本：

- `firmware-rp2350.zip` - 固件（.uf2 / .elf / .bin）
- `pc-tool-windows.zip` - 上位机（自包含单文件，无需安装 .NET）

---

## 🛠️ 开发环境

### 推荐开发方式

| 场景 | 推荐方式 |
|------|----------|
| **固件开发** | VS Code + Raspberry Pi Pico 扩展，打开 `firmware/` 目录 |
| **上位机开发** | Visual Studio 2022 或 Rider，打开 `pc_tool/HidConfigTool.slnx` |
| **整体编译** | 根目录执行 `.\build.ps1 -Target All` |

### 详细文档

| 文档 | 说明 |
|------|------|
| [快速上手指南](docs/QUICKSTART.md) | 5 分钟快速上手 |
| [开发环境配置](docs/SETUP.md) | 完整的环境配置指南 |
| [macOS 安装教程](docs/SETUP_MAC.md) | Mac 上安装工具链并编译固件 |
| [功能清单](docs/FEATURES.md) | 详细功能说明 |
| [HID 协议文档](docs/HID_PROTOCOL.md) | USB HID 协议详细说明 |
| [硬件接线图](docs/wiring_diagram.md) | 硬件接线说明 |
| [编码规范](docs/CODING-STANDARDS.md) | 代码风格规范 |
| [开发路线图](docs/ROADMAP.md) | 开发计划 |
| [开发日志](docs/DEVLOG.md) | 开发记录 |
| [项目进度](docs/PROGRESS.md) | 进度报告 |

---

## 📊 性能指标

| 指标 | 数值 |
|------|------|
| 主循环频率 | > 1000 Hz |
| 按键扫描延迟 | < 1 ms |
| 鼠标报告率 | 1000 Hz |
| CPU 使用率（空闲） | < 10% |
| 固件大小 | ~ 112 KB |
| RAM 占用 | ~ 20 KB |

---

## 🤝 贡献

欢迎贡献代码！请先阅读 [编码规范](docs/CODING-STANDARDS.md)。

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

---

## 📝 变更日志

查看 [CHANGELOG.md](CHANGELOG.md) 获取详细的版本更新记录。

---

## 📄 License

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 了解详情。

---

## 🙏 致谢

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [TinyUSB](https://github.com/hathach/tinyusb)
- [CommunityToolkit.Mvvm](https://github.com/CommunityToolkit/dotnet)

---

## 📧 联系作者

- **作者**：JYW
- **邮箱**：[J.YW@outlook.com](mailto:J.YW@outlook.com)

欢迎反馈问题、建议或交流技术！

---

<div align="center">

**如果这个项目对你有帮助，别忘了给个 ⭐ Star！**

</div>
