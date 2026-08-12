# 开发记录汇总

本文件汇总了项目开发过程中的所有日志、问题记录和决策。

---

## 📋 项目总览

### 项目基本信息
- **项目名称**：HID复合设备固件（dev_hid_composite）
- **硬件平台**：Raspberry Pi Pico 2 (RP2350) - 双核Cortex-M33
- **开发语言**：C11（固件）、C# .NET 10 + WPF（上位机）
- **目标版本**：v1.0

### 功能清单
- 64键SPI键盘矩阵（双手对称六向按键）
- PAW3395光学鼠标传感器（4档DPI：400/800/1600/3200）
- ADC摇杆（X/Y轴 + 按键）
- 旋转编码器（A/B相 + 中键）
- 宏功能（8个宏，每个32个动作）
- 工厂测试模式
- 可靠性设计

---

## 📌 重要决策记录

### 架构决策
1. **六层架构**：Hardware → Board → Device → Middleware → Protocol → App
   - Protocol 层专门负责 USB HID 协议相关代码（描述符、配置、报文解析）
2. **双核架构**：Core1负责硬件扫描（生产者），Core0负责业务处理（消费者）
3. **设备驱动设计**：零状态、可重入、多实例、无全局变量
4. **依赖管理**：SDK 采用"版本锁定 + 自动拉取"方案
   - 本地优先使用 Pico 扩展 SDK
   - 团队协作时 build.ps1 自动从官方拉取
   - 仓库干净无大文件

### 技术选型
- **USB协议栈**：TinyUSB（官方SDK自带）
- **上位机UI**：WPF + CommunityToolkit.Mvvm（MVVM模式）
- **HID通信**：P/Invoke原生Windows API（hid.dll）

---

## 📅 开发日志

### 2026-08-13 - 全面代码审计与修复

**审计范围**：全部固件源码（21 .c + 23 .h）+ 上位机全部 C#/XAML 文件

**完成的修复**：

#### 严重 Bug 修复（第一阶段）
- F1: key_stats 结构体 268B > 记录槽 256B → 记录槽改为 512B
- F2: shared_hw_data.c 缺失 11 个函数 → 完整实现 24 个函数
- F3: HID 描述符缺失 Report ID 10-13 → 添加 4 个 Feature 报告
- F4: Core1 从未启动 → 完整迁移硬件扫描到 Core1，激活双核架构
- F5: 鼠标报告硬编码 delta=5 → 删除 send_hid_report
- F6: 键盘报告重复发送 → 删除 tud_hid_report_complete_cb 链式发送
- F7: FATAL 错误不触发复位 → watchdog_reboot
- P1: MacroRecorder 录制 VK 码 → HidKeyConverter 转换为 HID Usage
- P2: MacroPageViewModel KeyCode=65 → 改为 0x04
- P3: HidConfigTool.Drivers 死代码 → 删除整个项目

#### 高优先级修复（第二阶段）
- 固件：F9-F13（config_pending、栈溢出、multicore 链接、const 正确性、宏未定义）
- 上位机：P4-P9（InputBox 替换、应用感知、配置管理、线程安全、键码统一、Stats 导航）

#### 资源泄漏与死代码清理（第三/四阶段）
- P11: UpdateService HttpClient 未 Dispose
- P12: TrayIconManager GDI 句柄泄漏
- P13: MainWindow HwndSource 钩子未移除
- 删除固件 encoder_task/paw3395_task（约 90 行）
- 删除上位机 Class1.cs 空文件

#### 新增功能
- 宏播放/停止：固件 CMD_MACRO_PLAY(0x09)/CMD_MACRO_STOP(0x0A) + 上位机完整控制
- HID 超时机制：5 秒超时，防止设备无响应时程序卡住
- Joystick/Encoder 扩展属性：Sensitivity/InvertX/InvertY/StepsPerTick/ScrollSpeed
- 上位机自更新：SHA256 验证 + InstallUpdateAsync（exe/zip）
- HidKeyConverter 统一键码转换类
- 单元测试：HidKeyConverterTests（75 个测试全部通过）

**编译状态**：固件和上位机均 0 错误 0 警告
**测试状态**：75 个单元测试全部通过

