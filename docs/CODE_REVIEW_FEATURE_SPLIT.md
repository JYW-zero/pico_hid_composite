# 功能角色分离 —— 功能盘点与代码审查报告

> 审查日期：2026-10-05　审查人：JYW <JYW@outlook.com>
> 审查基线：固件协议 v0.2 / 配置结构 **v8（1510B）**、Pico SDK 2.3.0 / RP2350、上位机 .NET 10、factorytool 原型 0.1.0
> 范围：`firmware/`（90 个 C/H 文件，约 1.45 万行）、`pc_tool/`（177 个 C# 文件，约 2.35 万行，含测试）、`factorytool/`（约 1 千行 Python）
> 方法：只审查不改代码；按“消费者 / 开发者 / 工厂售后”三角色核对功能归属与裁剪，再逐模块做正确性 / 安全 / 简化审查。

---

## ★ 修复回填（2026-10-05，JYW）

本轮已按授权直接修复 **P0 与全部可立即处理的 P2**，并补齐跨端 / 回归测试。**P1 中需产品决策或属阶段 4 的项保持不动**。

| 编号 | 问题 | 状态 | 落地 / 验证 |
|---|---|---|---|
| **P0-2**（审查漏标，本轮补） | 固件 `merge_config` 漏合并 v6/v7 字段，GUI 改 4 档 DPI / LOD / 角度捕捉 / DPI 切换键 / 第 3、4 层键映射，点应用后回退 | ✅ 已修 | `config_validator.c` 补 memcpy/赋值；新增 6 个 host 用例 |
| **P0-1** | 上位机配置 ABI 停在 v7/1492，与固件 v8/1510 不一致 | ✅ 已修 | 常量升 `ConfigVersion=8 / ConfigSize=1510`、CRC@1506、perf 段解析/序列化、v7→v8 迁移；模拟器同步；新增 `ConfigAbiConsistencyTests` 跨端锁偏移 |
| P2-1 | `factory_test.c` sprintf/strcpy 无界 | ✅ 已修 | 25 处改 `snprintf` + `_Static_assert` |
| P2-2 | raw 诊断报告缺 `#if FEATURE_RAW_DIAG` | ✅ 已修 | `config_hid.c` 已包裹 |
| P2-3 | `CMD_CLEAR_FAULT` 缺 `#if FEATURE_FAULT_LOG` | ✅ 已修 | 已包裹 |
| P2-4 | 制造命令保留段 0x20–0x2F 静默丢弃 | ✅ 已修 | `command_gate.c` 显式回 `NOT_SUPPORTED` |
| P2-5 | 1492 / 块数陈旧注释 | ✅ 已修 | `version.h`、`config_hid.h` 改 1510 |
| P2-9 | 1492 / 版本口径散落 | ✅ 已修 | 统一收敛到 `ProtocolConstants` |
| P2-10 | identify 致命失败未短路，出现“型号错却写号 PASS” | ✅ 已修 | 型号不符/识别失败 → write_sn 与自检全 SKIP；扫码失败 → write_sn SKIP |
| P2-11 | 报告文件名秒级时间戳会互相覆盖 | ✅ 已修 | 改毫秒 + 同毫秒序号兜底 |
| P2-12 | CLI `--fault` 无取值校验 | ✅ 已修 | 加 `choices=FAULT_CHOICES`，拼错 exit 2 |
| P2-13 | 错误码未标来源 | ✅ 已修 | `StepResult.code_source = device/tool` |
| **P1-2** | `CAP_DFU_UPGRADE` 恒置位但 bootloader 禁用 | ✅ 已修（严格） | 新增 `FEATURE_DFU_BOOTLOADER` 开关，bit10 严格跟随 bootloader 是否编入；raw `0x04` 挂固件能力闸门；上位机按 bit10 隐藏一键/在线 DFU、BOOTSEL UF2 主推；固件+上位机+测试同步，见 §3 P1-2 |
| P1-3 | 双前端（WPF/Avalonia）+ 两份驱动去留 | 🔁 方向已定，挂 CH582 分支 | 已拍板**跨平台（Win/Linux/mac）**：Avalonia 主线、WPF 退役、驱动统一 HidSharp、CI 多平台；因近期切 CH582、RP2350 仅最小维护，重构随移植分支做，见 §3 P1-3 与 §7 |
| P1-4 | `DeviceService.cs` 上帝类 | ⬜ 未修 | 建议 P0 稳定后渐进抽取 |
| P1-1 | 持久制造状态机（阶段 4） | ⬜ 待形态决策 | FactoryTool 真机化前置 |
| P2-6 / P2-7 / P2-8 | 死页面 / 开发者入口口令 / `CapabilityId` 归位 | ⬜ 未修 | 随前端决策一批处理 |

**最终验证（全绿）**

- 固件：`firmware_dev` / `firmware_release` 双目标 ninja 构建 exit 0（uf2：dev 195072 B / release 176128 B，P1-2 为常量级位图/闸门改动，体积不变）；host 测试 **主套件 230 + 紧凑 Fault 9 + 制造门控 7 = 246，0 失败**。
- 上位机：解决方案 build **0 错误**；`HidConfigTool.Tests` **185/185**（含新增 5 个能力位图用例）；`HidConfigTool.IntegrationTests` **57/57**。
- factorytool：`unittest` **40/40**（P1-2 未改 Python，回归无变化）。

