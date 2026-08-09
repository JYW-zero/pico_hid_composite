# HID复合设备固件

基于 Raspberry Pi Pico 2 (RP2350) 的 USB HID 复合设备固件。

## 功能特性

- 64键SPI键盘矩阵（双手对称六向按键）
- PAW3395光学鼠标传感器（4档DPI）
- ADC摇杆（X/Y轴 + 按键）
- 旋转编码器（A/B相 + 中键）
- 宏功能（8个宏，每个32个动作）
- 工厂测试模式
- 可靠性设计（双备份配置、CRC校验、看门狗等）

## 文档

完整文档请查看项目根目录的 [docs/](../../docs/) 文件夹：

- [开发记录汇总](../../docs/DEVLOG.md) - 开发日志、问题记录、决策记录
- [固件功能说明](../../docs/FEATURES.md) - 详细功能清单
- [编码规范](../../docs/CODING-STANDARDS.md) - 代码规范
- [开发路线图](../../docs/ROADMAP.md) - 开发计划
- [开发环境配置指南](../../docs/SETUP.md) - 环境搭建
- [项目进度报告](../../docs/PROGRESS.md) - 进度报告

## 编译

```bash
cd build
ninja
```

## 烧录

使用 OpenOCD + CMSIS-DAP：

```bash
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "program dev_hid_composite.elf verify reset exit"
```