**架构变更**：
- 固件：双核架构激活，Core1 硬件扫描，Core0 业务处理
- 上位机：解决方案从 4 个项目减为 3 个（移除 Drivers）

**剩余待处理**：
- 工厂测试 LED/入口检测（需硬件引脚信息）
- 固件配置结构扩展（Joystick/Encoder 属性持久化）
- 设备固件 DFU 更新流程
- 固件单元测试

---

### 2026-08-07
- 项目启动，从MicroPython原型版迁移到C语言
- 搭建五层架构基础框架
- 实现SPI键盘驱动和基础HID功能

### 2026-08-08
- 完成Flash布局管理模块（动态检测Flash大小）
- 修复配置保存问题（页对齐 + 返回值判断）
- 完成宏功能开发和调试
- 性能监控模块升级：
  - 新增超时告警功能
  - 新增任务CPU占比统计
  - 新增10秒/30秒滑动窗口平均
- 上位机按键设置页面UI重设计：
  - 从8x8矩阵改成双手对称六向布局
  - 新增左手/右手切换显示
  - 优化按键大小和字体
- 修复标题栏问题（改成系统标准标题栏）
- 项目上传到GitHub
- 整理项目目录结构

### 2026-08-09 - 项目结构大优化
**主题**：项目结构优化，向规范的开源项目看齐

#### 完成的优化
1. **根目录 CMake 清理**
   - 删除根目录冗余的 CMakeLists.txt 和 pico_sdk_import.cmake
   - build.ps1 改为直接编译 firmware/ 目录
   - 日常开发可单独打开 firmware/ 或 pc_tool/ 目录
   - 保留 build.ps1 作为整体一键编译入口

2. **SDK 智能管理方案**
   - 新增 firmware/lib/ 目录，存放版本锁定文件
   - 创建 .sdk_version 文件锁定 SDK 版本 (2.3.0)
   - 改造 build.ps1，实现四级 SDK 检测机制：
     1. 手动指定路径
     2. 环境变量 PICO_SDK_PATH
     3. Pico VS Code 扩展默认路径
     4. 项目本地 lib/pico-sdk（自动拉取兜底）
   - 未检测到 SDK 时自动从官方浅克隆 + 初始化子模块
   - 仓库干净无大文件，团队协作零门槛

3. **protocol 协议层拆分**
   - 新增 firmware/include/protocol/ 和 src/protocol/ 目录
   - 将 usb_descriptors.c/h 从 app 层移至 protocol 层
   - 将 tusb_config.h 从 app 层移至 protocol 层
   - USB 通信协议与业务逻辑彻底解耦
   - 方便后期修改报告描述符而不影响上层应用

4. **PC 工具目录优化**
   - HidConfigTool.slnx 上移至 pc_tool/ 根目录
   - pc_tool/src/ 只保留四个项目子文件夹（App/Core/Drivers/Tests）
   - 符合 .NET 项目标准结构

