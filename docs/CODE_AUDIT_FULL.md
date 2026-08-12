# 全面代码审计报告

> 审计日期：2026-08-13  
> 项目：pico_hid_composite（RP2350 固件 + Windows 上位机）  
> 审计范围：全部固件源码（21 .c + 23 .h）+ 上位机全部 C#/XAML 文件

---

## 一、总体评价

| 维度 | 固件 | 上位机 |
|------|------|--------|
| 架构设计 | 优秀（六层分层，零状态驱动） | 良好（MVVM + DI） |
| 功能完成度 | **约 75%** | **约 70%** |
| 代码质量 | 中上（有若干严重 Bug） | 中（有死代码和未连接模块） |
| 测试覆盖 | 极低（无测试源码） | 极低（<5%，仅模型验证） |
| 文档一致性 | 中（文档声称 vs 实际有偏差） | N/A |

---

## 二、固件审计（C / RP2350）

### 2.1 Board 层

| 文件 | 状态 | 说明 |
|------|------|------|
| `board/pins.h` | ✅ 完成 | 引脚定义、时序常量齐全 |
| `board/board.c` | ✅ 完成 | SPI/ADC/GPIO 初始化正确 |
| `board/flash_layout.c` | ✅ 完成 | 运行时 Flash 尺寸检测，动态偏移计算 |
| `board/config.c` | ⚠️ 功能正常，有隐患 | 双备份 + CRC32 + 版本迁移 |

**问题：**
- 🔴 `config.c:318` — `uint8_t write_buf[1536]` 栈上 1.5KB 缓冲区，在深层调用链中有栈溢出风险，应改为 static 或堆分配
- 🟡 `config.c:480` — `config_reset_default()` 中间态修改了 `s_current_config`，存在副作用
- 🟡 `config.c:498` — `config_get_default()` 临时修改全局状态再恢复，非线程安全

### 2.2 Device 层

| 文件 | 状态 | 说明 |
|------|------|------|
| `device/keypad_spi.c` | ✅ 完成 | SPI 超时保护、空指针检查、可重入 |
| `device/paw3395.c` | ✅ 完成 | 寄存器读写、硬件复位、12bit 位移解析 |
| `device/joystick.c` | ✅ 完成 | ADC 通道映射、按键 GPIO |
| `device/encoder.c` | ✅ 完成 | 4 态状态机解码、累加器抗抖动 |

**问题：**
- 🟡 `paw3395.c` — 初始化读取 PID 但未验证是否为 0x42（PAW3395 预期值），接错芯片不会报错
- 🟢 `joystick.c` — `adc_init()` 被 board 和 joystick 各调一次，冗余但无害
- 🟢 `joystick.c` — 无校准程序，中心值硬编码为 2048

### 2.3 Middleware 层

| 文件 | 状态 | 说明 |
|------|------|------|
| `middleware/debounce.c` | ✅ 完成 | 计数器消抖，64 键独立 |
| `middleware/watchdog.c` | ✅ 完成 | 三层看门狗 + 硬件后备 |
| `middleware/scheduler.c` | ✅ 完成 | 协作式时间片调度 |
| `middleware/fault.c` | ⚠️ 功能有缺陷 | Flash 持久化错误日志 |
| `middleware/power_manager.c` | ✅ 完成 | 休眠/深度休眠双模式 |
| `middleware/perf_monitor.c` | ✅ 完成 | 任务级性能监控 |
| `middleware/shared_hw_data.c` | 🔴 **接口缺失** | 头文件声明了 11 个函数但 .c 未实现 |
| `middleware/ipc.h` | ✅ 完成 | 核间通信命令编码 |

**问题：**
- 🔴 `shared_hw_data.c` — 头文件声明 11 个统计函数（`shared_hw_inc_keypad_scan` 等）但 .c 文件中**未实现**，启用 Core1 时会**链接失败**
- 🔴 `fault.c:157` — FATAL 级别错误**不触发复位**（注释 TODO，代码被注释掉），违反设计规格
- 🔴 `fault.c:135` — 环形缓冲区回绕逻辑在擦除扇区后，旧条目被清除但 `s_log_count` 未重置，读取 API 会返回错误数据。**数据完整性 Bug**
- 🟡 `scheduler.c` — 将 `const sched_task_t*` 强转后修改 `last_run_us`，违反 const 正确性
- 🟡 `power_manager.c` — 使用 `extern bool tud_mounted(void)` 但未 `#include "tusb.h"`，隐式声明

### 2.4 App 层