> **2026-10-06（JYW）P1-2 严格修复轮**：DFU 能力位改为由编译开关 `FEATURE_DFU_BOOTLOADER` 严格决定，固件位图 + raw 命令闸门 + 上位机显隐三层收口，BOOTSEL UF2 作为芯片原生能力始终可用；host 主套件 +2（228→230）、制造门控套件 +2（5→7，纳入 `command_gate.c` 编译）、上位机单元 +5（180→185）。同时记录 CH582 移植与跨平台上位机决策（见 §7）。

> **重要定性更正（详见 §2 P0-1 / P0-2）**：实时 HID 保存路径由固件 `config_save()` 强制重算 magic/version/length/seq/CRC 兜底，**不会**因 CRC 偏移不同而直接拒写；1492/1488 的真实破坏面在上位机**配置文件导入 / 导出、云备份、版本迁移与本地校验**，而实时路径“改了不生效”的真凶是 P0-2 的 merge 漏 v6/v7 字段。

---

## 0. 总体结论（先看这里）

1. **减法（裁剪）的骨架是扎实的、方向正确**：编译期 `FEATURE_*` 开关 + 固件命令/报告闸门（`command_gate`）+ 上位机“本地角色 AND 设备能力”双条件显隐，三层都真正接了线，raw HID 绕不过固件闸门。release/dev 双构建、host 回归（246 项）、factorytool（40 项）全绿。
2. **“加法”原有两处阻断级缺口，均已修复（见文首回填表与 §2）**：① 上位机配置链路停在 v7/1492，落后固件 v8/1510——破坏面在配置文件导入导出 / 云备份 / 版本迁移 / 本地校验（实时 HID 路径由固件重算 CRC 兜底，不直接拒写）；② 审查时漏标的固件 `merge_config` 漏合并 v6/v7 字段，这才是 GUI 改配置“点应用后回退”的实时路径真凶。
3. **最大的结构性债是上位机双前端**：WPF 与 Avalonia 两套 UI、约 20 个 Service 各写一份、两份 HID 驱动，同时在解决方案里构建，但 CI 只发布 WPF。**去留已拍板（2026-10-06）：跨平台，Avalonia 主线、WPF 退役、统一 HidSharp**；因近期切 CH582，重构挂移植分支做（见 §3 P1-3、§7）。
4. 制造状态机目前是**编译期常量**（release 上电即锁、dev 恒开），真正的“产线未锁→出厂锁定→售后解锁”持久状态机未做（Provisioning 分区仅占位）。这是 FactoryTool 接真机前的硬前置，属阶段 4，不影响原型。
5. 其余多为局部健壮性 / 一致性 / 简化项（`sprintf` 无界、个别诊断报告缺编译期裁剪、死页面、陈旧注释等），风险低、改动小。

> 关于“先加法还是先减法”：**先补 P0 加法（把消费者配置通路修到 v8），再做减法收敛**。在 P0 未修前谈进一步裁剪没有意义——消费者主功能当前是坏的。

---

## 1. 功能角色总表（加法 / 减法清单）

### 1.1 消费者必需（release 必须保留，PUBLIC / 基础能力位）

| 功能 | 固件 | 上位机页面 | release 状态 |
|---|---|---|---|
| 键盘 64 键 / NKRO、多媒体、手柄 HID 报告 | 设备本体，不受 `FEATURE_*` 控制 | Key / KeyManagement | 保留 ✅ |
| 鼠标 PAW3395、DPI 档位 / 切换 / LOD | 基础能力位 bit1/14 | Mouse | 保留 ✅ |
| 摇杆（死区 / 灵敏度 / 校准 / 实时） | bit2/15 | Joystick | 保留 ✅ |
| 编码器 | bit3 | Encoder | 保留 ✅ |
| 4 层键映射 / Fn | bit4 | KeyManagement | 保留 ✅ |
| 宏播放 / 停止 | bit5 | Macro | 保留 ✅ |
| 配置读 / 写 / 应用 / 恢复出厂 / 解锁 | `CMD_0x01/02/05/0D`，PUBLIC | Device / Presets | 保留，但**上位机 ABI 落后 → 见 P0-1** ❌ |
| 版本 / 能力查询、版本协商 | `CMD_0x10/11/13`，PUBLIC | Device | 保留 ✅ |
| 重启 / 进 BOOTSEL / 固件升级 | `CMD_0x03/04/14`，bit10 | FirmwareUpdate | ✅ 已修：bit10 严格跟随 `FEATURE_DFU_BOOTLOADER`；当前两 profile 均未编入自定义 bootloader → 一键/在线 DFU 隐藏且 raw `0x04` 被拒，**BOOTSEL UF2（0x14，芯片原生）主推、始终可用**，见 P1-2 |
| 精简 Fault 只读（售后 / 稳定性回收） | `FEATURE_FAULT_LOG=1` + 紧凑后端 | ErrorLog（需开发者模式） | 保留 ✅（设计如此） |

**加法结论**：消费者功能在固件侧裁剪正确、没有误伤；缺口集中在“上位机配置 ABI 未跟上 v8”和“固件升级闭环名实不符”。

### 1.2 开发者 / 诊断（dev 全开，release 关闭）

