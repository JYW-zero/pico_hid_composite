# 功能角色分离与产品形态拆分方案（Feature / Role Split Plan）

> 文档版本：v1.1（阶段 0 / 1 / 2 已落地）
> 创建日期：2026-10-02
> 最近更新：2026-10-05（阶段 2 双 Profile 落地，JYW）
> 作者：JYW <JYW@outlook.com>
> 状态：**已拍板，作为后续所有相关 PR 的唯一依据；阶段 0 / 1 / 2 已完成，阶段 3 / 4 待实施**
> 适用代码基线：固件协议 v0.2、配置结构 v8（`device_config_t` 实测 **1510 字节**，已加 `_Static_assert` 锁定）、Pico SDK 2.3.0 / RP2350、上位机 .NET 10
>
> 变更：v1.0（2026-10-02）决策冻结；v1.1（2026-10-05）阶段 2 落地——release/dev 双目标与裁剪矩阵（§8.2）、Fault 双后端与紧凑档/磨损均衡定稿（§11.1）、过渡制造门控、Provisioning 分区占位（§9.1）、ABI 断言、CI 双构建。

---

## 0. 决策记录（已冻结，勿再改动方向）

| 编号 | 决策点 | 最终选择 |
|------|--------|----------|
| D1 | 上位机产品形态 | **1-C：消费者单 App（标准/开发者模式）+ 独立内部工厂工具（FactoryTool）** |
| D2 | 固件形态 | **出两套构建目标：`firmware_release` 与 `firmware_dev`** |
| D3 | 量产固件中的诊断能力 | **保留精简 Fault 错误记录（售后刚需）**，关闭任务级性能监控/按键统计/raw 诊断/UART |
| D4 | 工厂测试去留 | **工厂测试代码在 release 中保留编译，但由 Flash manufacturing flag 运行时门控：产线态可进入、出厂锁定、售后受控重开** |

> **关于 D2 与 D4 的统一解释（重要）**：量产产线烧录的就是 release 固件，因此工厂自检代码**必须编进 release**，否则无法"出厂后售后重开"。所以 `FEATURE_FACTORY_TEST` 在 release/dev **都为 1**，release 与 dev 的差异在于 UART / 任务级 perf / raw 诊断 / 按键统计，以及 manufacturing flag 的默认状态。release 对工厂测试的限制是**运行时锁定**，不是编译期裁剪。

---

## 1. 背景与问题

当前代码按**技术分层**组织（固件六层、上位机 MVVM），但缺少"**使用者角色**"这一维度，导致面向消费者、发烧友/开发者、产线/售后的功能堆叠在同一固件与同一界面中。

本方案**不删除功能、不拆成互不相干的工程**，而是建立"**一套代码、多种形态**"：在固件构建、HID 协议、上位机界面三个层面，用统一的"角色 + 能力"开关功能。

### 1.1 现有可复用基础（阶段 1 是改造而非新建）

- Device Info 报告（Report ID `0x06`）**已包含 64 位 `capability_bitmap` 字段**，见 `firmware/include/protocol/config_hid.h` 的 `config_hid_device_info_t`。
- 固件已有能力枚举 `capability_id_t`（bit 0–15），上位机 `VersionComparer.cs` 有一一对应的 `CapabilityId` 枚举。
- 命令 `CMD_GET_CAPABILITY_INFO = 0x13` 已支持按能力 ID 查询能力版本。
- `firmware/src/protocol/config_lock.c` 已有"命令是否需要解锁"的白名单 `switch-case`，是命令闸门的天然落点。
- 工厂测试 `firmware/src/app/factory_test.c` 已是独立运行时模式（启动时长按 Fn 进入，`main.c` 调用 `factory_test_enter()` 阻塞执行），未与正常业务耦合。
- Flash 布局 `flash_layout.h` 已统一从末尾向前分配，新增分区不会改变既有区域偏移。

### 1.2 当前短板（本方案要补的）

- 固件 `CMakeLists.txt` 用 `file(GLOB ...)` 全量编译，**没有任何功能裁剪开关**。
- `capability_bitmap` 当前在 `config_hid.c` 中**硬编码全置位**，未与实际编译功能挂钩。
- 命令没有分级，诊断/制造类命令与消费者命令走同一分发路径，缺少固件侧强校验。
- 上位机页面无"角色"概念，所有页面常驻导航。

---

## 2. 目标发布物

| 发布物 | 形态 | 面向角色 | 功能范围 | 发布渠道 |
|--------|------|----------|----------|----------|
| `firmware_dev.uf2` | 工程固件（全功能） | 开发者 / 硬件调试 | 全部功能 + UART + raw 诊断，工厂测试默认不锁 | 内部 / GitHub Pre-release |
| `firmware_release.uf2` | 量产固件（裁剪 + 门控） | 消费者 / 产线 / 售后 | 消费者功能 + 精简 Fault + 工厂测试（flag 门控） | 正式 Release，产线烧录 |
| `pico_hid_bootloader.uf2` | Bootloader | 全部 | **不变**，DFU + 签名验证在 release 强制保留 | 正式 Release |
| **HID Config Tool**（ConsumerApp） | 消费者配置工具（WPF + Avalonia） | 消费者（默认）/ 开发者（模式解锁） | 标准模式仅消费者页；开发者模式显示诊断页 | 正式 Release，对外 |
| **HID Factory Tool**（FactoryTool） | 内部工厂/售后工具 | 产线 / 售后 | 工厂自检 PASS/FAIL、序列号/校准烧录、出厂锁定、日志回收 | **仅内部，不随消费者 Release 发布** |

> ConsumerApp 与 FactoryTool 共享现有 `HidConfigTool.Core` / `HidConfigTool.Hid` / `HidConfigTool.ViewModels`，不重复实现协议层。

---

## 3. 三类角色定义

| 角色 | 说明 | 典型操作 |
|------|------|----------|
| **消费者 Consumer** | 购买成品的普通用户，默认形态 | 改键、DPI、摇杆/编码器、宏、预设、主题语言、一键固件升级、恢复出厂 |
| **开发者 Developer** | 发烧友、固件/上位机开发者、技术支持 | 上述全部 + 性能监控、错误/故障日志、综合诊断、按键实时测试、使用统计 |
| **工厂/售后 Factory/Service** | 产线测试工位、维修售后 | 硬件自检、序列号与校准烧录、良率采集、出厂锁定、售后解锁与日志回收 |