| 文件 | 状态 | 说明 |
|------|------|------|
| `app/main.c` | ⚠️ **问题最多的文件** | 主循环、HID 报告、配置协议 |
| `app/macro.c` | ✅ 基本完成 | 8 宏 × 32 动作，状态机 |
| `app/keymap.c` | ✅ 完成 | 双层映射、消费者键码转换 |
| `app/core1_scanner.c` | ❌ **死代码** | 完整实现但从未启动 |
| `app/factory_test.c` | ⚠️ 大部分完成 | LED 测试为桩函数 |
| `app/key_stats.c` | 🔴 **严重 Bug** | Flash 持久化按键统计 |

**问题：**
- 🔴 **`key_stats.h` — 结构体大小 (268B) 超过记录槽大小 (256B)**，写入 Flash 会**破坏相邻记录**，这是最严重的 Bug
- 🔴 `main.c` — **Core1 从未启动**：`multicore_launch_core1()` 从未被调用，双核架构形同虚设
- 🔴 `main.c:497` — `send_hid_report(MOUSE)` 发送**硬编码 delta=5 鼠标移动**，这是 TinyUSB 示例的残留测试代码，导致每 10ms 产生虚假鼠标移动
- 🔴 `main.c:1598` — `keypad_task()` 和 `send_hid_report()` **重复发送键盘报告**，每次按键产生双份 HID 报告
- 🔴 `main.c:614` — `g_config_pending` 标志**从未被设置为 true**，相关代码路径为死代码
- 🟡 `main.c:100-106` — 函数声明重复（第一处无 `static`，第二处有 `static`），前者为死代码
- 🟡 `core1_scanner.c` — `MOUSE_ACCEL_ENABLE` 宏从未定义，鼠标加速永远禁用
- 🟡 `factory_test.c` — 使用 `sprintf` 无边界检查，应改 `snprintf`
- 🟡 `factory_test.c:431` — `factory_test_check_entry()` 永远返回 false，入口检测未实现

### 2.5 Protocol 层

| 文件 | 状态 | 说明 |
|------|------|------|
| `protocol/usb_descriptors.c` | ⚠️ 有缺陷 | 18 个 Report ID 定义 |
| `protocol/tusb_config.h` | ✅ 完成 | 2 个 HID 接口，64 字节端点 |

**问题：**
- 🔴 `usb_descriptors.c` — **Key Stats Report IDs 10-13 在 HID 配置描述符中缺失**，主机无法识别按键统计报告
- 🟡 `usb_descriptors.h:54` — `CONFIG_TOTAL_SIZE 146` 仅覆盖非宏部分，命名容易误导

### 2.6 构建系统

- 🟡 `CMakeLists.txt` — **缺少 `pico_multicore` 库链接**，启用 Core1 代码时会链接失败

---

## 三、上位机审计（C# / WPF / .NET 10）

### 3.1 HidConfigTool.Core（核心模型层）

| 文件 | 状态 | 说明 |
|------|------|------|
| `Models/DeviceInfo.cs` | ✅ 完成 | VID/PID/固件版本 |
| `Models/DeviceConfig.cs` | ✅ 完成 | DPI、按键映射、宏数据 |
| `Models/KeyDefinition.cs` | ✅ 完成 | ~80 个 HID 键码定义 |
| `Models/Macro*.cs` | ✅ 完成 | 6 种动作类型、双向绑定 |
| `Models/ErrorLogEntry.cs` | ✅ 完成 | 日志条目 + 计算属性 |
| `Models/PerfMonitor.cs` | ✅ 完成 | 系统/任务级性能数据 |
| `Interfaces/IHidDriver.cs` | ✅ 完成 | 异步 HID 驱动接口 |
| `Interfaces/IDeviceService.cs` | ✅ 完成 | 完整的设备服务接口 |
| `Interfaces/ICloudSyncService.cs` | ✅ 完成 | 云同步接口（预留） |
| `Interfaces/IUpdateService.cs` | ✅ 完成 | 更新服务接口 |

**问题：**
- 🟢 `Class1.cs` — 空占位文件，应删除
- 🟡 `DeviceConfig.cs` — 数组大小无校验，`Keymap`/`FnKeymap` 可被设为错误长度

### 3.2 HidConfigTool.Drivers（WinRT 驱动层）

| 文件 | 状态 | 说明 |
|------|------|------|
| `HidDriver.cs` | ❌ **死代码** | WinRT 实现，但 App 未引用此项目 |
| `Class1.cs` | ❌ 空占位 | 应删除 |

