# 架构重构实施计划

**版本**: 1.0
**日期**: 2026-08-24
**状态**: 待执行
**优先级**: High

---

## 概述

本文档描述 Pico HID Composite 项目的 6 个大规模架构重构任务的详细实施计划。这些任务涉及核心模块的拆分和整合，需要谨慎规划和逐步执行，每个重构阶段后必须运行完整测试确保无回归。

---

## 重构任务清单

| 编号 | 任务 | 预估工作量 | 风险等级 | 依赖 |
|------|------|-----------|---------|------|
| H1 | main.c 巨型文件拆分 | 3-5 天 | 中 | 无 |
| H2 | config_hid.c 巨型文件拆分 | 3-5 天 | 中 | 无 |
| C9 | DeviceService 巨型类拆分 | 5-7 天 | 高 | 无 |
| H16 | 双UI(WPF+Avalonia)整合 | 7-10 天 | 高 | C9 |
| H19 | ViewModel层瘦身 | 3-5 天 | 中 | C9 |
| H20 | 配置偏移量编译时绑定 | 2-3 天 | 低 | H2 |

---

## H1: main.c 巨型文件拆分

### 现状

- **文件**: `firmware/src/app/main.c`
- **规模**: ~1400 行，49KB
- **问题**: 单文件包含初始化、主循环、键盘处理、鼠标处理、编码器处理、USB 回调等多个职责，违反单一职责原则

### 目标架构

```
firmware/src/app/
├── main.c                  # 入口点，仅包含 main() 和初始化调度
├── system_init.c           # 系统初始化（时钟、GPIO、USB、外设）
├── keyboard_processor.c    # 键盘处理（按键扫描、NKRO、消费者键）
├── mouse_processor.c       # 鼠标处理（光学传感器、位移、按键）
├── encoder_processor.c     # 编码器处理（旋转、模式切换、待发送队列）
└── usb_callbacks.c         # TinyUSB 回调函数（tud_*_cb）
```

### 实施步骤

**阶段 1: 提取 USB 回调（1天）**
1. 创建 `usb_callbacks.c` 和 `usb_callbacks.h`
2. 将 `tud_mount_cb`、`tud_umount_cb`、`tud_hid_set_report_cb`、`tud_hid_get_report_cb` 等回调函数移到新文件
3. 在 `main.c` 中保留 `#include "app/usb_callbacks.h"`
4. 编译验证，确保链接正确
5. 运行固件测试

**阶段 2: 提取系统初始化（1天）**
1. 创建 `system_init.c` 和 `system_init.h`
2. 将 `board_init()`、时钟配置、GPIO 初始化、USB 初始化、外设初始化移到新文件
3. 导出 `system_init_all()` 函数
4. 在 `main.c` 中调用 `system_init_all()`
5. 编译验证

**阶段 3: 提取键盘处理（1天）**
1. 创建 `keyboard_processor.c` 和 `keyboard_processor.h`
2. 将按键扫描、NKRO 报告生成、消费者键报告、按键统计移到新文件
3. 导出 `keyboard_processor_init()`、`keyboard_processor_task()` 函数
4. 在主循环中调用 `keyboard_processor_task()`
5. 编译验证，测试键盘功能

**阶段 4: 提取鼠标和编码器处理（1-2天）**
1. 创建 `mouse_processor.c/h` 和 `encoder_processor.c/h`
2. 分别提取鼠标和编码器处理逻辑
3. 导出初始化和任务函数
4. 编译验证，测试鼠标和编码器功能

**阶段 5: 清理 main.c（0.5天）**
1. 移除已提取的函数和相关静态变量
2. 保留 `main()` 函数，仅包含初始化调度和主循环
3. 确保 `main.c` 行数控制在 200 行以内
4. 最终编译和测试验证

### 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| 静态变量跨文件访问 | 使用 getter/setter 函数或 shared_hw_data 模块 |
| 函数调用顺序依赖 | 保持原有的调用顺序，仅移动代码位置 |
| 中断处理函数位置 | TinyUSB 回调必须在特定链接段，确保编译选项正确 |

---

## H2: config_hid.c 巨型文件拆分

### 现状

- **文件**: `firmware/src/protocol/config_hid.c`
- **规模**: ~1300 行，64KB
- **问题**: 单文件包含 HID 报告接收、命令解析、配置读写、宏处理、工厂测试命令等多个职责

### 目标架构