| 功能 | 编译开关（release） | 命令闸门 | 报告闸门 | 上位机 | 结论 |
|---|---|---|---|---|---|
| 任务级性能监控 / 阈值 | `PERF_MONITOR=0` | DIAG + CAP | CAP bit7 | 开发者模式 AND 能力 | 到位 ✅ |
| 按键使用次数统计 | `KEY_STATS=0` | DIAG + CAP | CAP bit9 | （无独立菜单，见 P2-5） | 基本到位 ⚠️ |
| 调试串口 / 详细日志 | `DEBUG_UART=0`、`LOG_LEVEL=0` | — | CAP bit17 | — | 到位 ✅ |
| 实时 raw 状态（KeyState/JoystickState） | `RAW_DIAG=0` | — | CAP bit16 运行时拦 | Diagnostics | **报告处理函数缺编译期裁剪 → 见 P2-2** ⚠️ |
| 清除错误日志（售后写操作） | `FAULT_LOG=1` | DIAG + 会话锁 | — | ErrorLog | 到位（阶段 4 接挑战-应答）✅ |

### 1.3 工厂 / 售后

| 功能 | 现状 | 缺口 |
|---|---|---|
| 工厂自检固件（`factory_test.c`） | release **编入**（`FACTORY_TEST=1`），本地“长按 Fn”物理入口保留 | HID 入口受制造状态门控 ✅ |
| 制造能力位 / 出厂锁定 | release 上电撤销 CAP_MFG_*、上报 `SHIPPED_LOCKED` | **状态是编译期常量，非持久状态机 → P1-1** |
| 写 SN / 校准烧录 / ship lock 命令 | 命令段 `0x20–0x2F`、Feature `0x1A–0x1F` 尚空闲未定义 | 阶段后期定稿，factorytool 已留 `HidDevice` 缝 |
| 售后解锁 | `DIAG_LOCKED/MFG_LOCKED` 错误码、device_flags 已定义 | 凭证方案未定（挑战-应答 vs 口令+审计），阶段 4 |
| FactoryTool | Python 原型（Mock 最小闭环 + 32 测试 + CI） | 真机化依赖 P1-1 与制造命令定稿 |

---

## 2. 阻断级问题（P0，必须先修）

### P0-1　上位机配置 ABI 停在 v7/1492，与固件 v8/1510 不兼容　【✅ 已修复，2026-10-05】

**证据（两端对同一结构的认知不一致）**

固件（v8，已用 `_Static_assert` 锁定，`firmware/include/board/config.h:115-135`）：

| 项 | 固件现值 | 位置 |
|---|---|---|
| `perf_thresholds[8]`（v8 新增，16B） | 偏移 **1488** | `config.h:116` |
| `reserved5[2]` | 偏移 1504 | `config.h:117` |
| `crc32` | 偏移 **1506** | `config.h:118,134` |
| 总大小 / `CONFIG_TOTAL_SIZE` | **1510** | `version.h:64`、`config.h:126` |
| `CONFIG_VERSION` | **8** | `version.h:55` |
| DeviceInfo 实际上报 `config_size` | `sizeof(device_config_t)` = **1510** | `config_hid.c:267` |

上位机（仍 v7）：

| 项 | 上位机现值 | 位置 |
|---|---|---|
| `ConfigOffsets.Crc32` | **1488**（正好压在固件新字段 `perf_thresholds` 头上） | `Models/ConfigOffsets.cs:129` |
| `ConfigOffsets.TotalSize` → `ProtocolConstants.ConfigSize` | **1492** | `ConfigOffsets.cs:132`、`ProtocolConstants.cs:65` |
| `ProtocolConstants.ConfigVersion` | **1（v1）** | `ProtocolConstants.cs:59` |
| 序列化缓冲区 | `new byte[1492]` | `ConfigSerializerService.cs:152` |
| CRC 计算区间 | `Crc32Calculate(data, 0, Crc32=1488)` | `ConfigSerializerService.cs:233` |
| 其它硬编码 1492 | `DeviceService.cs:194`、`DeviceDiagnosticsService.cs:115`、集成测试模拟器 `SimulatedHidDevice.cs:57` | — |

**后果链**

> ⚠️ **定性更正（修复后复核）**：下面第 1、2 点描述的“实时 HID 保存被 CRC 拒 / perf 区被踩”在当前固件上**不会发生**——APPLY 经 `merge_config()` 后由 `config_save()` 强制重算 magic/version/length/seq/CRC（`firmware/src/board/config.c`），不采信上位机下发的 CRC。1492/1488 的真实破坏面是第 3、4 点对应的**上位机本地文件 / 备份 / 迁移 / 本地校验链路**；而实时 HID 路径“改了不生效”的真凶是另立的 **P0-2（merge 漏 v6/v7）**。下述机制分析保留，供理解 ABI 漂移危害。