**问题：**
- 🔴 **整个项目未被引用** — App 使用自带的 P/Invoke 版本，此项目完全是死代码
- 🔴 `FindDevicesAsync:50` — `UsagePage`/`UsageId` 始终为 0，若被使用会导致设备过滤失败

### 3.3 HidConfigTool.App — 驱动层

| 文件 | 状态 | 说明 |
|------|------|------|
| `Drivers/HidDriver.cs` | ✅ 完成 | P/Invoke 实现（hid.dll + setupapi） |

**问题：**
- 🟡 `GetFeatureReportAsync` — 硬编码 65 字节缓冲区，大于此长度的报告会被截断
- 🟡 无超时机制 — `HidD_GetFeature`/`HidD_SetFeature` 可能永久阻塞
- 🟢 `FindDevicesAsync` 中 `Log()` 方法与同名 lambda 存在遮蔽

### 3.4 HidConfigTool.App — 服务层

| 文件 | 状态 | 说明 |
|------|------|------|
| `Services/DeviceService.cs` | ⚠️ 核心文件，1673 行 | 二进制协议、重连、心跳 |
| `Services/TrayIconManager.cs` | ✅ 完成 | 系统托盘图标 |
| `Services/AutoStartManager.cs` | ✅ 完成 | 注册表开机自启 |
| `Services/ConfigProfileManager.cs` | ✅ 完成 | JSON 配置存档 |
| `Services/OsdManager.cs` | ✅ 完成 | OSD 窗口管理 |
| `Services/AppAwarenessManager.cs` | ⚠️ 功能不完整 | 前台窗口检测 |
| `Services/KeyboardHook.cs` | ✅ 完成 | 低级键盘钩子 |
| `Services/MacroRecorder.cs` | ⚠️ 有 Bug | 宏录制 |
| `Services/LocalCloudSyncService.cs` | ✅ 完成 | 文件模拟云同步（占位） |
| `Services/UpdateService.cs` | ⚠️ 桩实现 | HTTP 更新检查 |

**问题：**
- 🔴 `MacroRecorder` — 录制的是 **VK 码**，固件期望 **HID Usage 码**，所有录制的宏键码都是错的
- 🔴 `DeviceService.cs` — 可能存在语法损坏（`ReadErrorLogEntryAsync` 附近代码似乎与 `SerializeMacroData` 混合），需验证编译
- 🟡 `DeviceService.cs:1316` — `SetAccelerationAsync` 只改本地配置不写设备，UI 无提示
- 🟡 `AppAwarenessManager:220` — `SwitchProfile()` 显示 OSD 但**不实际加载/应用配置**
- 🟡 `UpdateService:16` — 更新 URL 为 `https://example.com/updates/version.json`（占位符）
- 🟡 `UpdateService:152` — `InstallUpdateAsync()` 为桩实现
- 🟡 `UpdateService:138` — SHA256 校验为 TODO
- 🟡 `UpdateService` — `HttpClient` 构造后从未 Dispose
- 🟡 `TrayIconManager:136` — `Icon.FromHandle()` 后未调用 `DestroyIcon`，GDI 句柄泄漏

### 3.5 HidConfigTool.App — ViewModel 层

| ViewModel | 状态 | 说明 |
|-----------|------|------|
| `MainViewModel` | ✅ 完成 | 9 页导航 + 设备状态桥接 |
| `DevicePageViewModel` | ✅ 完成 | 自动刷新、连接/断开/重启 |
| `KeyPageViewModel` | ⚠️ 有桩函数 | 8×8 键盘布局 + 选键 |
| `MousePageViewModel` | ✅ 完成 | DPI 切换 + 加速设置 |
| `JoystickPageViewModel` | ⚠️ 幻影属性 | 灵敏度/反转无后端支持 |
| `EncoderPageViewModel` | ⚠️ 幻影属性 | 步长/滚速无后端支持 |
| `MacroPageViewModel` | ⚠️ 有 Bug | 宏编辑 + 录制 |
| `ErrorLogPageViewModel` | ✅ 完成 | 日志加载/过滤/清除 |
| `PerfMonitorPageViewModel` | ✅ 完成 | 实时 CPU 图表 |
| `SettingsPageViewModel` | ⚠️ 多处桩函数 | 托盘/自启/配置/更新 |
| `StatsPageViewModel` | ⚠️ 功能受限 | 时间范围选择器不可用 |
| `FingerViewModel` | ✅ 完成 | 手指布局数据 |

