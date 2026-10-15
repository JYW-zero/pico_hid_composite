# USB 描述符合规性检查工具

## 概述

`check_usb_descriptors.py` 是一个用于自动检查 USB 描述符合规性的 Python 脚本。它可以解析 `firmware/src/protocol/usb_descriptors.c` 中的 USB 描述符，并验证其是否符合 USB 规范和最佳实践。

## 功能特性

### 检查项目

| 类别 | 检查内容 |
|-----|---------|
| **设备描述符** | VID/PID、USB 版本、固件版本、字符串索引、配置数 |
| **HID 报表描述符** | 描述符长度、Report ID 唯一性、集合结构匹配 |
| **配置描述符** | 接口数、端点配置、端点地址唯一性 |
| **字符串描述符** | 语言 ID、厂商字符串、产品字符串、序列号 |
| **VID/PID 合规性** | 保留 VID 检测、Raspberry Pi VID 警告、pid.codes VID 验证 |

### 输出格式

- **文本报告**：人类可读的检查报告，包含错误、警告和信息
- **JSON 格式**：机器可读的 JSON 输出，便于 CI 集成和后续处理

### 运行模式

- **常规模式**：错误导致失败，警告不影响退出码
- **严格模式**（`--strict`）：错误和警告都导致失败

## 安装要求

- Python 3.8+
- 无第三方依赖（仅使用标准库）

## 使用方法

### 基本用法

```bash
cd firmware/scripts
python check_usb_descriptors.py
```

默认检查 `../src/protocol/usb_descriptors.c`。

### 指定文件路径

```bash
python check_usb_descriptors.py --file /path/to/usb_descriptors.c
```

### JSON 输出

```bash
python check_usb_descriptors.py --json
```

输出 JSON 格式的检查结果，便于程序解析。

### 严格模式

```bash
python check_usb_descriptors.py --strict
```

严格模式下，警告也会导致非零退出码。

### 保存报告到文件

```bash
# 文本报告
python check_usb_descriptors.py > report.txt

# JSON 报告
python check_usb_descriptors.py --json > report.json
```

## 命令行参数

| 参数 | 说明 | 默认值 |
|-----|------|-------|
| `--file`, `-f` | usb_descriptors.c 文件路径 | `../src/protocol/usb_descriptors.c` |
| `--json` | 以 JSON 格式输出 | 关闭 |
| `--strict` | 严格模式（警告也失败） | 关闭 |
| `--help`, `-h` | 显示帮助信息 | - |

## 退出码

| 退出码 | 说明 |
|-------|------|
| 0 | 检查通过（无错误，常规模式下允许有警告） |
| 1 | 检查失败（存在错误，或严格模式下存在警告） |

## 输出示例

### 文本报告

```
======================================================================
  USB 描述符合规性检查报告
======================================================================
  文件: /path/to/usb_descriptors.c
  模式: 常规
----------------------------------------------------------------------

  【设备描述符】
    ℹ️  [设备描述符] USB 2.0 全速设备
    ℹ️  [设备描述符] 固件版本: v4.0 (bcdDevice=0x0400)
    ℹ️  [设备描述符] 厂商字符串索引: 1
    ℹ️  [设备描述符] 产品字符串索引: 2
    ℹ️  [设备描述符] 序列号字符串索引: 3

  【HID 报表描述符】
    ℹ️  [HID 报表描述符] desc_hid_report: 49 字节, 5 个 Report ID
    ℹ️  [HID 报表描述符] desc_hid_report: Report IDs = [1, 2, 3, 4, 22]
    ℹ️  [HID 报表描述符] desc_hid_config_report: 278 字节, 18 个 Report ID

  【配置描述符】
    ℹ️  [配置描述符] HID 接口数: 2
    ℹ️  [配置描述符] EPNUM_HID: 0x81 (EP1 IN)
    ℹ️  [配置描述符] EPNUM_HID_CONFIG: 0x82 (EP2 IN)

  【字符串描述符】
    ℹ️  [字符串描述符] 语言 ID: 0x0409 (英语 - 美国)
    ⚠️  [字符串描述符] 厂商字符串为占位符: "PicoHID"
       💡 建议: 建议替换为实际品牌名称
    ⚠️  [字符串描述符] 产品字符串为占位符: "Pico HID Composite Device"
       💡 建议: 建议替换为实际产品名称
    ℹ️  [字符串描述符] 序列号: 使用芯片唯一 ID (动态生成)

  【VID/PID】
    ℹ️  [VID/PID] VID 0x1209 是 pid.codes VID (开源项目可用)

======================================================================
  汇总
======================================================================
  ❌ 错误: 0
  ⚠️  警告: 2
  ℹ️  信息: 15

  ✅ 检查通过
======================================================================
```

