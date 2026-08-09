# Changelog

所有重要的变更都记录在这个文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范。

## [Unreleased]

### Added
- 性能监控模块升级：
  - 任务超时告警（自动记录错误日志）
  - 任务CPU占比统计
  - 10秒/30秒滑动窗口平均
  - 超时次数统计
- 上位机性能监控页面优化：
  - 新增10秒/30秒平均CPU使用率显示
  - 新增10秒平均主循环频率显示
  - 任务表格新增CPU占比列
  - 任务表格新增超时次数列
- 🆕 Protocol 协议层 - USB HID 描述符与报文独立分层
- 🆕 SDK 智能管理 - build.ps1 自动检测/拉取 Pico SDK
- 🆕 .gitattributes - 强制 LF 换行符
- 🆕 根目录 tools/ - 公共工具脚本目录
- 🆕 完善 CI/CD 配置 - 双 Job 并行构建 + 缓存优化

### Changed
- 优化性能监控HID报告格式
- 重构项目目录结构（firmware / pc_tool / docs）
- 🏗️ 架构升级：从五层架构升级为六层架构（新增 Protocol 层）
- 📁 上位机解决方案上移至 pc_tool/ 根目录
- 🔧 build.ps1 升级为智能构建脚本
- 📝 README 全面升级

### Fixed
- 修复Flash写入大小不对齐问题
- 修复配置保存返回值判断错误

### Removed
- 删除根目录冗余 CMakeLists.txt 和 pico_sdk_import.cmake

---

## [0.1.0] - 2026-08-07

### Added
- 上位机设备操作按钮（重启设备、进入烧录模式）
- 固件软件重启命令（CMD_REBOOT）
- 固件进入 BOOTSEL 模式命令（CMD_ENTER_DFU）
- 统一 build 目录（只保留 firmware/build/）

### Fixed
- 修复上位机性能监控页面报错（Run.Text 绑定 TwoWay 问题）
- 修复 .gitignore 规则（添加 CMakeUserPresets.json、VS Code 配置等）

### Changed
- 优化项目目录结构（固件与上位机彻底分离）
- 优化 .gitignore（更完善的忽略规则）

### Removed
- 删除根目录 build/ 目录
- 删除 tools/ 下的 Python 脚本（pico_control.py、flash_swd.py、run_pc_tool.py）


### Added
- 初始版本发布
- 基础HID复合设备功能（键盘+鼠标+摇杆+编码器）
- 双备份配置存储
- 宏功能
- 工厂测试模式
- 性能监控基础功能
- Windows上位机配置工具