**问题：**
- 🔴 `MacroPageViewModel:239` — `AddKeyAction` 硬编码 KeyCode=65（ASCII 'A'），应为 HID Usage 0x04。**录制/手动添加的宏键码全部错误**
- 🟡 `MacroPageViewModel:391` — `PlayMacro()`/`StopPlayback()` 为空桩函数
- 🟡 `KeyPageViewModel:286` — `ResetDefault()` 显示"功能开发中..."
- 🟡 `SettingsPageViewModel:267` — `CheckUpdateAsync()` 为桩（延迟 1s 返回"已是最新版本"）
- 🟡 `SettingsPageViewModel:292` — `StartUpdateAsync()` 为桩（Task.Delay 模拟进度）
- 🟡 `SettingsPageViewModel:430` — 使用 `Microsoft.VisualBasic.Interaction.InputBox` 但 .csproj 中**未引用该程序集**
- 🟡 `SettingsPageViewModel:460` — `OpenLogFolder()` 为桩函数
- 🟡 `JoystickPageViewModel` — `Sensitivity`、`InvertX`、`InvertY` 属性无 DeviceConfig 对应字段
- 🟡 `EncoderPageViewModel` — `StepsPerTick`、`ScrollSpeed` 无保存逻辑
- 🟡 `StatsPageViewModel` — 构造函数中始终启动定时器，不管页面是否可见
- 🟡 HID 键码→名称映射存在**三份拷贝**（`KeyDefinitions`、`KeyPageViewModel.GetKeyName()`、`MacroPageViewModel.GetKeyName()`）

### 3.6 HidConfigTool.App — View 层

| View | 状态 | 说明 |
|------|------|------|
| `MainWindow.xaml` | ✅ 完成 | 深色主题、自定义标题栏、SVG 图标 |
| `DevicePage.xaml` | ✅ 完成 | |
| `KeyPage.xaml` | ✅ 完成 | |
| `MousePage.xaml` | ✅ 完成 | |
| `JoystickPage.xaml` | ✅ 完成 | |
| `EncoderPage.xaml` | ✅ 完成 | |
| `MacroPage.xaml` | ✅ 完成 | |
| `ErrorLogPage.xaml` | ✅ 完成 | |
| `PerfMonitorPage.xaml` | ✅ 完成 | |
| `StatsPage.xaml` | ⚠️ 已注册但**导航不可达** |
| `SettingsPage.xaml` | ✅ 完成 | |

**问题：**
- 🟡 `MainWindow.xaml.cs` — `HwndSource` 钩子添加后从未移除，资源泄漏
- 🟡 页面以 Transient 方式创建，旧页面从未 Dispose，ViewModel 事件订阅累积

### 3.7 测试覆盖

| 测试文件 | 测试数 | 覆盖范围 |
|----------|--------|----------|
| `UnitTest1.cs` | 1 | 空占位 |
| `MacroTests.cs` | 3 | 宏默认值、动作创建 |
| `DeviceConfigTests.cs` | 3 | 默认值、JSON 序列化 |
| `KeyDefinitionTests.cs` | 4 | 键码定义完整性 |
| **总计** | **11** | **<5% 代码覆盖** |

**严重缺失：**
- ❌ DeviceService（1673 行核心业务逻辑）零测试
- ❌ 二进制协议序列化/反序列化零测试
- ❌ 所有 ViewModel 零测试
- ❌ 所有 Service 零测试
- ❌ 所有 Converter 零测试
- ❌ Tests 项目未引用 App 项目，只能测试 Core 模型

---

## 四、汇总矩阵

### 🔴 严重 Bug（必须修复）

| # | 位置 | 问题 | 影响 |
|---|------|------|------|
| F1 | `key_stats.h` | 结构体 268B > 记录槽 256B | Flash 数据损坏 |
| F2 | `shared_hw_data.c` | 11 个函数声明但未实现 | 链接失败 |
| F3 | `usb_descriptors.c` | Report ID 10-13 描述符缺失 | 主机无法读取按键统计 |
| F4 | `main.c` | Core1 从未启动 | 双核架构失效 |
| F5 | `main.c:497` | 鼠标报告发送硬编码 delta=5 | 虚假鼠标移动 |
| F6 | `main.c:1598` | 键盘报告重复发送 | 每次按键双份报告 |
| F7 | `fault.c:157` | FATAL 不触发复位 | 致命错误被忽略 |
| F8 | `fault.c:135` | 环形缓冲区回绕数据损坏 | 错误日志读取异常 |
| P1 | `MacroRecorder` | 录制 VK 码而非 HID Usage 码 | 宏键码全部错误 |
| P2 | `MacroPageViewModel:239` | KeyCode=65（ASCII）应为 0x04（HID） | 手动添加宏键码错误 |
| P3 | `HidConfigTool.Drivers` | 整个项目未被引用 | 死代码，增加维护负担 |

