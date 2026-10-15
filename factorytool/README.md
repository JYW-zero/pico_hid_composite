# FactoryTool 原型（产线工装 · 最小闭环）

> 阶段 3 原型。目的是**先验证产线流程能不能跑通、有没有价值**，不在此阶段决定最终是 GUI 产品还是 CLI/夹具脚本。
> 作者：JYW <JYW@outlook.com>

## 它是什么 / 不是什么

**是**：一个能演示完整过站闭环的最小工具——**扫码 → 识别型号 → 写 SN → 跑 1~2 项自检 → 判定 PASS/FAIL → 出 JSON 报告**，设备用 Mock，不需要硬件。

**不是（原型阶段刻意不做）**：完整 CLI/API、HIL/夹具 IO/PLC/TestStand/LabVIEW、多工位、MES 闭环、插件系统。

只依赖 **Python 3.8+ 标准库**（GUI 用内置 Tkinter），`pip install` 什么都不需要。

## 三个“缝”（以后上不上 HIL 都不用重写核心）

1. **核心逻辑独立**：烧录/写号/测试/判定/日志全在 `core/`，GUI 和 CLI 都只是薄壳，调用同一个 `core.workflow.run_test()`。
2. **设备抽象**：`adapters/base.py` 定义 `Device` 接口（connect / identify / write_serial / run_self_test / …）；原型只实现 `MockDevice`。以后接真机或夹具，新增 `adapters/hid.py` / `fixture.py` 实现同一接口即可。
3. **结构化结果**：每步返回 `StepResult`，整站汇总为 `TestResult`，统一序列化为 JSON——GUI、CLI、未来的夹具脚本/MES 读同一份结构。

## 目录

```
factorytool/
  run.py                 # 入口（默认 GUI）
  core/                  # 核心逻辑（不依赖外壳/硬件）
    config.py            #   工装 JSON 配置加载
    sn.py                #   SN 规则校验/生成
    workflow.py          #   过站编排 run_test()
    models.py            #   StepResult / TestResult / DeviceInfo
    errors.py            #   错误码（设备码对齐固件，工具码 100+）
  adapters/
    base.py              # Device 抽象接口 + 制造能力位（对齐固件）
    mock_device.py       # Mock 设备/模拟器（可注入故障）
    factory.py           # 设备工厂（原型只造 Mock，真机在此留缝）
  ui/gui.py              # 极简 Tkinter GUI
  cli/cli.py             # 最小调试 CLI（只有一个 run 子命令）
  configs/model_pico2.json  # 型号 / SN 规则 / 自检步骤
  tests/                 # unittest（32 项，零依赖）
  reports/               # JSON 报告输出目录（运行时生成）
```

## 运行

```bash
cd factorytool

# 极简 GUI（默认）
python run.py
python run.py gui

# CLI：Mock 跑一块“好板”
python run.py run --mock --scan P20000000001

# CLI：注入各类故障，验证判定与报告
python run.py run --mock --scan P20000000001 --fault write_sn
python run.py run --mock --scan P20000000001 --fault mfg_locked
python run.py run --mock --scan P20000000001 --fault selftest:keypad
python run.py run --mock --fault connect          # 连接失败
python run.py run --mock --scan ABC               # SN 不合规
```

CLI 退出码：`0` = PASS，`1` = FAIL，`2` = 配置/设备错误（便于以后脚本判断）。

### 故障注入（Mock）

`--fault`（GUI 下拉同名）：`connect` / `identify` / `wrong_model` / `write_sn` /
`mfg_locked` / `selftest` / `selftest:keypad` / `selftest:sensor`。

## 工装配置（configs/*.json）

描述“某型号怎么过站”：型号名、SN 规则、要跑哪些自检、报告目录、Mock 行为。
原型用 JSON（零依赖）；以后需要 YAML 只在 `core/config.py` 加一个可选 loader。

```json
{
  "model": "PICO2-HID-COMPOSITE",
  "sn_rule": { "prefix": "P2", "length": 12 },
  "scan": { "barcode_is_serial": true },
  "tests": [
    { "test_id": "keypad", "label": "按键矩阵自检", "required": true },
    { "test_id": "sensor", "label": "光学传感器自检", "required": true }
  ],
  "report_dir": "reports"
}
```

## 输出 JSON（结构化结果）

每块板生成 `reports/<SN>_<时间戳>.json`：

```json
{
  "serial": "P20000000001",
  "model": "PICO2-HID-COMPOSITE",
  "overall": "pass",
  "passed": true,
  "total_elapsed_ms": 153,
  "adapter": "mock",
  "device": { "model": "...", "firmware": "0.1.0", "protocol": "0.2", "capabilities": 1073741823 },
  "steps": [
    { "name": "write_sn", "status": "pass", "code": 0, "message": "...", "elapsed_ms": 30, "details": {} }
  ],
  "logs": ["[connect] OK ...", "[judge] 最终判定：PASS"]
}
```

## 测试

```bash
cd factorytool
python -m unittest discover -s tests -v
```

## 与固件 / 上位机协议的对齐

- 设备错误码 `0~12 / 0xFF` 与固件 `config_hid_error_t`、pc_tool `ProtocolConstants`/`HidErrorMapper` 一致（如 `6=FLASH_WRITE`、`9=NOT_SUPPORTED`、`12=MFG_LOCKED`）；工具流程级码从 `100` 起，互不冲突。
- 制造能力位 `CAP_MFG_FACTORY_TEST=24`、`CAP_MFG_PROVISION=25`，出厂锁定标志 `DEVICE_FLAG_MFG_SHIPPED_LOCKED=1<<0`，与固件 capability_bitmap / device_flags 对齐。
- 制造命令段拟用 `0x20–0x2F`、Feature Report 段 `0x1A–0x1F`（当前空闲），**接入真实设备前在固件 `config_hid.h` 定稿并同步 pc_tool `ProtocolConstants.cs`**。

## 接真实设备时（后续，不在原型内）

1. 在 `adapters/` 新增 `hid.py`，实现 `Device` 接口，移植 pc_tool 已有的 HID/协议层；`factory.build_device(..., use_mock=False)` 返回它。
2. 核心编排、JSON 结果、GUI/CLI 无需改动。
3. 若上自动化/HIL，再加夹具适配器与 CLI/API，同一核心可被脚本直接 `import` 调用。

## 什么时候再决定“GUI 还是 CLI”

等原型跑完、拿到以下信息再定：产线上不上 HIL？操作员是否需要手动点？夹具脚本谁来写（Python / LabVIEW / TestStand）？MES 是否自动交互？
届时：人工工位 → GUI 产品化；自动化/HIL → CLI + API + 夹具脚本；混合 → 同一核心、两个外壳。
