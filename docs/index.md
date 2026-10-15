# 指动 HID 复合设备文档

欢迎来到指动 HID 复合设备的官方文档网站！

## 项目简介

指动 HID 复合设备是一款基于 **Raspberry Pi RP2350** 的高度可配置 USB HID 复合设备，集成了键盘、鼠标、摇杆、编码器等多种输入功能。

### 核心特性

- ✅ **全键自定义** - 每个按键可映射为键盘按键、鼠标按键、媒体键或宏
- ✅ **多层按键映射** - 支持 4 层按键映射，通过 Fn 键切换
- ✅ **宏录制** - 支持按键序列、延迟、循环，存储到 Flash
- ✅ **多档 DPI** - 支持 4 档预设 DPI + 自定义 DPI
- ✅ **摇杆校准** - 支持死区调节、灵敏度曲线、方向反转
- ✅ **编码器** - 支持音量、滚动、缩放、亮度等多种模式
- ✅ **配置导入/导出** - 配置可保存为文件，便于备份和分享
- ✅ **固件升级** - 通过 USB DFU 模式在线升级

## 快速导航

### 🚀 快速开始

- [快速开始指南](QUICKSTART.md) - 从零开始设置你的设备
- [硬件连接](wiring_diagram.md) - 硬件接线图和连接说明
- [Windows 编译指南](SETUP.md) - Windows 环境下编译固件和上位机
- [macOS 编译指南](SETUP_MAC.md) - macOS 环境下编译固件和上位机

### 📖 用户手册

- [用户手册 (中文)](user_manual/用户手册_zh-CN.md) - 完整的中文用户手册
- [User Manual (English)](user_manual/User_Manual_en-US.md) - Complete English User Manual

### 🔧 开发者文档

- [项目架构](CODE_AUDIT.md) - 项目整体架构和代码审计
- [编码规范](CODING-STANDARDS.md) - 代码编写规范和最佳实践
- [功能列表](FEATURES.md) - 已实现和计划中的功能
- [开发路线图](ROADMAP.md) - 项目开发路线图
- [开发进度](PROGRESS.md) - 当前开发进度

### 📡 协议文档

- [HID 配置协议](CONFIG_HID_PROTOCOL.md) - 配置 HID 协议详细说明
- [HID 协议](HID_PROTOCOL.md) - HID 通信协议
- [DFU 升级协议](DFU_UPGRADE.md) - DFU 固件升级协议
- [版本检查](VERSION_CHECK.md) - 固件版本检查和兼容性管理

### 🧪 测试

- [HID 通信健壮性](HID_ROBUSTNESS.md) - HID 通信健壮性增强
- [看门狗与故障恢复](WATCHDOG_FAULT_RECOVERY.md) - 看门狗和故障恢复机制

### 🔄 CI/CD

- [CI/CD 流水线](CI-CD.md) - 持续集成和持续部署流水线说明
- [代码覆盖率](COVERAGE.md) - 代码覆盖率集成和报告

### 📜 认证

- [认证准备清单](certification/认证准备清单.md) - FCC/CE/RoHS 认证完整准备指南
- [USB 合规审计](USB_COMPLIANCE.md) - USB VID/PID 合规审计报告

### 📝 开发日志

- [开发日志](DEVLOG.md) - 项目开发历史记录

## 硬件规格

| 项目 | 规格 |
|-----|------|
| 主控芯片 | RP2350（双核 ARM Cortex-M33 @ 150MHz） |
| 内存 | 520KB SRAM |
| 存储 | 2MB Flash（可扩展至 4MB/8MB/16MB） |
| USB | USB 2.0 Full Speed，Type-C 接口 |
| 按键 | 最多 64 键（可配置） |
| 摇杆 | 双通道 12 位 ADC 模拟摇杆 |
| 编码器 | 旋转编码器（带按键） |
| 鼠标传感器 | 光学传感器，最高 26000 DPI |
| 工作电压 | 5V（USB 供电） |
| 工作电流 | < 500mA |

## 系统兼容性

| 系统 | 最低版本 | 状态 |
|-----|---------|------|
| Windows | Windows 10 1809 | ✅ 支持 |
| Windows 11 | 22H2+ | ✅ 支持 |
| macOS | 12 Monterey | ✅ 支持 |
| Linux | Ubuntu 20.04 | ✅ 支持 |
| Android | OTG 支持 | ⚠️ 部分功能 |
| iOS | - | ❌ 不支持 |

## 开源协议

本项目采用 **MIT License** 开源协议。

### 第三方库致谢

本项目使用了多个开源第三方库，所有第三方库均使用宽松开源许可证（MIT / BSD 3-Clause / Apache 2.0），允许商业使用。

详细信息请参阅 [第三方库许可证声明](../THIRD_PARTY_LICENSES.md)。

## 社区与支持

- **GitHub 仓库**：https://github.com/JYW-zero/pico_hid_composite
- **问题反馈**：https://github.com/JYW-zero/pico_hid_composite/issues
- **讨论区**：https://github.com/JYW-zero/pico_hid_composite/discussions

## 贡献指南

我们欢迎各种形式的贡献！无论是报告 Bug、提交功能请求、改进文档还是贡献代码，都非常感谢。

请参阅 [贡献指南](CONTRIBUTING.md) 了解详细的贡献流程。

---

**指动科技 / Zhidong Technology**

*让每一次输入都精准可控*