5. **新增根目录 tools/**
   - 预留公共脚本目录
   - 存放固件和上位机共用的辅助工具
   - 后续可添加：HID 描述符生成、版本号自动生成等

6. **新增 .gitattributes**
   - 强制 LF 换行符，避免 Windows/Linux 跨平台编译问题
   - 明确文本文件和二进制文件的换行处理规则
   - 批处理文件保持 CRLF

7. **临时文件规范化**
   - 新增 temp/ 临时文件夹
   - 所有截图、调试文件统一存放
   - 已加入 .gitignore，定期清理即可

#### 验证结果
- ✅ 固件编译通过（117 个编译单元，112.5 KB .uf2）
- ✅ SDK 智能检测正常（识别到 Pico 扩展 SDK）
- ✅ protocol 层移动后编译正常
- ✅ 上位机路径更新正常

#### 架构决策更新
- 架构从五层调整为六层：Hardware → Board → Device → Middleware → **Protocol** → App
- Protocol 层专门负责 USB HID 协议相关代码
- 依赖库采用"版本锁定 + 自动拉取"方案，不使用 Git Submodule

---

### 2026-08-12 - 上位机 UI 全面完善

**主题**：上位机配置工具 UI 设计升级，达到成熟商业软件水准

#### 完成的工作

1. **主题系统重构**
   - 重写 DarkTheme.xaml（Tokyo Night 深色配色方案）
   - 新建 Icons.xaml（35个矢量图标，随控件文字颜色自动变化）
   - 新建 ProgressConverters.cs（多值转换器：进度条、滑块填充、滑块位置）
   - 自定义控件样式：ToggleSwitch、Slider、ProgressBar、ComboBox、Expander、ScrollViewer 等

2. **主窗口重构**
   - 使用 WindowChrome 实现自定义标题栏（支持拖动/缩放/系统按钮）
   - 左侧导航栏（设备状态卡片 + 分组导航菜单 + 版本信息）
   - 右侧内容区（带淡入动画的页面切换）
   - 底部状态栏（连接状态 + 就绪指示）

3. **所有页面 UI 优化**
   - DevicePage：设备列表 + 设备信息 + 设备操作卡片
   - KeyPage：层切换 + 手部分配 + 手指按键映射（六向按键网格）
   - MousePage：DPI 档位 + 指针加速（ToggleSwitch + Slider）
   - JoystickPage / EncoderPage：统一卡片式布局
   - MacroPage：左右分栏宏编辑器（重点优化，见下文）
   - SettingsPage：Expander 折叠面板分组
   - ErrorLogPage / PerfMonitorPage：统一按钮样式 + 表格优化

4. **宏编辑器重点优化**
   - 固定8个宏槽位（固件限制），空槽位灰色显示"未配置"
   - 清空宏功能（清空动作 + 重置名称，带确认弹窗）
   - 动作序号显示（Index 属性，从1开始）
   - 录制时动作列表自动滚动到最新项
   - 循环设置联动逻辑（循环次数与"按住重复松开停止"互斥联动）
   - 右侧编辑区支持滚轮滚动
   - 按键下拉框按名称匹配（SelectedValuePath=Name）

5. **模型类完善**
   - Macro 类实现 INotifyPropertyChanged（Name/RepeatCount/RepeatUntilReleased）
   - KeyDefinition 类实现 IEquatable<KeyDefinition>（按 KeyCode 比较）
   - 宏动作 KeyCode 类型从 int 改为 byte
   - 录制按键 KeyCode 自动转换为 HID 用法码

#### 遇到的关键问题与解决方案

1. **ComboBox 选中项文字不显示**
   - 原因：自定义模板中 ContentPresenter 缺少 `Content="{TemplateBinding SelectionBoxItem}"` 绑定
   - 解决：补充 SelectionBoxItem / SelectionBoxItemTemplate / SelectionBoxItemStringFormat 绑定

2. **按键下拉框无法匹配选中项**
   - 原因：录制时 KeyCode 是 Win32 虚拟键码（如 G=71），CommonKeys 中是 HID 用法码（如 G=10），数值不同
   - 解决：改用 `SelectedValuePath="Name"` + `SelectedValue="{Binding KeyName}"` 按按键名称匹配

3. **录制按钮 Command 切换失效**
   - 原因：Button 元素上直接设置的 Command 是本地值，WPF 本地值优先级高于 Style Trigger
   - 解决：把默认 Command 从 Button 元素移到 Style 的 Setter 中

4. **DPI 按钮选中态引发运行时异常**
   - 原因：在 Style Trigger 中设置 `Property="Style"` 会引发异常
   - 解决：改用 `Tag="Active"` 触发选中态

5. **ToggleSwitch 滑块位移失效**
   - 原因：嵌套 TranslateTransform 无法被 Trigger 按名称找到
   - 解决：改用 Margin 切换实现滑块位移

6. **宏列表点击不切换面板**
   - 原因：CurrentMacro 变化时没有加载对应动作
   - 解决：添加 `OnCurrentMacroChanged` partial 方法

7. **循环设置功能不生效**
   - 原因：RepeatCount 与 RepeatUntilReleased 是独立属性，无联动；固件用 repeat_count=0 表示无限循环
   - 解决：在 Macro 类中实现属性联动，开启"按住重复"时 RepeatCount 自动设为0

#### 验证结果
- ✅ 编译通过（0 警告 0 错误）
- ✅ 所有页面 UI 正常显示
- ✅ 宏录制/编辑/保存功能正常
- ✅ 循环设置联动正常

---

## 🐛 问题记录与解决方案

### 001 - HID配置块写入后读取失败
**问题**：上位机写入配置后，读回来的数据不对。
**原因**：Flash写入大小没有对齐到256字节页边界。
**解决方案**：写入大小向上取整到256字节的倍数。

### 002 - 写Flash导致USB句柄失效
**问题**：写入Flash后，上位机的HID设备句柄失效，后续通信失败。
**原因**：写Flash时USB中断被长时间关闭，导致设备重枚举。
**解决方案**：写完后重新连接设备 + 重试机制。

### 003 - HID开发踩坑总结
#### 上位机端
1. HID报告长度必须完整，Windows要求缓冲区大小等于报告大小
2. 设备枚举找不到设备：SP_DEVICE_INTERFACE_DETAIL_DATA的cbSize值不对
3. Feature报告读写用hid.dll标准函数，不用DeviceIoControl
4. 复合设备连错接口：过滤只返回UsagePage == 0xFF00的设备
5. 控制端点包长限制：RP2350最大64字节，需要分块传输

#### 固件端
1. TinyUSB会自动处理Report ID，应用层回调里buffer已经跳过了Report ID
2. 双接口方案后PID固定为0x4004

### 004 - Pico 2 Flash大小识别问题
**问题**：代码里假设Flash是4MB，但实际开发板只有2MB。
**原因**：第三方创客板用了2MB Flash代替官方的4MB。
**解决方案**：使用pico-sdk官方API运行时检测Flash实际大小，动态计算地址。

---

## 📊 项目进度

### 已完成
- ✅ 基础架构搭建
- ✅ 键盘驱动
- ✅ 鼠标驱动（PAW3395）
- ✅ 摇杆驱动
- ✅ 编码器驱动
- ✅ 配置管理（双备份 + CRC）
- ✅ 宏功能
- ✅ 性能监控（已升级）
- ✅ 错误日志
- ✅ 按键统计
- ✅ 上位机基础功能
- ✅ 上位机按键设置页面（双手布局）

### 进行中
- 🔄 上位机其他页面完善
- 🔄 整体测试优化

### 待完成
- ⏳ 工厂测试模式完善
- ⏳ 低功耗模式
- ⏳ 更多宏动作类型
- ⏳ 固件在线升级

---

## 2026-08-10 更新记录

### 完成的工作
- 修复上位机性能监控页面报错（Run.Text 绑定默认 TwoWay，只读属性报错，加 Mode=OneWay）
- 添加上位机设备操作按钮（重启设备、进入烧录模式）
- 实现固件软件重启命令（CMD_REBOOT）
- 实现固件进入 BOOTSEL 模式命令（CMD_ENTER_DFU）
- 统一 build 目录（删除根目录 build/，只保留 firmware/build/）
- 完善 .gitignore 规则
- 优化项目目录结构（固件与上位机分离）

### 问题记录
1. **WPF Run.Text 绑定报错**
   - 问题：点击性能监控页面报错，提示无法对只读属性进行 TwoWay 绑定
   - 原因：WPF 中 Run.Text 属性的绑定默认模式是 TwoWay，而绑定的源属性是只读的
   - 解决方案：给所有 Run.Text 绑定显式加上 Mode=OneWay

2. **OpenOCD Windows 路径问题**
   - 问题：SWD 烧录时提示找不到文件，路径里的反斜杠被吃掉了
   - 原因：OpenOCD 在 Windows 下会把路径中的反斜杠 \ 当作转义字符
   - 解决方案：把路径中的反斜杠替换成正斜杠 /

3. **watchdog_reboot 延迟参数问题**
   - 问题：软件重启命令好像没生效
   - 原因：watchdog_reboot(0, 0, 0) 第三个参数（延迟时间）传 0 可能不触发复位
   - 解决方案：改成 watchdog_reboot(0, 0, 1)，并加上死循环等待复位生效

### 当前进度
- 已完成：基础功能全部实现，上位机 UI 基本完成
- 进行中：软件重启功能验证、整体测试优化
- 待完成：工厂测试模式完善、低功耗模式、固件在线升级