1. 上位机只构造 1492 字节，并把 CRC 写到偏移 1488；固件在偏移 **1506** 读 CRC、对 0..1505 计算校验 → 上位机写的 CRC 落在固件认为是“性能阈值”的位置，固件读到的 CRC 是残留/0 → **`CONFIG_HID_ERR_CRC_MISMATCH(4)`**，保存被拒。
2. 即便 CRC 侥幸通过，固件的 `perf_thresholds` 会被 CRC 字节踩坏。
3. 反向读取时，上位机按 1492 截断，末尾 18 字节（含真实 CRC）读不到，本地校验也不过，可能误判配置损坏而恢复默认。
4. **集成测试全绿是假象**：模拟器 `SimulatedHidDevice` 也写死 1492，测试只验证了“旧上位机 ↔ 旧模拟器”自洽，根本覆盖不到与 v8 固件的跨端一致性。

**修复清单（建议作为一个独立改动 + 跨端一致性测试）**

- `ProtocolConstants.cs`：`ConfigSize = 1510`、`ConfigVersion = 8`。
- `ConfigOffsets.cs`：新增 `PerfThresholds = 1488`（`ushort[8]`）、`Reserved5 = 1504`、`Crc32 = 1506`；`TotalSize` 随之变 1510；补 v8 版本注释。
- `DeviceConfig` 模型增加 `PerfThresholds`（8 项）字段；`ConfigSerializerService` 序列化 / 反序列化、长度字段（`:195`）同步；全量替换散落的 1492 为单一常量引用。
- `SimulatedHidDevice` 模拟器升到 1510 / CRC@1506。
- `DeviceDiagnosticsService.cs:115` 等显示用 1492 同步。
- **新增一道跨端 ABI 一致性测试**：用一个小脚本 / 单元测试，从固件 `config.h`（或生成的 JSON）抽取 `sizeof` 与关键偏移，断言与 C# `ConfigOffsets` 完全相等；CI 里跑。这是防止再次漂移的根本手段（当前两端各有一份手工偏移，必漂）。
- 旧配置迁移：固件已支持 v1/v3/v5→v8（`config.c:458-461`），上位机导入旧配置文件的迁移路径（`ConfigMigrationService`）补 v7→v8。

> 附带纠正：你此前高优先的“**性能监控阈值持久化**”，**固件侧其实已经做完**——v8 字段 + 默认值（`config.c:152`）+ v7→v8 迁移（`config.c:458`）+ 命令写入（`config_hid.c:1193`）+ 启动加载（`main.c:899-902`，还专门处理了 packed 未对齐访问）。**缺的正是本项的上位机半边**。修 P0-1 即同时闭环该优先项。

### P0-2　固件 merge_config 漏合并 v6/v7 字段 —— GUI 改配置“保存后回退”（审查漏标，本轮补）　【✅ 已修复，2026-10-05】

- **现象（消费者实时路径的真问题）**：上位机改 4 档 DPI（`dpi_slot[4]`）、当前档位、LOD、角度捕捉、DPI 切换键（v6）以及第 3 / 4 层键映射（v7，`layer2_keymap` / `layer3_keymap`），点应用后全部回退到 Flash 旧值。
- **根因**：`config_validator_merge_config()` 只 `memcpy` 前 142 字节并补 v3 五个字段，漏了 macro_data 之后的 v6（偏移 1346+）与 v7（1360+）字段；这些段不在前 142 字节拷贝范围内。
- **修复**：补 `dpi_slot` 的 memcpy 与 v6 标量赋值、`layer2_keymap` / `layer3_keymap` 的 memcpy；v5 摇杆校准（center_x/center_y/calibrated）与 v8 `perf_thresholds` **刻意不合并**（上位机配置块不下发它们，合并零值反而会清掉校准与阈值），并加注释锁定该策略。
- **回归**：新增 6 个 host 用例（v6 档位数组 / 标量合并、v7 两层键映射合并、v5 校准与 v8 阈值保留），固件主测试套件 228 项全绿。

---

## 3. 重要问题（P1）

### P1-1　制造状态是编译期常量，缺持久状态机（FactoryTool 真机化前置）　【⬜ 待形态决策，阶段 4】

- `capability.c:19-30`：`capability_mfg_factory_available()` 在 release 恒 `false`、dev 恒 `true`；`device_flags` 同理（`:66-81`）。两处都留了 `TODO(阶段3)`：读 Provisioning 分区 `mfg_state`。
- 后果：烧入 **release** 固件的真机，产线**无法通过 HID 写 SN**（制造位上电即撤、命令回 `MFG_LOCKED`），只剩本地长按 Fn 自检。FactoryTool 的 `write_serial` 在真机上会一直拿到 12 号错误。
- 建议（阶段 4，出厂闭环）：在已占位的 Provisioning 扇区实现 `mfg_state { FACTORY_OPEN, SHIPPED_LOCKED, SESSION_UNLOCKED }` + 掉电保持；定义制造命令段 0x20–0x2F 与 ship-lock / 售后解锁；定稿后同步 `ProtocolConstants.cs` 与 factorytool 的 `adapters/hid.py`。**原型演示不依赖它，可等形态决策。**

### P1-2　`CAP_DFU_UPGRADE` 能力位恒置位，但 DFU bootloader 临时禁用　【✅ 已修（严格），2026-10-06 JYW】