```
firmware/src/protocol/
├── config_hid.c            # HID 报告入口（set_report/get_report 回调）
├── config_hid_receive.c    # 配置块接收和组装（分块传输、CRC校验）
├── config_hid_send.c       # 配置块发送和分片
├── config_hid_handlers.c   # 命令处理器（CMD_GET_CONFIG、CMD_SET_CONFIG 等）
└── config_hid_macro.c      # 宏相关命令处理
```

### 实施步骤

**阶段 1: 提取宏处理（1天）**
1. 创建 `config_hid_macro.c/h`
2. 将 `CMD_GET_MACRO`、`CMD_SET_MACRO`、`CMD_PLAY_MACRO`、`CMD_STOP_MACRO` 相关处理移到新文件
3. 导出宏命令处理函数
4. 编译验证

**阶段 2: 提取配置接收（1天）**
1. 创建 `config_hid_receive.c/h`
2. 将分块接收逻辑、缓冲区管理、CRC 校验移到新文件
3. 导出 `config_hid_receive_block()`、`config_hid_is_complete()` 函数
4. 编译验证

**阶段 3: 提取配置发送（1天）**
1. 创建 `config_hid_send.c/h`
2. 将配置分片发送逻辑移到新文件
3. 导出 `config_hid_send_config()` 函数
4. 编译验证

**阶段 4: 提取命令处理器（1天）**
1. 创建 `config_hid_handlers.c/h`
2. 将各个命令的处理逻辑移到新文件
3. 使用命令分发表（command dispatch table）替代 switch-case
4. 编译验证

**阶段 5: 清理 config_hid.c（0.5天）**
1. 保留 HID 报告入口函数
2. 移除已提取的函数和静态变量
3. 确保 `config_hid.c` 行数控制在 300 行以内
4. 最终编译和测试验证

### 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| 静态缓冲区跨文件访问 | 使用 getter 函数或将缓冲区移到 receive 模块 |
| 命令处理状态机 | 保持原有的状态变量，通过函数参数传递 |
| 命令分发表引入新 bug | 先保持 switch-case，拆分完成后再重构为分发表 |

---

## C9: DeviceService 巨型类拆分

### 现状

- **文件**: `pc_tool/shared/HidConfigTool.Core/Services/DeviceService.cs`
- **规模**: ~3500 行，133KB
- **问题**: 单类包含设备连接、配置读写、DFU升级、错误日志、性能监控、宏管理、按键统计等多个职责，违反单一职责原则

### 目标架构

```
pc_tool/shared/HidConfigTool.Core/Services/
├── DeviceService.cs              # 设备服务门面（聚合子服务，保持向后兼容）
├── DeviceConnectionService.cs    # 设备连接管理（查找、连接、断开、重连）
├── ConfigService.cs              # 配置读写（读取、保存、导入导出、迁移）
├── DfuService.cs                 # DFU 固件升级（bootloader 切换、固件传输、校验）
├── DiagnosticsService.cs         # 诊断服务（错误日志、性能监控、按键统计）
├── MacroService.cs               # 宏管理（读取、设置、播放、停止）
└── CapabilityService.cs          # 能力检测（版本信息、能力位图、兼容性检查）
```

### 实施步骤

**阶段 1: 提取设备连接服务（1-2天）**
1. 创建 `DeviceConnectionService.cs`
2. 将 `GetDevicesAsync()`、`ConnectAsync()`、`Disconnect()`、`ReconnectDeviceAsync()`、自动重连逻辑移到新类
3. 在 `DeviceService` 中保留委托属性或方法，保持向后兼容
4. 添加单元测试
5. 编译验证

**阶段 2: 提取配置服务（1-2天）**
1. 创建 `ConfigService.cs`
2. 将 `ReadConfigFromDeviceAsync()`、`SaveConfigAsync()`、`ResetConfigAsync()`、`ExportConfigAsync()`、`ImportConfigAsync()` 移到新类
3. 在 `DeviceService` 中保留委托
4. 添加单元测试
5. 编译验证

**阶段 3: 提取 DFU 服务（1天）**
1. 创建 `DfuService.cs`
2. 将 `DfuUpgradeAsync()`、`GetBootloaderInfoAsync()`、bootloader 切换逻辑移到新类
3. 在 `DeviceService` 中保留委托
4. 添加单元测试
5. 编译验证

