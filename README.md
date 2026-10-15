# Pico HID Composite

> 基于 RP2350（Pico 2）的 USB HID 复合设备固件 + 跨平台上位机配置工具（WPF + Avalonia）

<div align="center">

[![CI](https://github.com/JYW-zero/pico_hid_composite/actions/workflows/ci.yml/badge.svg)](https://github.com/JYW-zero/pico_hid_composite/actions/workflows/ci.yml)
[![Firmware](https://img.shields.io/badge/Firmware-Build%20%7C%20Test%20%7C%20Analysis-blue)](https://github.com/JYW-zero/pico_hid_composite/actions/workflows/ci.yml)
[![PC Tool](https://img.shields.io/badge/PC%20Tool-Build%20%7C%20Test%20%7C%20Analysis-purple)](https://github.com/JYW-zero/pico_hid_composite/actions/workflows/ci.yml)
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
| **64键 SPI 键盘矩阵** | 74HC165 级联，硬件扫描 + 软件消抖，支持 NKRO |
| **PAW3395 光学鼠标** | SPI1 接口，4 档 DPI（400/800/1600/3200），LOD/角度捕捉 |
| **ADC 摇杆** | X/Y 轴 + 按键，12 位精度，死区/灵敏度/中心点校准 |
| **旋转编码器** | A/B 相 + 中键，步长/滚动速度可调 |
| **多媒体键** | 音量、播放控制等 |
| **游戏手柄** | 标准 HID Gamepad 报告 |

**配置与协议（v7，1492字节）：**

| 功能 | 说明 |
|------|------|
| **配置存储** | 1492 字节（v7），4 层按键映射，CRC32 校验 |
| **CONFIG_EXT** | v3/v4/v5 扩展字段（18字节：摇杆/编码器/长度/写入次数/CRC） |
| **CONFIG_EXT2** | v6/v7 扩展字段（子块索引机制，5个子块：DPI档位/Layer2/Layer3 keymap） |
| **ACK 命令响应** | 所有控制命令返回 ACK（Report ID 24，Input报告），含状态/错误码/参数 |
| **协议版本协商** | 上位机与固件协商 HID 协议版本，确保兼容性 |
| **宏功能** | 8 个宏，每个 32 个动作，6 种动作类型 |
| **按键统计** | 64 键使用次数统计，Flash 持久化 |
| **性能监控** | CPU 使用率、任务统计、超时告警 |
| **错误日志** | Flash 持久化存储，掉电不丢失 |

**Bootloader 与 DFU 升级：**

| 功能 | 说明 |
|------|------|
| **自定义 Bootloader** | 32KB，位于 Flash 起始区域，USB HID DFU 协议 |
| **DFU 升级** | 上位机通过 HID Feature 报告升级固件，支持擦除/写入/校验/完成 |
| **CRC32 校验** | 升级数据 CRC32 校验，失败可回滚 |
| **UF2 兼容** | 按住 BOOTSEL 进入 bootrom UF2 模式，兼容官方烧录方式 |
| **应用有效性检测** | Bootloader 检测应用程序栈指针和 Reset_Handler，无效则停在 DFU 模式 |
| **固件版本读取** | 通过设备信息 Report 读取固件版本和协议版本 |

**可靠性设计：**

- ✅ **CRC32 配置校验**，自动故障恢复
- ✅ **看门狗** + 故障自动恢复机制
- ✅ **Flash 磨损均衡**，延长使用寿命
- ✅ **双核架构** - Core1 硬件扫描，Core0 业务处理
- ✅ **双备份启动** - Bootloader + 应用程序分离，升级失败不砖

---

### HID 报告描述符（25 个 Report ID）

**接口 0（标准 HID）：**

| Report ID | 名称 | 方向 | 大小 | 说明 |
|-----------|------|------|------|------|
| 1 | Keyboard | Input | 8B | 标准键盘报告 |
| 2 | Mouse | Input | 5B | 标准鼠标报告 |
| 3 | Consumer Control | Input | 2B | 多媒体控制 |
| 4 | Gamepad | Input | 8B | 游戏手柄 |
| 22 | NKRO Keyboard | Input | 35B | NKRO 位图键盘（256键） |

**接口 1（配置接口，Vendor Defined 0xFF00）：**

| Report ID | 名称 | 方向 | 大小 | 说明 |
|-----------|------|------|------|------|
| 5 | Config Block 0 | Feature | 62B | 配置块 0（偏移 0-61） |
| 6 | Device Info | Feature | 32B | 设备信息（版本/协议版本/UID） |
| 7 | Control | Feature | 3B | 控制命令（cmd + param_low + param_high） |
| 8 | Config Block 1 | Feature | 62B | 配置块 1（偏移 62-123） |
| 9 | Config Block 2 | Feature | 62B | 配置块 2（偏移 124-185） |
| 10-13 | Key Stats 0-3 | Feature | 32B | 按键统计（64 键分 4 块） |
| 14 | Macro Config | Feature | 62B | 宏配置读写 |
| 15 | Perf System | Feature | 62B | 性能监控 - 系统状态 |
| 16 | Perf Task | Feature | 62B | 性能监控 - 任务统计 |
| 17 | Fault Info | Feature | 62B | 错误日志 - 信息 |
| 18 | Fault Log | Feature | 62B | 错误日志 - 读取日志 |
| 19 | Key State | Feature | 8B | 实时按键状态（64位bitmap） |
| 20 | Joystick State | Feature | 8B | 实时摇杆状态 |
| 21 | Config EXT | Feature | 18B | v3/v4/v5 扩展字段 |
| 23 | Joystick Calib | Feature | 10B | 摇杆校准状态 |
| 24 | ACK | Input | 8B | 命令执行结果响应（固件主动发送） |
| 25 | Config EXT2 | Feature | 63B | v6/v7 扩展字段（子块索引机制） |

**Bootloader DFU Report ID：**

| Report ID | 名称 | 方向 | 说明 |
|-----------|------|------|------|
| 0x30 | DFU Status | Feature | 状态查询/响应 |
| 0x31 | DFU Erase | Feature | 擦除命令 |
| 0x32 | DFU Data | Feature | 数据写入 |
| 0x33 | DFU Verify | Feature | 校验命令 |
| 0x34 | DFU Finish | Feature | 完成/重启 |
| 0x35 | DFU Abort | Feature | 中止升级 |
| 0x36 | DFU Boot App | Feature | 启动应用程序 |

---

### 上位机（.NET 10 / WPF + Avalonia 双平台）

**双 UI 架构：**

| 平台 | UI 框架 | 说明 |
|------|---------|------|
| Windows | WPF | 原生 Windows 体验，Fluent 设计风格 |
| 跨平台 | Avalonia | Windows/macOS/Linux，共享 Core 层 |

**核心功能：**

- 🎨 **深色/浅色主题**，多语言支持（中文/英文）
- ⌨️ **按键设置** - 4 层按键映射可视化配置
- 🖱️ **鼠标设置** - 4 档 DPI 调节、LOD、角度捕捉
- 🕹️ **摇杆设置** - 死区、灵敏度、中心点校准、实时坐标显示
- 🔄 **编码器设置** - 方向、步长、滚动速度
- 🎬 **宏配置与录制** - 可视化宏编辑器
- 📊 **性能监控** - 实时 CPU 使用率、任务统计、超时告警
- 📈 **按键统计** - 按键使用频率统计
- 📋 **错误日志查看** - 固件错误日志读取与导出
- 💾 **配置文件管理** - 导入/导出/备份
- 🔄 **DFU 固件升级** - 完整升级流程，进度显示，CRC校验
- 📱 **设备模式检测** - 自动识别应用程序/Bootloader 模式
- ⚠️ **错误记录系统** - 统一错误记录、历史查询、统计分析、UI实时提示

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
| UF2 触发按钮 | GPIO | GP2（可选，上电时拉低进入 bootrom UF2） |

> 💡 所有外设均使用 3.3V 供电，请勿直接连接 5V。

---

## 📐 Flash 分区布局（2MB）

```
0x10000000 ┌─────────────────────────────┐
            │    Bootloader (32KB)        │  自定义 USB HID DFU Bootloader
0x10008000 ├─────────────────────────────┤
            │    DFU 元数据 (4KB)         │  升级状态/CRC/版本信息
0x10009000 ├─────────────────────────────┤
            │                             │
            │    应用程序 (1948KB)        │  主固件（键鼠复合设备）
            │                             │
0x101F0000 ├─────────────────────────────┤
            │    用户配置 (64KB)           │  配置/按键统计/错误日志
0x10200000 └─────────────────────────────┘
```

**关键地址：**
- Bootloader 起始：`0x10000000`
- 应用程序起始：`0x10009000`
- 用户配置起始：`0x101F0000`
- Flash 总大小：`2MB`（2097152 字节）

---

## 🏗️ 项目架构

### 六层固件架构

```
┌─────────────────────────────────────────┐
│              App Layer                  │  应用逻辑（按键映射、宏、业务）
├─────────────────────────────────────────┤
│           Protocol Layer                │  USB HID 协议（描述符、报文、ACK）
├─────────────────────────────────────────┤
│          Middleware Layer               │  中间件（调度、电源、监控）
├─────────────────────────────────────────┤
│           Device Layer                  │  外设驱动（SPI、ADC、GPIO、DFU）
├─────────────────────────────────────────┤
│            Board Layer                  │  板级支持（引脚定义、硬件初始化）
├─────────────────────────────────────────┤
│          Hardware / SDK                 │  Pico SDK + TinyUSB
└─────────────────────────────────────────┘
```

### 上位机分层架构

```
┌─────────────────────────────────────────┐
│         UI 层（双平台）                  │
│  ┌──────────────┐  ┌──────────────┐   │
│  │  WPF (Win)   │  │ Avalonia     │   │
│  │  HidConfig   │  │ (跨平台)     │   │
│  └──────────────┘  └──────────────┘   │
├─────────────────────────────────────────┤
│         ViewModels (MVVM)               │  视图模型，共享逻辑
├─────────────────────────────────────────┤
│         Core (业务核心)                  │  设备服务、配置、DFU、协议
├─────────────────────────────────────────┤
│         Hid (HID驱动)                    │  HidSharp 跨平台 / WPF 原生
├─────────────────────────────────────────┤
│         Interfaces (接口)                │  IDeviceService, IHidDriver 等
└─────────────────────────────────────────┘
```

### 目录结构

```
pico_hid_composite/
├── 📁 .github/                    # GitHub 配置（CI/CD、Issue模板）
├── 📁 .vscode/                    # VS Code 工作区配置
│
├── 📁 firmware/                   # 固件源码（C 语言）
│   ├── 📁 bootloader/             # 自定义 Bootloader（DFU升级）
│   │   ├── 📁 include/            #   Bootloader 头文件
│   │   ├── 📁 src/                #   Bootloader 源文件
│   │   └── CMakeLists.txt
│   ├── 📁 include/                # 头文件（分层：app/board/device/middleware/protocol）
│   ├── 📁 src/                    # 源文件（镜像 include 结构）
│   ├── 📁 lib/                    # SDK 版本锁定
│   ├── 📁 tests/                  # 单元测试
│   ├── 📁 scripts/                # 辅助脚本
│   ├── 📁 tools/                  # 工具脚本
│   ├── pico_flash_region.ld       # Flash 分区链接脚本覆盖
│   ├── app.ld                      # 备用链接脚本
│   ├── .clang-format               # 代码格式化配置
│   └── CMakeLists.txt
│
├── 📁 pc_tool/                    # 上位机（C# / .NET 10）
│   ├── HidConfigTool.slnx         # 解决方案
│   ├── 📁 shared/                  # 共享层（跨平台）
│   │   ├── 📁 HidConfigTool.Core/       # 核心业务层
│   │   ├── 📁 HidConfigTool.Hid/        # HID 驱动层
│   │   └── 📁 HidConfigTool.ViewModels/ # MVVM 视图模型
│   ├── 📁 windows/                 # Windows WPF 平台
│   │   └── 📁 HidConfigTool.App/        # WPF 应用
│   ├── 📁 desktop/                 # Avalonia 跨平台
│   │   └── 📁 HidConfigTool.Desktop/    # Avalonia 应用
│   └── 📁 tests/                   # 单元测试 + 集成测试
│
├── 📁 docs/                       # 项目文档
│   ├── DEVLOG.md                  # 开发日志（持续更新）
│   ├── AUDIT_REPORT.md            # 系统性审计报告
│   ├── HID_PROTOCOL.md            # HID 协议文档
│   ├── CONFIG_HID_PROTOCOL.md     # 配置协议文档
│   ├── DFU_UPGRADE.md             # DFU 升级文档
│   ├── FEATURES.md                # 功能详细说明
│   ├── SETUP.md                   # 开发环境配置
│   ├── QUICKSTART.md              # 快速上手指南
│   ├── ROADMAP.md                 # 开发路线图
│   ├── CODING-STANDARDS.md        # 编码规范
│   └── wiring_diagram.md          # 硬件接线图
│
├── 📁 tools/                      # 公共工具脚本
├── 📄 build.ps1                   # 一键构建脚本
├── 📄 .editorconfig               # 编辑器配置
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
- **picotool**（烧录固件用，可选，也可用 UF2 拖拽）

### 一键构建

```powershell
# 克隆仓库
git clone https://github.com/JYW-zero/pico_hid_composite.git
cd pico_hid_composite

# 全部编译（固件 + Bootloader + 上位机）
.\build.ps1 -Target All

# 或只编译固件
.\build.ps1 -Target Firmware

# 或只编译上位机
.\build.ps1 -Target PcTool
```

> 💡 **智能 SDK 管理**：build.ps1 会自动检测 Pico SDK，找不到时自动从官方拉取指定版本。

### 手动编译

**固件：**
```powershell
cd firmware
mkdir build; cd build
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
nmake
# 产物：build/pico_hid_composite.uf2
```

**Bootloader：**
```powershell
cd firmware/bootloader
mkdir build; cd build
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
nmake
# 产物：build/pico_hid_bootloader.uf2
```

**上位机：**
```powershell
cd pc_tool
dotnet build HidConfigTool.slnx --configuration Release
```

---

### 固件烧录（完整流程）

#### 第一步：进入 BOOTSEL 模式（bootrom UF2 模式）

1. 拔掉 USB
2. **按住 BOOTSEL 按钮**不松开
3. 插入 USB
4. 松开 BOOTSEL 按钮
5. 电脑出现 `RPI-RP2` U 盘

> 💡 这是 RP2350 bootrom 的硬件行为，与 Flash 内容无关，永远可用。

#### 第二步：擦除整个 Flash（全新烧录时）

```powershell
picotool erase
# 输出：Erased 2097152 bytes
```

#### 第三步：烧录 Bootloader

```powershell
picotool load firmware\bootloader\build\pico_hid_bootloader.uf2
```

#### 第四步：烧录应用程序

```powershell
picotool load firmware\build\pico_hid_composite.uf2
```

#### 第五步：复位启动

```powershell
picotool reboot
```

设备复位后：
1. Bootloader 先运行（检测应用程序有效性）
2. 自动跳转到应用程序
3. USB 枚举为 HID 复合设备（VID=0x1209, PID=0x4004）

#### 替代方案：UF2 拖拽

如果没有 picotool，可以用 UF2 拖拽：
1. 进入 BOOTSEL 模式，出现 `RPI-RP2` U 盘
2. 先拖入 `pico_hid_bootloader.uf2`（设备会自动复位）
3. 再次进入 BOOTSEL 模式
4. 拖入 `pico_hid_composite.uf2`
5. 自动复位，烧录完成

> ⚠️ UF2 只写入指定地址范围，不会自动擦除其他区域。全新烧录建议先用 picotool erase。

---

### 上位机运行

**WPF 版本（Windows）：**
```powershell
cd pc_tool\windows\HidConfigTool.App
dotnet run
```

**Avalonia 版本（跨平台）：**
```powershell
cd pc_tool\desktop\HidConfigTool.Desktop
dotnet run
```

---

## 📦 下载预编译版本

前往 [Releases](https://github.com/JYW-zero/pico_hid_composite/releases) 页面下载最新版本：

- `bootloader-rp2350.uf2` - Bootloader 固件
- `firmware-rp2350.uf2` - 应用程序固件
- `pc-tool-windows.zip` - Windows 上位机（自包含单文件）
- `pc-tool-crossplatform.zip` - 跨平台上位机（Avalonia）

---

## 🛠️ 开发环境

### 推荐开发方式

| 场景 | 推荐方式 |
|------|----------|
| **固件开发** | VS Code + Raspberry Pi Pico 扩展，打开 `firmware/` 目录 |
| **Bootloader 开发** | VS Code + Raspberry Pi Pico 扩展，打开 `firmware/bootloader/` 目录 |
| **上位机开发** | Visual Studio 2022 或 Rider，打开 `pc_tool/HidConfigTool.slnx` |
| **整体编译** | 根目录执行 `.\build.ps1 -Target All` |

### 详细文档

| 文档 | 说明 |
|------|------|
| [快速上手指南](docs/QUICKSTART.md) | 5 分钟快速上手 |
| [开发环境配置](docs/SETUP.md) | 完整的环境配置指南 |
| [功能清单](docs/FEATURES.md) | 详细功能说明 |
| [HID 协议文档](docs/HID_PROTOCOL.md) | USB HID 协议详细说明 |
| [配置协议文档](docs/CONFIG_HID_PROTOCOL.md) | 配置读写协议说明 |
| [DFU 升级文档](docs/DFU_UPGRADE.md) | DFU 固件升级流程 |
| [系统性审计报告](docs/AUDIT_REPORT.md) | 固件与上位机审计结果 |
| [开发日志](docs/DEVLOG.md) | 开发记录（持续更新） |
| [硬件接线图](docs/wiring_diagram.md) | 硬件接线说明 |
| [编码规范](docs/CODING-STANDARDS.md) | 代码风格规范 |
| [开发路线图](docs/ROADMAP.md) | 开发计划 |

---

## 📊 性能指标

| 指标 | 数值 |
|------|------|
| 主循环频率 | > 1000 Hz |
| 按键扫描延迟 | < 1 ms |
| 鼠标报告率 | 1000 Hz |
| ACK 响应超时 | 500 ms |
| 配置大小（v7） | 1492 字节 |
| Bootloader 大小 | ~20 KB（32KB 分区） |
| 应用程序大小 | ~150 KB（1948KB 分区） |
| RAM 占用 | ~ 40 KB |

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
- [HidSharp](https://www.zer7.com/software/hidsharp/)
- [Avalonia UI](https://avaloniaui.net/)

---

## 📧 联系作者

- **作者**：JYW
- **邮箱**：[J.YW@outlook.com](mailto:J.YW@outlook.com)

欢迎反馈问题、建议或交流技术！

---

<div align="center">

**如果这个项目对你有帮助，别忘了给个 ⭐ Star！**

</div>