- **原问题**：`capability.c` 把 `CAP_DFU_UPGRADE(10)` 放进恒置位的基础位图，而 `CMakeLists.txt` 已“临时禁用 DFU/Bootloader”（应用链回 `0x10000000`，自定义 bootloader 不随固件编入）。设备上报“支持 DFU 升级”，上位机升级页把**当前必然失败**的“在线升级 / 本地一键 DFU”（`CMD_ENTER_DFU=0x04` + 专用 BOOTLOADER VID/PID + report 0x30~0x36）放在主位、标“推荐”，反把唯一真实可用的 **BOOTSEL UF2（`CMD_ENTER_BOOTSEL=0x14`，RPI-RP2 盘符拖放）标为“备用”**。能力位与实现不符，属“谎报能力”。
- **修法（拍板：严格修，bit10 接 bootloader 编译开关）**，三层收口：
  1. **编译开关表达事实**：`features.h` 新增 `FEATURE_DFU_BOOTLOADER`（默认 1，注释写明 0/1 语义、与 BOOTSEL 的区别、恢复步骤）；release/dev 两个 profile 当前都显式注入 `=0`。
  2. **固件位图 + 命令双闸门**：`capability.c` 基础位图移除 bit10，改为 `CAP_DFU_BOOTLOADER_BITMAP`（开关为 1 才置 bit10）与运行时位图 OR；`command_gate.c` 把 `CMD_ENTER_DFU(0x04)` 的 `required_cap` 设为 bit10——**未编入时即使 raw HID 直发 0x04 也回 `NOT_SUPPORTED(9)`**，不只靠上位机藏按钮。`CMD_ENTER_BOOTSEL(0x14)` 是 RP2350 芯片原生能力，不占能力位、`required_cap=0`，任何形态始终放行。
  3. **上位机按位图显隐**：升级页 VM 新增 `IsDfuUpgradeAvailable`，订阅 `VersionCheckCompleted`（解决连接事件早于 DeviceInfo 解析的时序），在线升级卡 / 本地 DFU 卡 / 进度卡 / 开始·取消按钮全部绑该位隐藏，BOOTSEL 按钮与说明常驻、无 DFU 时标题由“备用”改“**推荐**”；Dashboard 的 GitHub 更新检查与 DevicePage 升级说明同样加 bit10 守卫。`VersionComparer` 旧协议 v2（位图为 0）的默认推断掩码由 `0xFFFF` 改为**排除 bit10**——DFU 不得凭协议版本推断。Avalonia(desktop) 端本就只有“进入 BOOTSEL”按钮、无一键 DFU 页，不误导，未改。
- **验证**：release 门控套件（`FEATURE_DFU_BOOTLOADER=0`）断言 bit10=0、`0x04→NOT_SUPPORTED`、`0x14→OK`、perf 诊断命令随能力位缺失被拒、消费者 `SAVE_CONFIG→OK`；host 全开套件断言编入时 `0x04/0x14` 均放行；上位机新增 `CapabilityBitmapTests` 5 例（bit10 set/clear、不影响消费者位、legacy v2 不推断 DFU、ParseDeviceInfo 端到端 bit10=0/1）。
- **恢复 bootloader 时**：把对应 profile 的 `FEATURE_DFU_BOOTLOADER` 置 1，并按 `CMakeLists.txt` 末尾注释恢复链接地址 `0x10009000` / linker override / `picotool --offset 0x10009000`，bit10 与命令闸门会自动放行，无需再改逻辑。
- **CH582 注意**：移植后无 RPI-RP2 UF2 盘、非标准 USB DFU，bit10 语义与升级方式需随新芯片重定义（产线 WCH ISP、现场 BLE OTA / USB IAP），见 §7。

### P1-3　上位机双前端（WPF + Avalonia）重复，需定去留（最大结构性债）　【🔁 方向已定：跨平台 / Avalonia 主线，重构挂 CH582 分支】

- `HidConfigTool.slnx` 同时构建 `windows/HidConfigTool.App`（WPF）与 `desktop/HidConfigTool.Desktop`（Avalonia）；CI 只**发布** WPF。
- 重复面：约 20 个 Service 在 `windows/Services/` 与 `desktop/Services/` 各一份（Dialog/Theme/Tray/AutoStart/OSD/Language…）；两份 HID 驱动——WPF 用 Win32 P/Invoke `Drivers/HidDriver.cs`（**806 行**，`App.xaml.cs:77` 注册），Avalonia 用跨平台 `shared/Hid/HidSharpDriver.cs`（327 行）。
- 好处是 ViewModel / Core / 协议已抽到 `shared/`，双端共用，重复主要集中在“平台壳 + 驱动”。
- **拍板结果（2026-10-06，JYW）**：宿主机要 **Windows + Linux/macOS 都支持**，故选定 **Avalonia 为主线、WPF 退役、HID 驱动统一到跨平台 `HidSharpDriver`（327 行，删除 806 行 Win32 驱动）、CI 多平台构建发布**。这与早前“只发 Windows 就砍 Avalonia”的备选相反，以本次拍板为准。
- **时机（重要）**：因已确定**近期就切 CH582、RP2350 不再大改（仅最小维护/止血）**，删 WPF / 统一驱动属于上位机架构大重构，若现在在 RP2350 线上做，紧接着移植 CH582 又要重写 USB/BLE 描述符、制造命令与升级路径，会重复返工。故 **P1-3 整体挂到 CH582 移植分支，与驱动 / 升级重写合并实施**；P2-6 死页面、P2-7 开发者口令、P2-8 `CapabilityId` 归位随该批一起做，本轮 RP2350 线不动。
- 在重构落地前，双端任何协议 / 角色改动仍需改两处，是已知的阶段性返工来源。