**阶段 4: 提取诊断服务（1天）**
1. 创建 `DiagnosticsService.cs`
2. 将错误日志、性能监控、按键统计相关方法移到新类
3. 在 `DeviceService` 中保留委托
4. 添加单元测试
5. 编译验证

**阶段 5: 提取宏和能力服务（1天）**
1. 创建 `MacroService.cs` 和 `CapabilityService.cs`
2. 分别提取宏管理和能力检测逻辑
3. 在 `DeviceService` 中保留委托
4. 添加单元测试
5. 编译验证

**阶段 6: 瘦身 DeviceService（0.5-1天）**
1. `DeviceService` 变为门面类，聚合所有子服务
2. 保留公共 API，内部委托给子服务
3. 标记旧 API 为 `[Obsolete]`，引导使用子服务
4. 确保 `DeviceService.cs` 行数控制在 500 行以内
5. 最终编译和测试验证（156+ 测试全部通过）

### 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| 公共 API 变更影响 ViewModel | 保持 `DeviceService` 门面类，内部委托，不破坏现有调用 |
| 共享状态（_currentConfig、_currentDevice） | 使用 `ConcurrentDictionary` 或 `lock` 保护，子服务通过接口访问 |
| 事件（DeviceConnectionChanged） | 在门面类中保留事件，子服务触发内部事件，门面类转发 |
| 测试覆盖 | 每个子服务独立添加单元测试，总测试数目标 200+ |

---

## H16: 双UI(WPF+Avalonia)整合

### 现状

- **WPF 项目**: `pc_tool/windows/HidConfigTool.App/`
- **Avalonia 项目**: `pc_tool/avalonia/HidConfigTool.Avalonia/`（推测）
- **问题**: 两套 UI 代码重复，XAML、ViewModel、样式都需要维护两份

### 目标架构

```
pc_tool/
├── shared/
│   ├── HidConfigTool.Core/          # 核心业务逻辑（已有）
│   ├── HidConfigTool.ViewModels/    # ViewModel 层（已有）
│   └── HidConfigTool.UI.Shared/     # 新增：共享 UI 组件
│       ├── Controls/                 # 自定义控件（跨平台抽象）
│       ├── Converters/               # 值转换器
│       ├── Styles/                   # 共享样式（颜色、字体、间距）
│       └── Themes/                   # 主题定义（浅色/深色）
├── windows/
│   └── HidConfigTool.App/            # WPF 平台特定代码
│       ├── PlatformControls/         # WPF 特定控件实现
│       └── PlatformServices/         # WPF 特定服务（对话框、文件选择）
└── avalonia/
    └── HidConfigTool.Avalonia/       # Avalonia 平台特定代码
        ├── PlatformControls/         # Avalonia 特定控件实现
        └── PlatformServices/         # Avalonia 特定服务
```

### 实施步骤

**阶段 1: 提取共享样式（2天）**
1. 创建 `HidConfigTool.UI.Shared` 项目
2. 提取颜色、字体、间距等静态资源为 C# 类
3. 在 WPF 和 Avalonia 项目中分别引用并映射为平台资源
4. 验证两个平台外观一致

**阶段 2: 提取值转换器（1天）**
1. 将通用值转换器移到共享项目
2. 使用 `IValueConverter` 接口（WPF 和 Avalonia 都支持）
3. 验证转换器在两个平台都能正常工作

**阶段 3: 抽象平台服务（2天）**
1. 在 Core 项目定义 `IDialogService`、`IFileDialogService`、`INavigationService` 接口
2. 在 WPF 和 Avalonia 项目分别实现
3. 通过依赖注入注册平台实现
4. ViewModel 只依赖接口，不依赖平台实现

**阶段 4: 共享页面逻辑（2-3天）**
1. 将页面逻辑（数据加载、命令绑定）保留在 ViewModel 中
2. XAML 视图分别在两个平台实现，但绑定相同的 ViewModel
3. 创建页面基类，封装通用逻辑
4. 验证两个平台功能一致

**阶段 5: 自定义控件抽象（1天）**
1. 对于需要自定义绘制的控件，定义 `ICustomControl` 接口
2. 在两个平台分别实现渲染逻辑
3. 共享控件逻辑（状态管理、输入处理）

### 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| WPF 和 Avalonia API 差异 | 使用接口抽象，平台特定代码隔离 |
| XAML 语法差异 | 不共享 XAML，只共享 ViewModel 和样式资源 |
| 平台特定功能（如 Windows 注册表） | 在平台项目中实现，通过接口暴露 |