角色由两个**正交**维度共同决定（缺一不可）：

- **设备能力（Device Capability）**：固件编译进了什么、manufacturing flag 处于什么状态——设备说了算，通过 `capability_bitmap` 宣告。
- **本地角色（Local Role）**：上位机当前处于标准模式还是开发者模式——本地设置，控制界面显隐。

---

## 4. 功能归属总表

### 4.1 上位机页面归属

| 页面（`PageType`） | 所需本地角色 | 所需能力位 | ConsumerApp | FactoryTool |
|--------------------|:---:|:---|:---:|:---:|
| Dashboard（首页） | 消费者 | —（连接即可） | ✅ | ✅ |
| Device（设备信息/版本兼容） | 消费者 | — | ✅ | ✅ |
| KeyManagement（4 层按键映射） | 消费者 | `CAP_KEYMAP(4)` | ✅ | — |
| Mouse（DPI/LOD/角度捕捉） | 消费者 | `CAP_MOUSE_BASIC(1)`+`CAP_DPI_SLOTS(14)` | ✅ | — |
| Joystick（死区/灵敏度/校准） | 消费者 | `CAP_JOYSTICK(2)`+`CAP_JOYSTICK_CALIB(15)` | ✅ | ◑ 校准初值烧录 |
| Encoder（编码器） | 消费者 | `CAP_ENCODER(3)` | ✅ | — |
| Macro（宏录制/编辑/播放） | 消费者 | `CAP_MACRO(5)` | ✅ | — |
| Presets（预设/导入导出/备份） | 消费者 | `CAP_CONFIG_STORAGE(6)` | ✅ | — |
| Settings（主题/语言/自启/托盘/OSD） | 消费者 | 纯本地 | ✅ | — |
| FirmwareUpdate（DFU 一键升级） | 消费者 | `CAP_DFU_UPGRADE(10)` | ✅ | ✅ 产线烧录 |
| KeyTest（按键/摇杆实时测试） | 开发者 | `CAP_DIAG_RAW_STATE(16)` | 仅开发者模式 | ✅ 必测 |
| Stats（按键使用统计） | 开发者 | `CAP_KEY_STATS(9)` | 仅开发者模式 | ◑ 耐久试验 |
| PerfMonitor（CPU/任务/阈值） | 开发者 | `CAP_PERF_MONITOR(7)` | 仅开发者模式 | ✅ |
| ErrorLog（Fault 日志读取/导出） | 开发者 | `CAP_FAULT_LOG(8)` | 仅开发者模式（release 只读精简版） | ✅ |
| Diagnostics（综合诊断） | 开发者 | 多个诊断位聚合 | 仅开发者模式 | ✅ |
| **Factory Test（一键自检/烧录/锁定）** | 工厂 | `CAP_MFG_FACTORY_TEST(24)`+`CAP_MFG_PROVISION(25)` | ❌ 不含 | ✅ 核心 |

### 4.2 固件模块与功能开关归属

| 固件模块 / 数据 | FEATURE 开关 | dev | release | 说明 |
|---|---|:---:|:---:|---|
| 键盘/鼠标/摇杆/编码器/多媒体/Gamepad 上报 | 始终编入 | ✅ | ✅ | 核心 HID 功能 |
| 配置存储（双备份/CRC/迁移） | 始终编入 | ✅ | ✅ | `device_config_t` 结构两版完全一致 |
| 宏引擎 | 始终编入 | ✅ | ✅ | 消费者功能 |
| DFU 应用侧 + Bootloader 签名校验 | 始终编入 | ✅ | ✅ | 消费者升级与产线烧录共用，安全链不可裁剪 |
| 精简 Fault 记录 | `FEATURE_FAULT_LOG` | ✅ | ✅（精简） | D3：release 保留只读、掉电不丢，供售后回收 |
| 工厂自检 `factory_test.c` | `FEATURE_FACTORY_TEST` | ✅ | ✅（**门控**） | D4：release 编入但由 manufacturing flag 锁定 |
| 任务级性能监控 `perf_monitor.c` | `FEATURE_PERF_MONITOR` | ✅ | ❌ | release 不启动任务统计/阈值任务 |
| 按键使用统计 | `FEATURE_KEY_STATS` | ✅ | ❌ | 属耐久/研发分析 |
| UART 调试日志（GP0/GP1，115200） | `FEATURE_DEBUG_UART` | ✅ | ❌ | release 关闭调试串口输出 |
| 实时 raw 诊断（KeyState/JoyState、原始命令） | `FEATURE_RAW_DIAG` | ✅ | ❌ | release 不提供实时状态报告 |
| Provisioning/制造数据区 | 始终编入 | ✅ | ✅ | 序列号、manufacturing flag、出厂校准 |

> **红线**：`device_config_t`（`firmware/include/board/config.h`）的**大小与字段偏移在 release/dev 两版中必须完全相同**，字段只增不删。`perf_thresholds[8]`（v8）等诊断字段在 release 中保留占位、仅不使用。所有诊断类持久化数据（Fault、KeyStats、Provisioning）放在独立 Flash 区，不与配置结构混排。

---

## 5. HID 命令三级分级

分级同时覆盖 **Control 命令（Report ID `0x07`）** 与 **Feature Report（GET/SET_REPORT）**。命令码与 Report ID 均以 `firmware/include/protocol/config_hid.h` 与 `ProtocolConstants.cs` 现状为准。

### 5.1 Public（消费者，release/dev 均可用）

| 命令码 | 名称 | 备注 |
|--------|------|------|
| `0x01` | CMD_SAVE_CONFIG | 写 Flash，需先 `0x0D` 解锁配置 |
| `0x02` | CMD_RESET_CONFIG | 恢复出厂，需解锁 |
| `0x03` | CMD_REBOOT | 既有 3 次确认 |
| `0x04` | CMD_ENTER_DFU | 既有 3 次确认 |
| `0x05` | CMD_APPLY_CONFIG | 需解锁 |
| `0x09` | CMD_MACRO_PLAY | |
| `0x0A` | CMD_MACRO_STOP | |
| `0x0C` | CMD_SET_JOYSTICK_DZ_RT | 实时预览，不写 Flash |
| `0x0D` | CMD_UNLOCK_CONFIG | 既有配置写解锁（连续 3 次 / 5 秒） |
| `0x0E` | CMD_JOYSTICK_CALIBRATE | 消费者校准 |
| `0x0F` | CMD_SWITCH_DPI | |
| `0x10` | CMD_GET_VERSION | |
| `0x11` | CMD_NEGOTIATE_VERSION | |
| `0x12` | CMD_SWITCH_LAYER | |
| `0x13` | CMD_GET_CAPABILITY_INFO | 能力发现，必须公开 |
| `0x14` | CMD_ENTER_BOOTSEL | 既有 3 次确认 |

