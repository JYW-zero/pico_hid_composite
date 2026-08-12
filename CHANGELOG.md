# Changelog

所有重要的变更都记录在这个文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范。

## [Unreleased]

### Added
- 上位机 UI 全面升级（Tokyo Night 深色主题）：
  - 全新自定义标题栏（WindowChrome 实现，支持拖动/缩放/最小化/最大化/关闭）
  - 左侧导航栏（设备状态卡片 + 分组导航菜单 + 版本信息）
  - 统一卡片式页面布局（圆角12px + 阴影）
  - 35个矢量图标资源（Icons.xaml）
  - 自定义控件样式：ToggleSwitch、Slider、ProgressBar、ComboBox、Expander 等
  - 多值转换器（ProgressToWidthConverter、SliderFillConverter、SliderThumbConverter）
- 宏编辑器功能完善：
  - 固定8个宏槽位（固件 MACRO_MAX_COUNT=8），空槽位显示"未配置"并灰色标识
  - 清空宏功能（清空动作 + 重置名称）
  - 动作序号显示（Index 属性）
  - 录制时动作列表自动滚动到最新项
  - 循环设置联动逻辑（循环次数 + 按住重复松开停止）
  - 右侧编辑区支持滚轮滚动
- 性能监控页面优化：
  - 自动刷新按钮状态修复（拆分 RefreshCoreAsync）
  - 按钮内容补全 + MinWidth 防跳动

### Changed
- 重构 MainWindow（自定义标题栏 + 导航栏 + 内容区 + 状态栏）
- 优化所有页面 UI（DevicePage、KeyPage、MousePage、JoystickPage、EncoderPage、MacroPage、SettingsPage、ErrorLogPage、PerfMonitorPage）
- Macro 类实现 INotifyPropertyChanged（Name/RepeatCount/RepeatUntilReleased 变更通知）
- KeyDefinition 类实现 IEquatable<KeyDefinition>（按 KeyCode 比较）
- 宏动作 KeyCode 类型从 int 改为 byte
- 录制按键 KeyCode 自动转换为 HID 用法码（OnKeyNameChanged）

### Fixed
- 修复 ComboBox 选中项文字不显示（ContentPresenter 缺少 SelectionBoxItem 绑定）
- 修复按键下拉框无法匹配（虚拟键码 vs HID 用法码，改用 SelectedValuePath=Name）
- 修复录制按钮 Command 切换失效（本地值优先级覆盖 Style Trigger，默认 Command 移到 Style Setter）
- 修复 DPI 按钮选中态异常（Style Trigger 中设置 Property="Style" 引发运行时异常，改用 Tag="Active"）
- 修复宏列表点击不切换面板（添加 OnCurrentMacroChanged partial 方法）
- 修复清空宏后列表名称不更新（Macro 类实现 INotifyPropertyChanged）
- 修复动作序号显示异常（移除 AlternationCount，改用 Index 属性）
- 修复 TextBox 内部 ScrollViewer 样式冲突（PART_ContentHost 设置 Style="{x:Null}"）
- 修复 Expander 模板 MC3011 错误（ToggleButton 无 Header 属性，改用 Binding）
- 修复 ToggleSwitch 滑块位移失效（嵌套 TranslateTransform 无法被 Trigger 找到，改用 Margin 切换）
- 修复循环设置功能不生效（RepeatCount 与 RepeatUntilReleased 联动 + 序列化正确）

### Removed
- 删除根目录冗余 CMakeLists.txt 和 pico_sdk_import.cmake
- 删除 HidConfigTool.Drivers 项目（未被引用的死代码，含 WinRT HID 驱动实现）
- 删除 Class1.cs 空占位文件（Core 和 Drivers）
- 删除固件中未使用的 encoder_task 和 paw3395_task 函数（硬件扫描已迁移到 Core1）
- 删除 send_hid_report、hid_task、tud_hid_report_complete_cb（TinyUSB 示例残留代码）

### Added（代码审计修复）
- 固件双核架构激活：Core1 负责硬件扫描（键盘/鼠标/编码器/摇杆），Core0 负责业务处理
- shared_hw_data.c 完整实现 24 个函数（原缺失 11 个统计函数）
- HID 描述符补充 Report ID 10-13（按键统计 Feature 报告）
- 宏播放/停止功能：固件 CMD_MACRO_PLAY(0x09)/CMD_MACRO_STOP(0x0A)，上位机完整播放控制
- 上位机自定义 InputDialog 对话框（替代 Microsoft.VisualBasic.InputBox）
- 按键统计页面导航（StatsPage 从不可达变为可访问）
- HID 操作超时机制（5秒超时，防止设备无响应时程序卡住）
- Joystick/Encoder 扩展属性（Sensitivity/InvertX/InvertY/StepsPerTick/ScrollSpeed）
- 上位机自更新：SHA256 哈希验证 + InstallUpdateAsync（支持 exe/zip）
- HidKeyConverter 统一键码转换类（HID Usage↔名称↔虚拟键码）
- 单元测试：HidKeyConverterTests（75个测试）+ DeviceConfigTests 更新

### Fixed（代码审计修复）
- 修复 key_stats 结构体 268B > 记录槽 256B 导致的 Flash 数据损坏（记录槽改为 512B）
- 修复鼠标报告硬编码 delta=5 导致的虚假鼠标移动
- 修复键盘报告重复发送（每次按键双份报告）
- 修复 fault.c FATAL 错误不触发复位（致命错误被忽略）
- 修复 config.c 1.5KB 栈缓冲区溢出风险（改为 static）
- 修复 scheduler.c const 正确性违反
- 修复 core1_scanner.c MOUSE_ACCEL_ENABLE 宏未定义
- 修复 CMakeLists.txt 缺少 pico_multicore 库链接
- 修复 MacroRecorder 录制 VK 码而非 HID Usage 码
- 修复 MacroPageViewModel AddKeyAction 硬编码 KeyCode=65（ASCII）应为 0x04（HID）
- 修复 AppAwarenessManager 配置切换不实际应用（注入 IDeviceService）
- 修复 SettingsPageViewModel 配置管理与 ConfigProfileManager 未连接
- 修复 DeviceService 线程安全缺口（添加 _stateLock）
- 修复三处键码映射重复（统一使用 HidKeyConverter）
- 修复 TrayIconManager GDI 句柄泄漏（DestroyIcon）
- 修复 MainWindow HwndSource 钩子未移除（OnClosed 中移除）
- 修复 UpdateService HttpClient 未 Dispose（实现 IDisposable）

### Changed（代码审计修复）
- 固件主循环任务列表从 7 个减为 5 个（硬件扫描迁移到 Core1）
- keypad_task 重构：从 shared_hw_data 读取稳定按键状态，保留业务处理+宏合并
- mouse_hid_task 重构：从 shared_hw_data 读取位移、滚轮和按键状态
- joystick_task 重构：从 shared_hw_data 读取处理后的摇杆数据
- 上位机解决方案从 4 个项目减为 3 个（移除 Drivers）
- DeviceConfig 模型扩展 5 个新字段

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