### 🟡 高优先级（应尽快处理）

| # | 位置 | 问题 |
|---|------|------|
| F9 | `main.c:614` | `g_config_pending` 永远为 false，代码路径为死代码 |
| F10 | `config.c:318` | 1.5KB 栈缓冲区有溢出风险 |
| F11 | `CMakeLists.txt` | 缺少 `pico_multicore` 库链接 |
| F12 | `scheduler.c` | const 正确性违反 |
| F13 | `core1_scanner.c` | `MOUSE_ACCEL_ENABLE` 宏未定义 |
| P4 | `SettingsPageViewModel:430` | `Microsoft.VisualBasic.InputBox` 未引用程序集 |
| P5 | `AppAwarenessManager:220` | 配置切换不实际应用 |
| P6 | `SettingsPageViewModel` | 配置管理与 `ConfigProfileManager` 未连接 |
| P7 | `DeviceService` | 线程安全缺口（多线程共享状态） |
| P8 | 三处键码映射 | 同一映射三份拷贝，维护风险高 |
| P9 | `StatsPage` | DI 注册但导航不可达 |

### 🟢 低优先级（后续优化）

| # | 位置 | 问题 |
|---|------|------|
| F14 | `paw3395.c` | PID 未验证 |
| F15 | `factory_test.c` | sprintf 无边界检查 |
| F16 | `power_manager.c` | 隐式函数声明 |
| P10 | `Class1.cs` | 空占位文件 |
| P11 | `UpdateService` | HttpClient 未 Dispose |
| P12 | `TrayIconManager` | GDI 句柄泄漏 |
| P13 | `MainWindow` | HwndSource 钩子未移除 |
| P14 | `JoystickPageViewModel` | 幻影属性无后端 |
| P15 | `EncoderPageViewModel` | 幻影属性无后端 |
| P16 | 各处桩函数 | 更新检查/安装、宏播放、重置默认等 |

---

## 五、文档 vs 实际 偏差

| 文档声称 | 实际情况 |
|----------|----------|
| ROADMAP: "Core1 异常自动恢复" | ❌ Core1 从未启动，无恢复代码 |
| ROADMAP: "频繁重启保护" | ❌ 任何位置均未实现 |
| FEATURES: "鼠标指针加速" | ❌ 编译宏未定义，功能永久禁用 |
| PROGRESS: "固件约 90% 完成" | ⚠️ 双核、鼠标加速、重启保护均未工作，实际约 75% |
| CODE_AUDIT: "宏配置持久化未完成" | ⚠️ 已通过 main.c 脏标志机制实现，文档过时 |
| FEATURES: "54 个单元测试全部通过" | ❌ 固件 tests/ 目录无测试源码文件 |

---

## 六、开发优先级建议

### 第一阶段：修复严重 Bug

1. **修复 `key_stats` 结构体大小不匹配**（调整记录槽大小或拆分结构体）
2. **修复 `send_hid_report(MOUSE)` 硬编码 delta**（删除测试代码）
3. **修复键盘报告重复发送**（统一发送路径）
4. **修复 `fault.c` FATAL 复位 + 环形缓冲区回绕**
5. **修复 HID 描述符缺失 Report ID 10-13**
6. **修复宏键码错误**（VK 码 → HID Usage 码转换）

### 第二阶段：激活核心功能

7. **启动 Core1 双核架构**（添加 `multicore_launch_core1()` 调用 + `pico_multicore` 库 + 实现 `shared_hw_data.c` 缺失函数）
8. **修复 `g_config_pending` 配置应用路径**
9. **连接 SettingsPage 与 ConfigProfileManager**
10. **修复 AppAwarenessManager 配置实际应用**

### 第三阶段：补全功能

11. 实现固件更新完整流程（检查 → 下载 → 校验 → 安装）
12. 实现宏播放/停止功能
13. 补全工厂测试 LED 测试 + 入口检测
14. 实现 JoystickPage / EncoderPage 幻影属性的后端支持
15. 统一三处键码映射为单一来源

### 第四阶段：质量提升

16. **补充单元测试**（DeviceService、二进制协议、ViewModel）
17. 清理死代码（`HidConfigTool.Drivers` 项目、`Class1.cs`）
18. 修复资源泄漏（GDI 句柄、HwndSource 钩子、HttpClient）
19. 添加 HID 操作超时机制
20. 更新文档使其与实际状态一致