**Public Feature Report**：Device Info `0x06`、Config Block `0x05/0x08/0x09`、Macro Config `0x0E(14)`、Config Ext `0x15(21)`、NKRO `0x16(22)`、Joystick Calib `0x17(23)`、Config Ext2 `0x19(25)`。

### 5.2 Diagnostic（诊断，受能力位 + 本地开发者模式约束）

| 命令码 | 名称 | 关联能力位 | release 行为 |
|--------|------|-----------|--------------|
| `0x06` | CMD_RESET_STATS | `CAP_KEY_STATS(9)` | `NOT_SUPPORTED` |
| `0x07` | CMD_CLEAR_FAULT | `CAP_FAULT_LOG(8)` | release 保留只读；清除需售后解锁（见 §9），否则 `LOCKED` |
| `0x08` | CMD_RESET_PERF | `CAP_PERF_MONITOR(7)` | `NOT_SUPPORTED` |
| `0x0B` | CMD_SET_PERF_ENABLE | `CAP_PERF_MONITOR(7)` | `NOT_SUPPORTED` |
| `0x15` | CMD_SET_PERF_THRESHOLD | `CAP_PERF_MONITOR(7)` | `NOT_SUPPORTED` |

**Diagnostic Feature Report**：

| Report ID | 名称 | release 行为 |
|-----------|------|--------------|
| `0x0A–0x0D`（10–13） | Key Stats 0–3 | 不编入，`NOT_SUPPORTED` |
| `0x0F`（15） | Perf System | 不编入，`NOT_SUPPORTED` |
| `0x10`（16） | Perf Task | 不编入，`NOT_SUPPORTED` |
| `0x11`（17） | Fault Info | **保留（精简），只读** |
| `0x12`（18） | Fault Log | **保留（精简），只读** |
| `0x13`（19） | Key State（实时 bitmap） | 不编入（`FEATURE_RAW_DIAG=0`） |
| `0x14`（20） | Joystick State（实时） | 不编入（`FEATURE_RAW_DIAG=0`） |

### 5.3 Manufacturing（制造，仅 FactoryTool；受能力位 + manufacturing flag 双重门控）

当前应用固件**没有**制造类 HID 命令（工厂自检目前仅由启动时长按 Fn 进入、走本地/UART）。阶段 3 新增，**拟分配命令码段 `0x20–0x2F`、Feature Report 段 `0x1A–0x1F`**（均为当前空闲段，定稿于 `config_hid.h`）：

| 拟用命令码 | 名称 | 说明 |
|------------|------|------|
| `0x20` | MFG_GET_STATE | 读取 manufacturing flag / 序列号 / 出厂状态 |
| `0x21` | MFG_TEST_START | 启动工厂自检（单项或全部） |
| `0x22` | MFG_TEST_GET_RESULT | 读取自检结果（PASS/FAIL/明细） |
| `0x23` | MFG_WRITE_SERIAL | 写入序列号 |
| `0x24` | MFG_WRITE_CALIBRATION | 写入出厂校准初值（摇杆中心等） |
| `0x25` | MFG_SHIP_LOCK | 出厂提交并锁定（置位 shipped flag） |
| `0x26` | MFG_AFTERSALES_UNLOCK | 售后受控解锁（需凭证，见 §9） |

> DFU（Bootloader Report `0x30–0x36`）独立于应用分级：消费者一键升级与产线烧录共用，release 必须保留并强制固件签名校验。
>
> **实现注记（2026-10-06，JYW）**：bit10 不再无条件置 1，而由编译开关 `FEATURE_DFU_BOOTLOADER` 严格决定——自定义 bootloader 未编入时位图清 bit10、固件命令闸门对 raw `CMD_ENTER_DFU(0x04)` 回 `NOT_SUPPORTED`、上位机隐藏一键/在线 DFU；`CMD_ENTER_BOOTSEL(0x14)` 为 RP2350 原生 UF2 能力，不占能力位、始终放行并作为当前主推升级方式。当前 release/dev 两 profile 均临时置 0（bootloader 待恢复）；恢复 bootloader 后把对应 profile 置 1，即回到本节“release 必须保留”的目标态。详见 `CODE_REVIEW_FEATURE_SPLIT.md` §3 P1-2。CH582 移植后升级路径改变（无 UF2 盘、非标准 DFU），bit10 语义需重定义。

### 5.4 闸门返回码

| 场景 | 返回 |
|------|------|
| 命令/报告未编入（对应能力位为 0） | `CONFIG_HID_ERR_NOT_SUPPORTED (9)` |
| 诊断功能在当前会话不允许 | `CONFIG_HID_ERR_DIAG_LOCKED (11)`（新增） |
| 制造功能已出厂锁定 / 未授权 | `CONFIG_HID_ERR_MFG_LOCKED (12)`（新增） |
| 既有配置写锁 | `CONFIG_HID_ERR_CONFIG_LOCKED (3)`（不变） |

> 错误码现状：`0–10` 已定义（`config_hid_error_t`），`11/12` 空闲，直接新增。上位机 `HidErrorMapper` 同步映射为本地化提示。

---

## 6. 能力位图定义（capability_bitmap）

Device Info 的 `capability_bitmap` 为 64 位。**bit 0–15 已被现有 `capability_id_t` 占用，不可复用**（用户早期草图中的 `1<<10`、`1<<20` 会与现有 `CAP_DFU_UPGRADE(10)` 等冲突，弃用）。按下表分段扩展：

### 6.1 功能能力段（bit 0–15，保持现状，仅改变置位来源）