### JSON 输出

```json
{
  "file": "/path/to/usb_descriptors.c",
  "strict": false,
  "summary": {
    "errors": 0,
    "warnings": 2,
    "info": 15,
    "total": 17
  },
  "device_descriptor": {
    "vid": 4617,
    "pid": 16388,
    "bcd_usb": 512,
    "bcd_device": 1024,
    "iManufacturer": 1,
    "iProduct": 2,
    "iSerialNumber": 3,
    "bNumConfigurations": 1
  },
  "hid_reports": [
    {
      "name": "desc_hid_report",
      "length": 49,
      "report_ids": [1, 2, 3, 4, 22]
    }
  ],
  "results": [
    {
      "level": "info",
      "category": "设备描述符",
      "message": "USB 2.0 全速设备",
      "suggestion": null
    }
  ]
}
```

## CI 集成

### GitHub Actions

在 `.github/workflows/ci.yml` 中添加以下步骤：

```yaml
  # USB 描述符合规性检查
  - name: Check USB descriptors
    run: |
      cd firmware/scripts
      python check_usb_descriptors.py --file ../src/protocol/usb_descriptors.c
```

### 完整示例

```yaml
name: CI

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]

jobs:
  usb-descriptor-check:
    name: USB Descriptor Check
    runs-on: ubuntu-latest

    steps:
    - name: Checkout repository
      uses: actions/checkout@v4

    - name: Setup Python
      uses: actions/setup-python@v5
      with:
        python-version: '3.11'

    - name: Check USB descriptors
      run: |
        cd firmware/scripts
        python check_usb_descriptors.py --file ../src/protocol/usb_descriptors.c

    - name: Check USB descriptors (strict)
      run: |
        cd firmware/scripts
        python check_usb_descriptors.py --file ../src/protocol/usb_descriptors.c --strict
      continue-on-error: true  # 严格模式仅警告，不阻止合并
```

### 生成 JSON 报告并上传

```yaml
    - name: Generate JSON report
      run: |
        cd firmware/scripts
        python check_usb_descriptors.py --file ../src/protocol/usb_descriptors.c --json > usb-descriptor-report.json

    - name: Upload report
      uses: actions/upload-artifact@v4
      with:
        name: usb-descriptor-report
        path: firmware/scripts/usb-descriptor-report.json
```

## 检查规则详解

### 设备描述符检查

| 规则 | 级别 | 说明 |
|-----|------|------|
| VID/PID 未定义 | 错误 | 必须定义 USB_VID 和 USB_PID |
| USB 版本过低 | 警告 | bcdUSB < 0x0110 时警告 |
| 字符串索引为 0 | 警告 | iManufacturer/iProduct/iSerialNumber 为 0 表示未设置 |
| 配置数为 0 | 错误 | bNumConfigurations 不能为 0 |

### HID 报表描述符检查