### P1-4　`DeviceService.cs` 2854 行上帝类　【⬜ 未修，建议 P0 稳定后渐进抽取】

- `shared/HidConfigTool.Core/Services/DeviceService.cs` 单文件 2854 行，混合连接管理、HID 协议收发、配置块传输、宏、DPI、设备信息、命令编码等多职责（`IDeviceService.cs` 也有 349 行）。
- 风险：P0-1 的 ABI 修改会高度集中在这个文件，回归面大；后续制造命令 / MES 接入会继续堆高。
- 建议：渐进抽取，不做大重写——按职责拆出 `ConfigTransferService`（分块收发 / CRC / ABI）、`DeviceInfoService`、`CommandCodec`、保留 `DeviceConnectionService` 管连接；每抽一块用现有 171 个上位机测试兜底。**P0-1 修完后再做**，避免两件大事叠加。

---

## 4. 建议级问题（P2，局部健壮性 / 简化 / 一致性）

### 固件

- ✅ **P2-1　`factory_test.c` 约 20 处 `sprintf`/`strcpy` 写固定 `detail[64]`**（`factory_test.h:46`）。当前中文串 + 有界数字实测未超 64，但**无任何编译期 / 运行期边界保护**，以后加一句更长的文案就溢出（`detail` 在结构体数组里，溢出会踩相邻项）。统一改 `snprintf(detail, sizeof detail, ...)`；cppcheck 也会报 `sprintf`。位置：`factory_test.c:42,62,67,82,...,380`。**【已修：25 处改 `snprintf`，并加 `FACTORY_TEST_DETAIL_SIZE` 宏与 `_Static_assert`】**
- ✅ **P2-2　raw 诊断报告缺编译期裁剪**：`config_hid.c:704-708` 的 `REPORT_ID_KEY_STATE / JOYSTICK_STATE` 处理分支没有 `#if FEATURE_RAW_DIAG` 包裹（PERF/KEY_STATS 都包了）。运行时闸门虽能拦（缺能力位返回空报告），但 release 仍编译这两个处理函数，Flash 没省到、二进制攻击面没收敛。建议与其它诊断报告一致加 `#if FEATURE_RAW_DIAG`。**【已修】**
- ✅ **P2-3　`CMD_CLEAR_FAULT` 分发分支未加 `#if FEATURE_FAULT_LOG`**（`config_hid.c:1448`），与 `RESET_STATS/RESET_PERF/SET_PERF_*` 风格不一。当前靠闸门兜底（能力位缺失即 NOT_SUPPORTED），但一旦某 profile 关掉 FAULT_LOG，可能出现“case 引用已裁剪函数”的链接 / 行为不一致。建议补 `#if`。**【已修】**
- ✅ **P2-4　未实现的制造命令段静默丢弃**：`command_gate.c:42` 仅注释占位，未知命令返回 `OK` 交 switch `default: break`（`:1468`）。raw 发 `0x20–0x2F` 时设备无任何回应，排查困难。建议在制造命令定稿前，对该保留段显式回 `NOT_SUPPORTED`，而不是静默。**【已修：制造段 0x20–0x2F 显式回 `NOT_SUPPORTED(9)`】**
- ✅ **P2-5　陈旧注释 / 口径**：`version.h:66` 仍写“支持 1492 字节、25 块”（值 25 对，1492 应为 1510）；`config_hid.h:163` `CAP_CONFIG_STORAGE` 注释“1492 字节”。改 P0-1 时一并更新，避免再误导。**【已修：两处注释改 1510】**

### 上位机

- ⬜ **P2-6　`KeyTestPage` / `StatsPage`（WPF）注册了页面与 ViewModel（`App.xaml.cs:156,186,205,238`），但主菜单没有对应按钮、`MainViewModel` 也没有 Navigate 方法**——是“减法后残留的死页面”。不构成向消费者泄露（无入口），但应二选一：聚合进 Diagnostics 页，或删除。Avalonia 端 `StatsPage.axaml.cs` 同理核对。**【未修，随 P1-3 前端决策处理】**
- ⬜ **P2-7　开发者模式入口无门槛**：设置页直接一个 CheckBox（WPF `SettingsPage.xaml:650`、Avalonia `SettingsPage.axaml:26`），无隐藏手势 / 口令。安全性由固件闸门兜底（release 设备能力位缺失，开了也大多看不到），但 release 保留了 Fault 能力，普通消费者打开开关就能看到错误日志页。建议加隐藏入口或口令（方案本就写了“隐藏入口/口令”，当前只做了显隐）。**【未修】**
- ⬜ **P2-8　`CapabilityId` 枚举放错文件**：能力枚举 / `CapabilityInfo` 定义在 `Utils/VersionComparer.cs:9-199`（494 行，多职责），应移到 `Models/`（如 `CapabilityInfo.cs` 或并入 `ProtocolConstants.cs`），`VersionComparer` 只做版本比较。**【未修】**
- ✅ **P2-9　1492 / 版本口径散落**：修 P0-1 时务必全量收敛到 `ProtocolConstants` 单一来源；`ConfigOffsets.cs:29` 注释“当前版本 7”、`ProtocolConstants.ConfigVersion=1`、固件 v8 三处口径都不一致。**【已修：`ConfigVersion=8`，散落 1492 全部改为引用 `ProtocolConstants.ConfigSize`，迁移 `CurrentVersion` 接常量使 v2→v8 迁移真正生效】**