| bit | 能力 | 置位条件（release） |
|-----|------|---------------------|
| 0 | CAP_KEYBOARD_BASIC | 始终 1 |
| 1 | CAP_MOUSE_BASIC | 始终 1 |
| 2 | CAP_JOYSTICK | 始终 1 |
| 3 | CAP_ENCODER | 始终 1 |
| 4 | CAP_KEYMAP | 始终 1 |
| 5 | CAP_MACRO | 始终 1 |
| 6 | CAP_CONFIG_STORAGE | 始终 1 |
| 7 | CAP_PERF_MONITOR | `FEATURE_PERF_MONITOR`（release=0） |
| 8 | CAP_FAULT_LOG | `FEATURE_FAULT_LOG`（release=1，精简只读） |
| 9 | CAP_KEY_STATS | `FEATURE_KEY_STATS`（release=0） |
| 10 | CAP_DFU_UPGRADE | `FEATURE_DFU_BOOTLOADER`（仅自定义 in-app bootloader 随固件编入时置 1；当前两 profile 临时禁用 bootloader 故=0，恢复后目标态=1。RP2350 原生 BOOTSEL UF2 不占此位、始终可用） |
| 11 | CAP_NKRO | 始终 1 |
| 12 | CAP_CONSUMER | 始终 1 |
| 13 | CAP_GAMEPAD | 始终 1 |
| 14 | CAP_DPI_SLOTS | 始终 1 |
| 15 | CAP_JOYSTICK_CALIB | 始终 1 |

### 6.2 诊断能力段（bit 16–23，新增）

| bit | 能力 | 置位条件 |
|-----|------|----------|
| 16 | **CAP_DIAG_RAW_STATE** | `FEATURE_RAW_DIAG`（实时 KeyState/JoyState；release=0） |
| 17 | **CAP_DIAG_DEBUG_UART** | `FEATURE_DEBUG_UART`（调试串口/详细日志；release=0） |
| 18–23 | 预留 | — |

### 6.3 制造能力段（bit 24–31，新增）

| bit | 能力 | 置位条件 |
|-----|------|----------|
| 24 | **CAP_MFG_FACTORY_TEST** | `FEATURE_FACTORY_TEST` **且** manufacturing flag 未处于"出厂锁定"（或本会话已售后解锁） |
| 25 | **CAP_MFG_PROVISION** | 同上（序列号/校准烧录、出厂锁定） |
| 26–31 | 预留 | — |

实施要点：

- `capability_id_t` 末尾扩展新枚举，`CAP_COUNT` 由 16 增至 **26**；`config_hid.c` 中 `capability_versions[]` 数组同步扩展。
- `config_hid.c:270` 处硬编码位图改为**编译期 `#if FEATURE_*` 拼装 + 运行时读取 manufacturing flag** 生成（见 §8）。
- 上位机 `CapabilityId` 枚举（`VersionComparer.cs`）同步新增 `DiagRawState=16 / DiagDebugUart=17 / MfgFactoryTest=24 / MfgProvision=25`。
- **能力来源以固件位图为唯一权威**；上位机仅在读取位图失败（老固件）时，临时回退到按协议版本推断并明确标注"兼容回退"。

---

## 7. Device Info 报告扩展

`config_hid_device_info_t` 扩展前为 21 字节（含 report_id；HID 数据区 20 字节），Report `0x06` 按 32 字节申报。在**不改 `device_config_t`** 的前提下扩展 HID 报告，扩展后为 32 字节（含 report_id；数据区 31 字节）。下表偏移以**含 report_id 的 packed 结构**为准，括号内为去掉 report_id 后的 HID 数据区下标（即上位机 `ParseDeviceInfo` 在 `offset` 之后的相对位置）：

| 新增字段 | 结构偏移（数据区下标） | 大小 | 说明 |
|----------|------|------|------|
| `device_flags` | 21（byte 20） | 1 | bit0=`MFG_SHIPPED_LOCKED`（已出厂锁定）；bit1=`MFG_SESSION_UNLOCKED`（本会话售后已解锁）；bit2–7 预留 |
| `reserved[10]` | 22–31（byte 21–30） | 10 | 对齐到 32 字节（含 report_id），留作后续序列号短码等 |

- 该变更属于 **HID 协议变更**：配置 HID 协议次版本号 `v0.1 → v0.2`（`version.h` 与 `ProtocolConstants.cs` 同步），通过既有 `CMD_NEGOTIATE_VERSION` 协商。
- 上位机按"读取长度"做向后兼容解析：老固件返回短报告时，`device_flags` 按 0 处理，不得越界。
- Device Info 是 HID 报告而非配置结构，此扩展**不触碰**配置二进制兼容红线。

---

## 8. 固件侧实现设计

### 8.1 CMake feature option（阶段 0 搭框架，默认全开，行为不变）

在 `firmware/CMakeLists.txt` 顶层引入：

```cmake
option(FEATURE_FACTORY_TEST "Factory self-test (runtime-gated by mfg flag in release)" ON)
option(FEATURE_PERF_MONITOR "Task-level performance monitor" ON)
option(FEATURE_FAULT_LOG    "Fault/error log (release keeps a slim read-only version)" ON)
option(FEATURE_KEY_STATS    "Per-key usage counters" ON)
option(FEATURE_DEBUG_UART   "Debug UART and verbose logging" ON)
option(FEATURE_RAW_DIAG     "Raw realtime diagnostic reports (KeyState/JoystickState)" ON)
```

阶段 0 仅生成一个可通过 `-D` 覆盖的默认目标（全开），不改变现状；阶段 2 增加两个可执行目标。

### 8.2 release / dev 编译矩阵（阶段 2，✅ 已落地）

| FEATURE | firmware_dev | firmware_release |
|---------|:---:|:---:|
| FEATURE_FACTORY_TEST | 1 | **1（保留，运行时 flag 门控，D4）** |
| FEATURE_PERF_MONITOR | 1 | **0** |
| FEATURE_FAULT_LOG | 1 | **1（精简只读，D3）** |
| FEATURE_KEY_STATS | 1 | **0** |
| FEATURE_DEBUG_UART | 1 | **0** |
| FEATURE_RAW_DIAG | 1 | **0** |
| FEATURE_FAULT_COMPACT（阶段 2 新增） | 0 | **1（16B 紧凑记录 + 磨损均衡）** |
| FEATURE_MFG_DEFAULT_LOCKED（阶段 2 新增） | 0 | **1（上电默认锁定制造能力，过渡方案）** |
| LOG_LEVEL | 1 | **0（关闭详细日志）** |

