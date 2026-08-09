# 开发环境配置指南

## 📋 概述
本文档记录了HID复合设备项目的开发环境配置要求，确保团队成员使用统一的工具版本，避免因版本不一致导致的各种问题。

---

## 🔧 工具版本要求

### 1. Pico SDK

| 项目 | 要求 |
|------|------|
| **版本** | 2.3.0 |
| **用途** | 固件开发 SDK |
| **验证方式** | 查看 `firmware/lib/.sdk_version` 文件 |

**说明**：
- 本项目锁定 SDK 版本为 2.3.0
- `build.ps1` 脚本会自动检测并拉取对应版本的 SDK
- 无需手动下载配置

---

### 2. Arm GNU Toolchain

| 项目 | 要求 |
|------|------|
| **版本** | 15_2_Rel1 |
| **用途** | 固件编译 |
| **验证命令** | `arm-none-eabi-gcc --version` |

**说明**：
- Pico VS Code 扩展会自动安装对应版本
- 通常位于 `~/.pico-sdk/toolchain/15_2_Rel1`

---

### 3. CMake（构建系统）

| 项目 | 要求 |
|------|------|
| **版本** | 4.3.4 |
| **用途** | 固件项目构建 |
| **验证命令** | `cmake --version` |

---

### 4. Ninja（构建工具）

| 项目 | 要求 |
|------|------|
| **版本** | 1.13.2 |
| **用途** | 加速构建 |
| **验证命令** | `ninja --version` |

---

### 5. OpenOCD（调试器）

| 项目 | 要求 |
|------|------|
| **版本** | 0.12.0+dev |
| **用途** | 固件烧录、调试、Flash操作 |
| **验证命令** | `openocd --version` |

**重要说明**：
- OpenOCD 0.12.0及以上版本才正式支持RP2350（Pico 2的主控芯片）
- 低于此版本可能会出现Flash大小识别错误（把4MB识别成2MB）
- Pico VS Code 扩展会自动安装对应版本

---

### 6. Picotool

| 项目 | 要求 |
|------|------|
| **版本** | 2.3.0 |
| **用途** | 固件烧录、设备信息查询 |
| **验证命令** | `picotool info` |

---

### 7. .NET SDK（上位机）

| 项目 | 要求 |
|------|------|
| **版本** | .NET 10 |
| **用途** | 上位机WPF应用开发 |
| **验证命令** | `dotnet --version` |

---

### 8. Python（可选）

| 项目 | 要求 |
|------|------|
| **推荐版本** | 3.10+ |
| **用途** | 脚本工具、测试 |
| **验证命令** | `python --version` |

---

## 📦 硬件要求

### 开发板
- **型号**：Raspberry Pi Pico 2
- **主控**：RP2350
- **Flash**：4MB（官方规格）
- **USB**：USB 1.1

### 调试器
- **推荐**：Raspberry Pi Debug Probe
- **备选**：其他支持SWD的调试器（J-Link、ST-Link等）

---

## 🚀 快速开始

### 方式一：一键构建脚本（推荐）

本项目提供 `build.ps1` 一键构建脚本，自动管理 SDK 和工具链：

```powershell
# 克隆仓库
git clone <repository-url>
cd pico_hid_composite

# 全部编译（固件 + 上位机）
.\build.ps1 -Target All

# 或只编译固件
.\build.ps1 -Target Firmware

# 或只编译上位机
.\build.ps1 -Target PcTool
```

**SDK 智能检测（四级优先级）**：
1. 手动指定路径（`-SdkPath` 参数）
2. 环境变量 `PICO_SDK_PATH`
3. Pico VS Code 扩展默认路径（`~/.pico-sdk/sdk/2.3.0`）
4. 项目本地 `firmware/lib/pico-sdk`（自动从 GitHub 浅克隆）

---

### 方式二：VS Code + Pico 扩展（固件开发）

本项目采用 **VS Code + Raspberry Pi Pico 官方插件** 进行开发，这是最简便的方式：

1. 在 VS Code 中安装扩展：**Raspberry Pi Pico** (`raspberry-pi.raspberry-pi-pico`)
2. 打开 `firmware/` 文件夹，插件会自动检测项目并引导配置 SDK
3. 点击底部状态栏的 **Build** 按钮即可编译

> 💡 如果你已经通过插件安装了 SDK，它通常位于 `~/.pico-sdk/sdk/2.3.0`。

---

### 方式三：手动编译（备选）

如果你不使用 VS Code 插件，也可以手动编译：

```bash
cd firmware
mkdir build && cd build
cmake .. -G Ninja -DPICO_SDK_PATH=/path/to/pico-sdk
ninja
```

---

### 上位机编译

```bash
cd pc_tool
dotnet build
```

或使用 VS / Rider 打开 `pc_tool/HidConfigTool.slnx`。

---

## 📂 开发工作流

### 日常开发

| 场景 | 推荐方式 |
|------|----------|
| **固件开发** | VS Code 打开 `firmware/` 目录 |
| **上位机开发** | VS / Rider 打开 `pc_tool/HidConfigTool.slnx` |
| **整体编译** | 根目录执行 `.\build.ps1 -Target All` |
| **固件烧录** | `picotool load build\pico_hid_composite.uf2 -f` |
| **上位机运行** | `dotnet run --project pc_tool/src/HidConfigTool.App` |

---

## ⚠️ 常见问题

### Q1: OpenOCD识别Flash为2MB而不是4MB
**原因**：OpenOCD版本过低，对RP2350支持不完善
**解决**：升级OpenOCD到0.12.0或更高版本

### Q2: 编译报错，找不到头文件
**原因**：ARM GCC工具链版本不对，或环境变量没配置好
**解决**：确认工具链版本，检查PATH环境变量，或使用 build.ps1 自动检测

### Q3: CMake版本过低
**原因**：系统自带的CMake版本太旧
**解决**：从CMake官网下载最新版，或使用包管理器更新

### Q4: Pico 扩展不识别项目
**原因**：工作区根目录没有 CMakeLists.txt
**解决**：单独打开 `firmware/` 目录开发，或使用 build.ps1 脚本

### Q5: build.ps1 执行策略报错
**原因**：PowerShell 执行策略限制
**解决**：以管理员身份执行 `Set-ExecutionPolicy RemoteSigned`

---

## 📝 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-08-09 | v1.1 | 更新工具版本，添加 build.ps1 说明，更新工作流 |
| 2026-08-08 | v1.0 | 初始版本，添加OpenOCD版本要求 |

---

## 📚 参考文档

- [Raspberry Pi Pico 2 官方文档](https://www.raspberrypi.com/documentation/microcontrollers/pico-2.html)
- [RP2350 数据手册](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [Pico SDK 官方文档](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html)
- [OpenOCD 官方文档](https://openocd.org/doc/html/index.html)