### factorytool（原型，整体质量良好，32 测试；以下为打磨项）

- ✅ **P2-10　前置致命失败未短路**：`core/workflow.py` 中 `identify` 型号不符后，仍继续 `write_sn` 和自检，报告会出现“型号错误却写号 PASS”的矛盾记录（只有 `connect` 失败会把后续标 SKIP）。建议对 identify / write_sn 这类前置失败与 connect 一致处理：后续步骤标 SKIP。**【已修：型号不符 / 识别失败 → write_sn 与全部 self_test 标 SKIP 并直接出报告；扫码失败 → write_sn 标 SKIP（identify/self_test 仍跑供维修定位）】**
- ✅ **P2-11　报告文件名时间戳精度到秒**：`<SN>_<YYYYMMDD_HHMMSS>.json`，产线同 SN 同秒复测会**静默覆盖**首份记录。建议加毫秒或短序号。**【已修：时间戳精确到毫秒，同毫秒再追加 `_1/_2` 序号】**
- ✅ **P2-12　CLI `--fault` 无取值校验**（`cli/cli.py`）：拼错（如 `write-sn`）会被当成“无故障”静默跑 PASS；GUI 下拉已有约束。建议 `add_argument("--fault", choices=FAULT_CHOICES)`。**【已修：加 `choices`，拼错即以 exit code 2 报错；新增 `tests/test_cli.py`】**
- ✅ **P2-13　错误码未标来源**：`StepResult.code` 混用设备码（0–12/0xFF）与工具码（100+），虽有 `is_device_code()` 但结果里没带来源；接真机 / MES 时建议在 `details` 加 `code_source: device|tool`。**【已修：`StepResult` 新增 `code_source` 字段（device/tool/空），由 `errors.is_device_code()` 判定并写入 JSON】**

---

## 5. 建议修复顺序

> **执行状态（2026-10-06 更新，JYW）**：顺序 **1 / 2 / 3 已全部完成并回归通过**（另补了审查漏标的 P0-2）；**顺序 5 中的 P1-2 DFU 能力口径已严格修复并回归**（见 §3 P1-2）；顺序 4（P1-3 双前端）方向已定（跨平台 / Avalonia 主线），但因近期切 CH582、RP2350 仅最小维护，**重构与 P2-6/7/8 一并挂到 CH582 移植分支**；6（P1-4 上帝类）等移植 / 接口稳定后靠现有测试兜底渐进拆分；7（P1-1 制造状态机）仍随 FactoryTool 形态决策（阶段 4）。

| 顺序 | 事项 | 级别 | 依赖 / 备注 |
|---|---|---|---|
| 1 | **P0-1 上位机配置 ABI 同步 v8/1510 + 模拟器 + 跨端 ABI 一致性测试** | P0 | 同时闭环“阈值持久化”；消费者配置功能的前提 |
| 2 | P2-1 `snprintf`、P2-2/P2-3 诊断 `#if` 包裹、P2-5 陈旧注释、P2-4 保留段回 NOT_SUPPORTED | P2 | 固件小修，可与 1 同批，走双构建 + host 回归 |
| 3 | P2-10/11/12/13 factorytool 打磨 | P2 | 原型内快速改，补测试 |
| 4 | **P1-3 双前端收敛**：Avalonia 主线 / 退役 WPF / 驱动统一 HidSharp / CI 多平台 | P1 | 🔁 方向已定（跨平台）；随 CH582 移植分支实施，RP2350 线不大改 |
| 5 | **P1-2 DFU 能力位真实性 ✅ 已严格修复**；P2-6 死页面、P2-7 开发者入口、P2-8 枚举归位 | P1/P2 | P1-2 已完成并回归；P2-6/7/8 随顺序 4 的 CH582 分支一批 |
| 6 | P1-4 DeviceService 渐进拆分 | P1 | P0-1 之后，靠现有测试兜底 |
| 7 | P1-1 制造状态机 + 制造命令定稿（阶段 4） | P1 | 等形态决策；FactoryTool 真机化前置 |

---

## 6. 复评 / 验证方式与本轮结果

- **P0-1 / P0-2（本轮已验证）**：
  - 上位机 + 模拟器全量 dotnet：解决方案 build 0 错误；`HidConfigTool.Tests` 180/180、`HidConfigTool.IntegrationTests` 57/57；新增 `ConfigAbiConsistencyTests` 在检测到固件源码树时解析 `config.h` 的 `_Static_assert` 与 `version.h` 的 `CONFIG_VERSION` 做强校验，锁定 1510 / v8 / 偏移 1488 / 1504 / 1506。
  - 固件 host：主套件 228（含 merge v6/v7 与 v5/v8 保留的 6 个新用例）+ 紧凑 Fault 9 + 制造门控 5 = 242，0 失败；dev/release 双目标 ninja 构建 exit 0。
  - ⏳ **仍建议在真机或 dev 固件上补一次“改 4 档 DPI / 第 3 层键映射 → 应用 → 掉电重连 → 读回仍在”的闭环**（host 与模拟器已覆盖逻辑，未替代硬件验证）。