```cmake
# 实际实现（CMakeLists.txt）：共享同一套 file(GLOB) 源集，function
# add_firmware_variant(TARGET PROFILE) 仅以 target_compile_definitions 区分。
target_compile_definitions(firmware_dev PRIVATE
    FEATURE_FACTORY_TEST=1 FEATURE_PERF_MONITOR=1 FEATURE_FAULT_LOG=1
    FEATURE_KEY_STATS=1 FEATURE_DEBUG_UART=1 FEATURE_RAW_DIAG=1
    FEATURE_FAULT_COMPACT=0 FEATURE_MFG_DEFAULT_LOCKED=0)

target_compile_definitions(firmware_release PRIVATE
    FEATURE_FACTORY_TEST=1 FEATURE_PERF_MONITOR=0 FEATURE_FAULT_LOG=1
    FEATURE_KEY_STATS=0 FEATURE_DEBUG_UART=0 FEATURE_RAW_DIAG=0
    FEATURE_FAULT_COMPACT=1 FEATURE_MFG_DEFAULT_LOCKED=1 LOG_LEVEL=0)
```

**裁剪方式（已实现）**：模块入口/任务创建/报告分发/命令处理一律 `#if FEATURE_XXX` 包裹；被裁剪模块采用**整翻译单元条件编译 + 头文件 `static inline` 空桩**（`perf_monitor.h` / `key_stats.h` 在开关关闭时提供全套空桩，类型始终保留），因此调用点无需散落 `#if`，release/dev 源集完全相同。

**实测占用（arm-none-eabi-gcc 15.2.1，PICO_BOARD=pico2，Release）**：

| 目标 | text（Flash 代码/只读） | bss（RAM） |
|------|:---:|:---:|
| firmware_dev | 101 016 B | 22 196 B |
| firmware_release | 91 836 B | 20 448 B |
| release 相对节省 | **9 180 B（≈8.96 KB）** | **1 748 B（≈1.71 KB）** |

> 说明：release 虽裁掉 perf / KeyStats，但编入了紧凑 Fault 后端（分类表 + CRC16 + 磨损均衡，约 2.3 KB），故净节省为上述数值。

### 8.3 命令闸门（阶段 1，落点 config_hid.c / config_lock.c）

不另起炉灶，在现有命令分发与 `config_lock.c` 白名单基础上增加"级别 + 能力"判定：

```c
typedef enum { CMD_LEVEL_PUBLIC = 0, CMD_LEVEL_DIAG, CMD_LEVEL_MFG } cmd_level_t;

/* 伪代码：在命令分发入口统一执行 */
static uint8_t gate_command(uint8_t cmd) {
    const cmd_entry_t *e = cmd_table_lookup(cmd);
    if (!e) return CONFIG_HID_ERR_INVALID_CMD;
    if ((capability_bitmap_runtime() & e->required_cap) == 0)
        return CONFIG_HID_ERR_NOT_SUPPORTED;          /* 未编入：9 */
    if (e->level == CMD_LEVEL_MFG && !mfg_is_unlocked())
        return CONFIG_HID_ERR_MFG_LOCKED;             /* 出厂锁定：12 */
    if (e->level == CMD_LEVEL_DIAG && !diag_allowed(e))
        return CONFIG_HID_ERR_DIAG_LOCKED;            /* 诊断受限：11 */
    return CONFIG_HID_OK;
}
```

> **阶段 1 简化原则（YAGNI）**：release 对 perf/keystats/rawdiag 直接**编译期不编入**，能力位即为 0，天然返回 `NOT_SUPPORTED`，无需会话级"诊断解锁"命令。`ERR_DIAG_LOCKED(11)` 预留给"已编入但需临时授权"的场景（典型为 release 中清除 Fault 日志的售后操作，与 §9 售后凭证合并实现）。Feature Report 的 `get_report_cb` 必须执行同等闸门，防止绕过命令直接 `GET_REPORT` 读取诊断数据。

### 8.4 能力位图运行时生成

```c
uint64_t capability_bitmap_runtime(void) {
    uint64_t b = CAP_BASE_BITMAP;   /* bit0-6,10-15 消费者能力 */
#if FEATURE_PERF_MONITOR
    b |= (1ULL << CAP_PERF_MONITOR);
#endif
#if FEATURE_FAULT_LOG
    b |= (1ULL << CAP_FAULT_LOG);
#endif
#if FEATURE_KEY_STATS
    b |= (1ULL << CAP_KEY_STATS);
#endif
#if FEATURE_RAW_DIAG
    b |= (1ULL << CAP_DIAG_RAW_STATE);
#endif
#if FEATURE_DEBUG_UART
    b |= (1ULL << CAP_DIAG_DEBUG_UART);
#endif
#if FEATURE_FACTORY_TEST
    if (mfg_factory_available()) {  /* flag 未锁 或 本会话售后解锁 */
        b |= (1ULL << CAP_MFG_FACTORY_TEST) | (1ULL << CAP_MFG_PROVISION);
    }
#endif
    return b;
}
```

---

## 9. Provisioning 分区与 manufacturing flag（D4）

### 9.1 新增 Flash 分区（阶段 2 已完成占位）

`flash_layout.h` 从末尾向前占用 6 个扇区（24KB）：配置 B/A、Fault A/B、KeyStats、**Provisioning（阶段 2 新增占位）**。新增扇区排在 KeyStats 之前（更靠近应用、地址更低，`end-24K`），既有 5 个区域相对 Flash 末尾的偏移**保持不变**：

```
（从 Flash 末尾向前）
  最后第 1 扇区 : 配置 B        end-4K
  最后第 2 扇区 : 配置 A        end-8K
  最后第 3 扇区 : Fault B       end-12K
  最后第 4 扇区 : Fault A       end-16K（默认活动区）
  最后第 5 扇区 : 按键统计       end-20K
  最后第 6 扇区 : Provisioning  end-24K（阶段 2 占位，制造数据 + manufacturing flag）
```

阶段 2 已落地：新增 `FLASH_REGION_PROVISIONING` 枚举（位于 KEY_STATS 之后、COUNT 之前）、`FLASH_LAYOUT_PROVISIONING_SECTORS=1`、`FLASH_LAYOUT_TOTAL_SECTORS` 由 5 增至 6、`flash_layout_provisioning_offset()/addr()` 便捷函数，host mock（`mock_flash.c`，24KB/6 扇区）同步。**阶段 2 仅占位，不写入数据**；`provisioning_data_t`、状态机与制造命令在阶段 3 实现。Provisioning 写入次数极少，采用单区 + magic + CRC（如需更高可靠可后续扩 A/B）。