---

## H19: ViewModel层瘦身

### 现状

- **问题**: ViewModel 中包含过多业务逻辑，应提取到 Service 层
- **典型问题**: `MainViewModel`、`DevicePageViewModel` 等包含设备连接、配置读写等业务逻辑

### 目标

- ViewModel 只负责：UI 状态管理、命令绑定、数据展示
- Service 负责：业务逻辑、数据处理、平台交互

### 实施步骤

**阶段 1: 识别业务逻辑（0.5天）**
1. 审查所有 ViewModel，标记包含业务逻辑的方法
2. 分类：设备操作、配置操作、文件操作、导航操作

**阶段 2: 提取到 Service（2-3天）**
1. 对于 C9 中已拆分的子服务，直接使用
2. 对于尚未提取的业务逻辑，创建对应的 Service
3. ViewModel 通过构造函数注入 Service
4. 逐步替换 ViewModel 中的业务逻辑调用

**阶段 3: 验证和测试（1天）**
1. 确保所有 ViewModel 功能正常
2. 为新增的 Service 添加单元测试
3. 运行完整测试套件

### 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| ViewModel 依赖 Service 循环引用 | 使用接口解耦，Service 不依赖 ViewModel |
| 异步命令处理 | 使用 `AsyncRelayCommand`，在 Service 中实现异步逻辑 |

---

## H20: 配置偏移量编译时绑定

### 现状

- **问题**: 上位机解析配置时使用硬编码的偏移量和大小，与固件 `device_config_t` 结构体不同步
- **风险**: 固件修改结构体后，上位机解析错误

### 目标

- 使用 Source Generator 在编译时从固件头文件生成配置偏移量常量
- 或者使用 T4 模板生成
- 确保上位机和固件配置结构一致

### 实施步骤

**阶段 1: 定义配置结构描述（0.5天）**
1. 创建 `device_config.schema.json`，描述配置结构体的字段、类型、偏移量
2. 固件和上位机都使用这个 schema 作为单一数据源

**阶段 2: 创建 Source Generator（1天）**
1. 创建 `HidConfigTool.SourceGenerators` 项目
2. 实现 `ConfigOffsetGenerator`，读取 schema JSON，生成 `ConfigOffsets.cs`
3. 生成的类包含每个字段的偏移量、大小、类型常量

**阶段 3: 集成到上位机（0.5天）**
1. 在 Core 项目中引用 Source Generator
2. 替换硬编码的偏移量为生成的常量
3. 编译验证

**阶段 4: 固件集成（可选，0.5天）**
1. 在固件构建脚本中添加 schema 验证步骤
2. 确保 `device_config_t` 与 schema 一致
3. 可使用 Python 脚本自动验证

### 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| Source Generator 调试困难 | 先使用 T4 模板或手动生成，验证逻辑正确后再迁移到 Source Generator |
| 固件和上位机 schema 不同步 | CI 中添加验证步骤，构建时检查一致性 |

---

## 执行顺序建议

### 第一阶段（1-2周）：低风险重构
1. **H20** 配置偏移量编译时绑定（低风险，独立）
2. **H1** main.c 拆分（中风险，固件核心）
3. **H2** config_hid.c 拆分（中风险，固件核心）

### 第二阶段（2-3周）：上位机重构
4. **C9** DeviceService 拆分（高风险，上位机核心）
5. **H19** ViewModel 层瘦身（依赖 C9）

### 第三阶段（1-2周）：跨平台整合
6. **H16** 双UI整合（高风险，依赖 C9 和 H19）

---

## 验证标准

每个重构任务完成后必须满足：
1. ✅ 固件编译通过（0 错误，0 警告）
2. ✅ 上位机编译通过（0 错误，0 警告）
3. ✅ 固件单元测试全部通过（124+ 测试）
4. ✅ 上位机单元测试全部通过（156+ 测试，目标 200+）
5. ✅ 代码审查通过（无新增 Critical/High 问题）
6. ✅ 功能测试通过（设备连接、配置读写、DFU 升级等核心功能）

---

## 回滚策略

每个重构任务使用独立的 Git 分支：
- 分支命名：`refactor/<task-id>-<short-description>`
- 例如：`refactor/h1-main-c-split`
- 重构完成后提交 PR，经过代码审查和测试验证后合并
- 如果发现严重问题，可以直接 revert 合并提交

---

*本文档随重构进展持续更新。*