| 规则 | 级别 | 说明 |
|-----|------|------|
| 描述符长度为 0 | 错误 | 描述符不能为空 |
| 描述符过短 | 警告 | 长度 < 10 字节时警告 |
| 重复的 Report ID | 错误 | 同一描述符中 Report ID 必须唯一 |
| Report ID 为 0 | 警告 | Report ID 0 不推荐使用 |
| Report ID > 255 | 错误 | Report ID 必须在 1-255 范围内 |
| Collection 不匹配 | 错误 | Collection (0xA1) 和 End Collection (0xC0) 数量必须相等 |

### 配置描述符检查

| 规则 | 级别 | 说明 |
|-----|------|------|
| 未找到 HID 接口 | 错误 | 至少需要一个 HID 接口 |
| 端点号为 0 | 错误 | 端点 0 是控制端点，不能用于数据传输 |
| 端点号过大 | 警告 | 全速设备最多 16 个端点 |
| 重复的端点地址 | 错误 | 端点地址不能重复 |

### 字符串描述符检查

| 规则 | 级别 | 说明 |
|-----|------|------|
| 未设置语言 ID | 警告 | 应设置支持的语言 ID |
| 厂商字符串为占位符 | 警告 | "PicoHID"、"Your Company" 等占位符应替换 |
| 产品字符串为占位符 | 警告 | "Pico HID Composite Device" 等占位符应替换 |
| 未设置序列号 | 警告 | 建议设置序列号或使用芯片唯一 ID |

### VID/PID 合规性检查

| 规则 | 级别 | 说明 |
|-----|------|------|
| 保留/测试 VID | 错误 | 0x0000、0xFFFF、0x1234、0xDEAD、0xBEEF 等 |
| Raspberry Pi VID | 警告 | 0x2E8A 是 Raspberry Pi 官方 VID，商业产品不应使用 |
| pid.codes VID | 信息 | 0x1209 是 pid.codes VID，开源项目可用 |
| PID 为 0 | 错误 | PID 不能为 0x0000 |
| PID 为 0xFFFF | 警告 | 0xFFFF 是测试值，不建议用于生产 |

## 扩展指南

### 添加新的检查规则

1. 在 `USBChecker` 类中添加新的检查方法
2. 在 `check_all()` 方法中调用新方法
3. 使用 `add_error()`、`add_warning()`、`add_info()` 添加检查结果

示例：

```python
def check_my_rule(self):
    """检查我的规则"""
    category = "我的检查"
    
    if some_condition:
        self.add_error(category, "发现错误", "修复建议")
    elif another_condition:
        self.add_warning(category, "发现警告")
    else:
        self.add_info(category, "检查通过")
```

### 添加新的宏常量映射

在 `_parse_byte_array` 方法的 `report_id_macros` 字典中添加新的映射：

```python
report_id_macros = {
    "REPORT_ID_KEYBOARD": 1,
    "REPORT_ID_MOUSE": 2,
    # 添加新的映射
    "REPORT_ID_MY_CUSTOM": 99,
}
```

## 常见问题

### Q: 脚本无法解析 TinyUSB 宏定义的描述符怎么办？

A: 脚本会尝试展开常见的 TinyUSB HID 报表描述符宏（如 `TUD_HID_REPORT_DESC_KEYBOARD`），提取其中的 Report ID。对于复杂的宏，可能无法完全展开，但脚本会记录其存在并继续检查其他部分。

### Q: 如何忽略某些警告？

A: 目前脚本不支持忽略特定警告。如果某些警告是预期的，可以在 CI 中使用常规模式（非严格模式），这样警告不会导致构建失败。

### Q: 脚本支持哪些 Python 版本？

A: 脚本使用 Python 标准库，支持 Python 3.8 及以上版本。

### Q: 如何在本地运行检查？

A: 确保已安装 Python 3.8+，然后运行：

```bash
cd firmware/scripts
python check_usb_descriptors.py
```

## 相关文档

- [USB 规范](https://www.usb.org/documents)
- [HID 规范](https://www.usb.org/hid)
- [TinyUSB 文档](https://docs.tinyusb.org/)
- [pid.codes](https://pid.codes/)
- [CI/CD 流水线说明](../../docs/CI-CD.md)