### 9.2 provisioning 数据结构（拟）

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;            /* PROVISIONING_MAGIC */
    uint16_t version;
    uint8_t  mfg_state;        /* 0=UNPROVISIONED 产线/工程态, 1=SHIPPED 已出厂锁定 */
    uint8_t  serial[24];       /* 序列号 */
    uint8_t  hardware_rev[8];  /* 硬件版本 */
    uint8_t  batch[16];        /* 批次/日期码 */
    uint16_t joy_center_x;     /* 出厂摇杆中心校准初值 */
    uint16_t joy_center_y;
    uint8_t  factory_result;   /* 出厂自检结果摘要 bitmask */
    uint16_t unlock_count;     /* 售后解锁次数（审计） */
    uint32_t crc32;
} provisioning_data_t;
```

> 该结构独立于 `device_config_t`，不影响消费者配置迁移；出厂校准在首次启动时作为 config 校准字段的初值。

### 9.3 状态机与出厂/售后流程

- **工程态（dev / 全新未烧录）**：`mfg_state=UNPROVISIONED`，长按 Fn 可进入工厂自检，制造命令可用。
- **产线流程（FactoryTool）**：烧录 release → 一键自检（PASS/FAIL）→ 写序列号/硬件版本/校准 → `MFG_SHIP_LOCK(0x25)` 置 `SHIPPED`。此后长按 Fn 不再进入工厂模式，`CAP_MFG_*` 位撤销。
- **售后流程**：`MFG_AFTERSALES_UNLOCK(0x26)` 凭受控凭证临时解锁**本会话**（不持久改回 UNPROVISIONED，避免永久降级），`unlock_count++` 并写入 Fault/审计记录；断电后恢复 SHIPPED。
- **售后解锁凭证方案**：阶段 4 细化，候选——挑战码 + 授权工具签名应答（复用现有 HMAC-SHA256 / 固件签名密钥体系），或售后专用口令 + 解锁审计。阶段 3 先预留命令与 `unlock_count` 字段。

---

## 10. 上位机实现设计（D1）

### 10.1 本地角色模式

- Core 层新增本地角色枚举与设置（持久化到本地配置，默认 `Consumer`）：

```csharp
public enum LocalRole { Consumer = 0, Developer = 1 }
```

- 页面显隐统一判定（在导航构建处，WPF 与 Avalonia 共用同一 ViewModel 判定）：

```csharp
bool ShouldShow(PageDescriptor page) =>
    LocalRole.Allows(page.RequiredRole) &&
    DeviceCapabilities.Has(page.RequiredCap);
```

- **标准模式**：仅 §4.1 中消费者页面。
- **开发者模式**：设置页隐藏入口（如连续点击版本号 7 次 / 快捷键 / 口令）开启，额外显示 KeyTest、Stats、PerfMonitor、ErrorLog、Diagnostics；模式状态本地持久化。
- 设备能力位为 0 时，即使处于开发者模式，对应页面仍隐藏/置灰（`role && cap` 双条件）。

### 10.2 FactoryTool（阶段 3 新建）

- 在 `pc_tool/` 下新增内部工具项目（建议 `pc_tool/factory/HidConfigTool.Factory`，Avalonia 或命令行 + 极简 UI），引用现有 `Core` / `Hid` / `ViewModels` 中可复用部分。
- 功能：连接设备 → 一键自检并汇总 PASS/FAIL → 写序列号/硬件版本/出厂校准 → 导出良率记录（CSV）→ 出厂锁定。
- **不接入消费者发布流水线**（`publish.ps1` 单独 target、CI 单独产物、标 internal），ConsumerApp 中不出现任何工厂入口。
- ConsumerApp 在阶段 3 移除任何残留的产线/原始制造入口。

---

## 11. 分阶段实施计划

> 原则：先文档冻结（本文件）→ 搭无行为变化的框架 → 协议闸门 + 界面分层（单固件即可见效）→ 双 Profile → 拆工厂工具 → 出厂/售后闭环。每阶段独立分支、独立 PR、全量回归后再进入下一阶段。

### 阶段 0：CMake feature 框架（默认全开，行为不变）— ✅ 已完成（2026-10，JYW）

- 新增 6 个 `option(FEATURE_*)`，默认 ON；生成一个配置头（如 `features.h`）。
- 在各模块入口加 `#if FEATURE_*` 包裹，但所有开关默认 1，**产物行为与现状完全一致**。
- CI 矩阵增加一个"显式全开"构建验证。
- 验收：固件 0 warning、现有单元测试全过、烧录功能与现状一致。

### 阶段 1：协议分级 + 固件闸门 + 上位机角色模式（关键阶段）

> **状态：✅ 已完成（2026-10-02，JYW）。** 固件全开 / 全关两套构建均 0 warning；host 单元测试 222 项全过（新增 `test_capability` 6 项、`test_command_gate` 10 项）。上位机 Core / ViewModels / WPF / Avalonia 四项目构建 0 错误，单元测试 171 项全过（新增本地角色闸门 6 项）。固件与上位机的能力位（`CAP_COUNT=26`）、协议版本（v0.2）、`device_flags` 偏移、错误码 11/12 已逐位核对一致。本阶段仍全开编译，源文件级裁剪与 release/dev 双目标留待阶段 2。

- 固件：
  - 扩展能力枚举至 `CAP_COUNT=26`，位图改为 `capability_bitmap_runtime()` 按 FEATURE + mfg 状态生成。
  - 新增错误码 `ERR_DIAG_LOCKED(11)`、`ERR_MFG_LOCKED(12)`。
  - 为命令表增加 `level / required_cap`，在分发与 `get_report_cb` 加闸门（与 `config_lock.c` 整合）。
  - Device Info 增加 `device_flags`，协议次版本升 v0.2，支持短报告兼容解析。
  - **此阶段仍全开编译**，仅当能力位/标志不满足时返回 LOCKED/NOT_SUPPORTED。
- 上位机：
  - `CapabilityId` 枚举同步扩展，能力来源改为位图优先。
  - 新增 `LocalRole` 与开发者模式隐藏入口；导航按 `role && cap` 显隐；`HidErrorMapper` 映射新错误码。