- **固件 P2 项（本轮已验证）**：双构建通过；host 三套 runner 全绿；raw 诊断 / CLEAR_FAULT 已加编译期裁剪，release text 随之小幅下降。
- **factorytool P2 项（本轮已验证）**：`python -m unittest discover -s tests` 40/40，已为短路、毫秒时间戳、choices、code_source 补 8 个用例（新增 `tests/test_cli.py`）。
- 本报告“位置”基于审查时工作区现状（含大量未提交改动）；**修复回填由 JYW 于 2026-10-05 在该工作区直接完成，未提交 / 未 stash 既有改动**。
- **遗留与后续（2026-10-06 更新）**：P1-2 DFU 能力口径已严格修复；P1-3 双前端方向已定（跨平台 / Avalonia 主线），重构连同 P2-6/7/8 与驱动统一挂到 **CH582 移植分支**（见 §7），RP2350 线仅最小维护；P1-4 `DeviceService` 上帝类等 CH582 移植 / 接口稳定后，靠现有 185+57 测试兜底渐进拆分；P1-1 持久制造状态机仍随 FactoryTool 形态决策（阶段 4，等产线演示）。
- ⏳ **真机验证缺口（自动化替代不了）**：① P0-2 的“改 4 档 DPI / 第 3 层键映射 → 应用 → 掉电重连 → 读回仍在”硬件闭环；② P1-2 后在 release 真机确认升级页只见 BOOTSEL、一键/在线 DFU 区隐藏、进 BOOTSEL 能出现 RPI-RP2 盘符。

---

## 7. CH582 移植与上位机跨平台决策备忘（2026-10-06，JYW）

本节冻结四个方向性决策，后续 CH582 移植分支与上位机重构均以此为准，避免在 RP2350 线上做随即被推翻的大改。

### 7.1 四项拍板

| # | 决策点 | 结论 |
|---|---|---|
| ① | **移植时间窗** | **近期就切 CH582（WCH BLE RISC-V）**；RP2350/Pico2 不再做大改，仅做最小维护 / 止血（本轮 P1-2 严格修即属此类止血）。 |
| ② | **DFU 能力位修法** | **严格修**：bit10 接 `FEATURE_DFU_BOOTLOADER` 编译开关，bootloader 没编则位图清 bit10、raw `0x04` 也拒、上位机按 bit10 隐藏一键/在线 DFU；BOOTSEL UF2 始终可用并主推。固件 + 上位机 + 测试已同步（见 §3 P1-2）。 |
| ③ | **上位机宿主机** | **Windows + Linux/macOS 都要**：方向为 **Avalonia 主线、WPF 退役、驱动统一 HidSharp、CI 多平台**。与早前“砍 Avalonia”的备选相反，**以本决策为准**。重构挂 CH582 分支，与驱动 / 升级重写合并做。 |
| ④ | **CH582 消费者现场升级方式** | **暂不定 / 先不讨论**；仅确定**产线走 WCH ISP**。等移植本体推进时再在 BLE OTA 与 USB IAP 间选型。 |

### 7.2 CH582 升级路径事实（移植时用）

- **产线 / 出厂**：WCHISPTool（GUI）或 **WCHISPTool_CMD（Windows / Linux / macOS 跨平台命令行，内置 ISP 库，支持 USB / 串口 ISP）**；按 BOOT 键上电进 ISP，或在 APP highcode 内跳转 ISP 免按键。
- **现场（待定）**：
  - **BLE OTA**：方式一 / 方式二 DFU，需 JumpIAP + IAP + 固定 LIB 分区，手机 APK 写特征值触发；方式二参考分区为 jump iap 4K / app 236K / iap 16K / lib 192K。
  - **USB IAP**：EVT 自带自定义 USB bootloader，思路最接近现有 HID DFU，但需重写。
- **关键差异**：CH582 **无 RPI-RP2 UF2 盘符、非标准 USB DFU**；现有 `DfuUpdater.cs`（专用 BOOTLOADER VID/PID、report 0x30~0x36）与“查找 RPI-RP2 盘符”的 BOOTSEL 逻辑**都不能直接用于 CH582**，升级模块需按新路径重写，能力位语义也要重新定义。
- 参考资料：
  - WCHISPTool_CMD：<https://www.wch.cn/downloads/WCHISPTool_CMD_ZIP.html>
  - WCH ISP / 跳转说明：<https://blog.csdn.net/qq_44836335/article/details/136588672>
  - WCH 论坛 BLE OTA / IAP：<https://www.wch.cn/bbs/thread-91177-1.html> 、<https://wch.cn/bbs/thread-151006-1.html> 、<https://wch.cn/bbs/thread-142344-1.html>

### 7.3 移植分支待办（仅登记，本轮不开工）

- Flash 分区（BLE OTA 的 JumpIAP / IAP / LIB 固定布局）与链接脚本；
- USB / BLE 描述符与复合设备建模；
- 配置协议（v8 / 1510 布局）与制造命令迁移，`device_config_t` 红线（只增不删、偏移不变）在新平台是否沿用需评估；
- 升级路径选型（④）与能力位重定义；
- 上位机 P1-3 重构（Avalonia 主线、统一 HidSharp、CI 多平台）与 P2-6/7/8 随这批落地。