- 验收（单固件即可见界面分层）：标准模式无诊断页、开发者模式出现；raw HID 直发被闸门拦截时返回正确错误码；老固件/新工具与新固件/老工具组合不崩溃。

### 阶段 2：固件双 Profile 落地 — ✅ 已完成（2026-10-05，JYW）

> **状态：✅ 已完成。** CMake 双目标 `firmware_dev` / `firmware_release` 构建 0 warning 并链接通过（矩阵与占用见 §8.2）；host 单元测试三套全绿——dev 全开主 runner **222** 项、紧凑 Fault runner **9** 项（`build_compact_tests.bat`）、release 制造门控 runner **5** 项（`build_mfg_lock_tests.bat`，以 release 裁剪矩阵单独编译）；CI 已改为双构建 + release-profile host 测试 + 双产物上传。

- **双目标与裁剪**：`add_firmware_variant(TARGET PROFILE)` + 共享 `file(GLOB)` 源集，两目标源文件完全相同，仅 `target_compile_definitions` 不同；裁剪采用**整翻译单元条件编译 + 头文件 `static inline` 空桩**（`perf_monitor.h` / `key_stats.h` 关时类型保留、函数空实现）。release 关闭任务级 perf、KeyStats、UART、raw 诊断并置 `LOG_LEVEL=0`，`FEATURE_FACTORY_TEST=1` 保留。
- **Fault 双后端（D3 / 决项 ②定稿）**：持久化拆为公共框架 `fault.c` + 两个互斥后端——dev 用 `fault_full.c`（252B/条文本，A/B），release 用 `fault_compact.c`（16B 紧凑记录 + 磨损均衡），详见 §11.1。
- **过渡制造门控（D4 的阶段 2 形态）**：新增 `FEATURE_MFG_DEFAULT_LOCKED`，release 上电即锁定——`capability_mfg_factory_available()` 返回 false、撤销 `CAP_MFG_FACTORY_TEST(24)` / `CAP_MFG_PROVISION(25)`、Device Info 置 `DEVICE_FLAG_MFG_SHIPPED_LOCKED`；dev 全开。物理"长按 Fn"本地自检（`main.c`，开机 500ms 窗口内 Fn 按下占比 >80% 进入，**不走 HID**）在两版均保留，不受门控影响。掉电保持的正式 manufacturing flag 与售后重开随阶段 3/4（§9）。
- **Provisioning 分区占位**：新增第 6 扇区（`end-24K`），既有 5 区偏移不变，详见 §9.1。
- **ABI 红线固化**：`config.h` 增加 `_Static_assert` 锁定 `sizeof(device_config_t)==1510` 及 magic/version/perf_thresholds/crc32 偏移；`fault.h` 锁定 `sizeof(fault_log_entry_t)==252` 且 ≤ `FAULT_LOG_ENTRY_SIZE(256)`。旧配置 v1/v3/v5 → latest 迁移测试持续通过。
- **验证**：两版 Flash/RAM 占用见 §8.2；`device_config_t` 大小/偏移两版一致；旧配置可读（迁移测试覆盖）。DFU 升级/回滚路径在阶段 4 随出厂闭环统一验证（Bootloader 本阶段未改动）。

#### 11.1 紧凑 Fault 后端定稿（release，`fault_compact.c`；决项 ②）

**16 字节 packed 记录**（CRC16 覆盖前 14 字节，`_Static_assert` 锁 16B）：

| 字段 | 类型 | 字节 | 说明 |
|------|------|:---:|------|
| code | uint16 | 2 | 事件码（BOOT_POWERON=0x0000、BOOT_WATCHDOG=0x0001、HARD_FAULT=0x0010、WATCHDOG=0x0011、ASSERT=0x0012、FLASH_ERROR=0x0013、IPC_TIMEOUT=0x0014、CRC_ERROR=0x0015、USB_ERROR=0x0016、GENERIC_WARN/ERROR/FATAL=0x001E/0x001F/0x002F 等） |
| level | uint8 | 1 | INFO/WARN/ERROR/FATAL |
| module_id | uint8 | 1 | 0–15：SYSTEM/FLASH/WATCHDOG/CORE1/USB/CONFIG/KEYPAD/OPTICAL/JOYSTICK/ENCODER/FACTORY/MACRO/POWER/SCHEDULER/IPC/OTHER |
| boot_count | uint32 | 4 | 启动序号，每次开机 +1（取历史最大值 +1） |
| timestamp_ms | uint32 | 4 | 相对本次启动毫秒 |
| occurrences | uint16 | 2 | 同类合并次数（1=未合并） |
| crc16 | uint16 | 2 | CRC-16（前 14 字节） |

**组织与寿命（磨损均衡，用户高优先项）**：

- 每扇区 4096/16 = **256 条**（全量文本档仅 16 条/扇区，容量 **16 倍**）；A/B 双扇区，写满一扇区时把最近 **128** 条搬到另一扇区。
- **按页攒批**：RAM 中攒满一页（256B = 16 条）才整页编程一次；FATAL / 异常启动用 PAD 记录（`code=0xFFFE`，占槽不计有效）立即封页落盘。
- **同类合并**：仅合并当前 RAM 页最后一条，指纹 = (code, module_id, level, boot_count)，BOOT 记录不合并；命中则 `occurrences++` 并重算 CRC，不新增槽、不重复编程；已编程页永不改写（Flash 只能 1→0）。
- **读取窗口**：`fault_get_logs` 反向取最近 **32** 条有效事件，还原为对外 `fault_log_entry_t`（补 magic、查模块名表、拼 `"<desc> (xN) boot#<boot>"`），**HID 报告结构与全量档完全一致**。
- **磨损寿命对比**：全量档每条事件编程 1 页、每写 16 条触发 1 次扇区擦除；紧凑档每 16 条编程 1 页、每写 256 条才擦 1 次扇区。**页编程与扇区擦除频率均降至约 1/16**（按 A/B 轮换计，单个固定扇区的擦除间隔由约每 32 条事件延长到约每 512 条）。
- **掉电取舍（有意设计）**：正常上电的 BOOT 与非致命记录先在 RAM 攒页，正常掉电最多丢失最后未满一页（≤15 条非致命记录）；看门狗/异常复位与 FATAL 立即封页落盘，**崩溃现场不丢**。扫描按页进行，遇 blank/CRC 坏页整页判废并从下一物理页续写，绝不二次编程已写页。

**全量档历史寻址 bug 修复**：`fault_log_entry_t` 为 packed，实际 **sizeof = 252**（旧注释误标 256；header 20B + module 32B + msg 200B），但每条记录按 `FAULT_LOG_ENTRY_SIZE = FLASH_PAGE_SIZE = 256` 定址（页内 4B 填充）。旧代码误用结构体数组下标（252 步长）读取，导致从第 1 条起逐条错位。已统一改为 `base + i*FAULT_LOG_ENTRY_SIZE`（256 步长）扫描/搬移/读取，并加 `_Static_assert` 锁定，结构 ABI（252B）与 HID 报告不变。

### 阶段 3：拆工厂工具

- 新增 Manufacturing 命令 `0x20–0x26` 与 Feature Report 段，受 `CAP_MFG_*` + flag 门控。
- 新建 FactoryTool：一键自检、序列号/校准烧录、良率 CSV、出厂锁定。
- ConsumerApp 移除产线入口；发布脚本与 CI 区分对外/内部产物。
- 验收：FactoryTool 可独立完成"烧录→自检→烧信息→锁定"全流程；锁定后长按 Fn 与制造命令均被拒。

### 阶段 4：出厂 / 售后闭环与文档分离

- 实现售后受控解锁（挑战-应答，复用 HMAC/签名体系）与解锁审计。
- 完善 provisioning 状态机、断电/重锁边界、Fault 日志回收流程。
- 文档拆分：`docs/user_manual/`（消费者）与工程/产线手册（FactoryTool 使用、解锁流程、制造命令）分开。
- 在开发环境文档中注明**推荐 OpenOCD 版本号**，避免团队因版本差异重复遇到 Flash 大小识别问题。

---

## 12. 总体验收标准

1. `firmware_release` 中任务级 perf、KeyStats、UART、raw 诊断**不启动/不响应**；`firmware_dev` 全部可用。　**【阶段 2 已验证：整翻译单元裁剪 + 门控 runner 5 项】**
2. 对 release 直接发送 raw HID 诊断命令/报告读取，分别返回 `NOT_SUPPORTED(9)` 或 `LOCKED(11/12)`，**无任何绕过路径**（含 `GET_REPORT`）。　**【阶段 2 部分达成：诊断/制造位编译期裁撤后返回 NOT_SUPPORTED(9)，已由门控测试覆盖；持久 SHIPPED 态返回 MFG_LOCKED(12) 与售后解锁路径随阶段 3/4】**
3. ConsumerApp 标准模式看不到任何诊断/工厂页；开发者模式可见诊断页且受设备能力位约束；FactoryTool 独立运行且不含消费者配置入口。
4. release/dev 的 `device_config_t` **大小与字段偏移完全一致**，旧版本配置在两版固件上均可正确读取与迁移。　**【阶段 2 已验证：`_Static_assert` 锁 1510B/偏移 + v1/v3/v5 迁移测试】**
5. 新增 Provisioning 分区后，配置/Fault/KeyStats 既有 Flash 偏移不变；manufacturing flag 掉电保持，出厂锁定后工厂入口不可用、售后可受控临时解锁并留审计。　**【阶段 2：分区占位落地、既有偏移不变已验证；掉电保持 flag / 售后解锁属阶段 3/4】**
6. CI 同时构建 release 与 dev，固件 0 warning、固件与上位机现有测试全部通过，并补充闸门/角色/分区的新增单元测试。　**【阶段 2 已验证：CI 双构建 + release-profile host 测试（compact 9 + mfg-lock 5）+ 双产物上传】**
7. DFU 升级（含中断、失败回滚）在 release 与 dev 上均正常，Bootloader 与签名链不受裁剪影响。
8. FactoryTool 能独立完成产线全流程并导出良率数据，且不随消费者 Release 发布。

---

## 13. 风险与回滚

| 风险 | 影响 | 缓解 |
|------|------|------|
| 裁剪导致 Flash 布局/配置错位 | 高 | 诊断数据独立分区；`device_config_t` 只增不删；✅ 阶段 2 已加入 `sizeof`/偏移 `_Static_assert`（1510B）与旧配置迁移测试，Provisioning 新扇区不移动既有 5 区 |
| 只在上位机藏按钮、固件无闸门 | 高 | 必须在命令分发与 `get_report_cb` 双侧加固件闸门，作为阶段 1 强制评审项 |
| 双 Profile 增加 CI/发布/版本对齐成本 | 中 | 同一源码 + 编译矩阵；CI 双构建；版本/能力由固件位图宣告 |
| 协议 v0.2 与老工具/老设备互通 | 中 | 版本协商 + 短报告兼容解析 + 位图读取失败回退 |
| release 移除工厂测试后售后无法诊断 | 中 | D4 已决定保留编译 + flag 门控 + 售后解锁；Fault 精简记录保留 |
| 工厂测试入口被消费者误触发 | 低 | 出厂锁定后撤销 `CAP_MFG_*` 并屏蔽长按 Fn；解锁需凭证且留痕 |

每个阶段使用独立分支（`feature/role-split-stageN-*`），独立 PR，全量回归通过后合并；出现严重问题可直接 revert 对应阶段合并提交，各阶段不交叉改动配置结构与 Flash 布局。

---

## 14. 待决问题（进入对应阶段前确认）

1. 售后解锁凭证采用"挑战-应答签名"还是"售后口令 + 审计"（阶段 4 定，建议前者）。
2. ~~release 中 Fault 精简版的字段范围与条数上限~~ **✅ 已定稿（阶段 2，见 §11.1）**：16B packed 记录、单扇区 256 条、A/B 搬最近 128、读取窗口最近 32、全局环形 + 同类合并、按页攒批磨损均衡；release 保留精简档，dev 保留全量文本档。
3. FactoryTool 形态：极简 GUI 还是 CLI + 测试夹具脚本（阶段 3 定，取决于产线是否需要 HIL）。
4. Manufacturing 命令/报告段最终编号（`0x20–0x2F` / `0x1A–0x1F` 为拟用值，阶段 3 在 `config_hid.h` 定稿并同步 `ProtocolConstants.cs`）。

---

*本文件为功能角色分离的冻结决策与实施依据，后续相关 PR 必须与本文一致；如需变更决策，须先更新本文档版本并记录变更原因。*
