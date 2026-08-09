# HID 协议文档

## 📋 概述

本文档详细描述了 Pico HID Composite 设备的 USB HID 协议规范，包括 Report ID 分配、数据格式和通信流程。

---

## 🔌 USB 设备信息

| 项目 | 值 | 说明 |
|------|-----|------|
| **VID** | 0xCafe | Vendor ID |
| **PID** | 0x4004 | Product ID（固定，不随接口数量变化） |
| **BCD** | 0x0200 | USB 2.0 |
| **设备类** | 0x00 | 接口级定义 |
| **最大包长（EP0）** | 64 字节 | 控制端点 |
| **配置数** | 1 | |
| **接口数** | 2 | 标准 HID + 配置 HID |

---

## 📡 HID 接口

设备有两个 HID 接口：

### 接口 0：标准 HID（Interface 0）

| 项目 | 值 |
|------|-----|
| **接口号** | 0 |
| **协议** | None |
| **端点** | EP1 IN (0x81) |
| **端点大小** | 64 字节 |
| **轮询间隔** | 5 ms |
| **Report ID** | 1-4 |

**功能**：标准 HID 设备（键盘、鼠标、多媒体、游戏手柄）

---

### 接口 1：配置 HID（Interface 1）

| 项目 | 值 |
|------|-----|
| **接口号** | 1 |
| **协议** | None |
| **端点** | EP2 IN (0x82) |
| **端点大小** | 64 字节 |
| **轮询间隔** | 10 ms |
| **Report ID** | 5-18 |
| **Usage Page** | 0xFF00 (Vendor Defined) |

**功能**：设备配置、状态查询、性能监控、错误日志等

---

## 📋 Report ID 总览

| Report ID | 名称 | 接口 | 方向 | 类型 | 大小 |
|-----------|------|------|------|------|------|
| 1 | Keyboard | 0 | IN | Input | 8 字节 |
| 2 | Mouse | 0 | IN | Input | 5 字节 |
| 3 | Consumer Control | 0 | IN | Input | 2 字节 |
| 4 | Gamepad | 0 | IN | Input | 8 字节 |
| 5 | Config Block 0 | 1 | IN/OUT | Feature | 62 字节 |
| 6 | Device Info | 1 | IN | Feature | 32 字节 |
| 7 | Control | 1 | OUT | Feature | 1 字节 |
| 8 | Config Block 1 | 1 | IN/OUT | Feature | 62 字节 |
| 9 | Config Block 2 | 1 | IN/OUT | Feature | 62 字节 |
| 10 | Key Stats 0 | 1 | IN | Feature | 62 字节 |
| 11 | Key Stats 1 | 1 | IN | Feature | 62 字节 |
| 12 | Key Stats 2 | 1 | IN | Feature | 62 字节 |
| 13 | Key Stats 3 | 1 | IN | Feature | 62 字节 |
| 14 | Macro Config | 1 | IN/OUT | Feature | 62 字节 |
| 15 | Perf System | 1 | IN | Feature | 62 字节 |
| 16 | Perf Task | 1 | IN | Feature | 62 字节 |
| 17 | Fault Info | 1 | IN | Feature | 62 字节 |
| 18 | Fault Log | 1 | IN | Feature | 62 字节 |

---

## 📝 标准 HID 报告（接口 0）

### Report ID 1：Keyboard（键盘）

**标准 Boot Keyboard 协议**

```
字节  0: Report ID = 1
字节  1: Modifier Keys (位图)
         bit 0: Left Ctrl
         bit 1: Left Shift
         bit 2: Left Alt
         bit 3: Left GUI
         bit 4: Right Ctrl
         bit 5: Right Shift
         bit 6: Right Alt
         bit 7: Right GUI
字节  2: Reserved (0)
字节  3: Keycode 1
字节  4: Keycode 2
字节  5: Keycode 3
字节  6: Keycode 4
字节  7: Keycode 5
字节  8: Keycode 6
```

**说明**：
- 支持 6KRO（6 键同时按下）
- 使用标准 USB HID Usage Page 0x07 (Keyboard)
- 符合 USB HID 键盘规范

---

### Report ID 2：Mouse（鼠标）

**标准 5 键鼠标 + 滚轮**

```
字节  0: Report ID = 2
字节  1: Buttons (位图)
         bit 0: 左键
         bit 1: 右键
         bit 2: 中键
         bit 3: 侧键 1 (Back)
         bit 4: 侧键 2 (Forward)
字节  2: X 位移 (有符号 8 位)
字节  3: Y 位移 (有符号 8 位)
字节  4: 滚轮 (有符号 8 位)
```

**说明**：
- X/Y 位移：相对位移，有符号 8 位整数
- 滚轮：正值向上，负值向下
- 符合 USB HID 鼠标规范

---

### Report ID 3：Consumer Control（多媒体控制）

```
字节  0: Report ID = 3
字节  1: Usage LSB
字节  2: Usage MSB
```

**常用 Usage Code**：

| Usage Code | 功能 |
|------------|------|
| 0x00E9 | 音量加 |
| 0x00EA | 音量减 |
| 0x00E2 | 静音 |
| 0x00CD | 播放/暂停 |
| 0x00B6 | 上一曲 |
| 0x00B5 | 下一曲 |
| 0x00B7 | 停止 |
| 0x00B8 | 弹出 |

---

### Report ID 4：Gamepad（游戏手柄）

**标准游戏手柄报告**

```
字节  0: Report ID = 4
字节  1: Buttons 0-7 (位图)
字节  2: Buttons 8-15 (位图)
字节  3: X 轴 (0-255, 中心 128)
字节  4: Y 轴 (0-255, 中心 128)
字节  5: Z 轴 (0-255, 中心 128)
字节  6: Rx 轴 (0-255, 中心 128)
字节  7: Ry 轴 (0-255, 中心 128)
字节  8: Rz 轴 (0-255, 中心 128)
```

---

## ⚙️ 配置 HID 报告（接口 1）

所有配置报告均使用 Feature 报告类型，通过控制端点传输。

### Report ID 5/8/9：Config Block 0/1/2（配置块）

**用途**：分块读写设备配置

| Report ID | 配置块 | 偏移范围 | 大小 |
|-----------|--------|----------|------|
| 5 | Block 0 | 0 - 61 | 62 字节 |
| 8 | Block 1 | 62 - 123 | 62 字节 |
| 9 | Block 2 | 124 - 185 | 62 字节 |

**总配置大小**：186 字节（3 × 62 字节）

**读流程**：
1. 主机发送 Get_Report(Feature, ID=5)
2. 设备返回配置块 0 的数据
3. 重复读取 Block 1、Block 2

**写流程**：
1. 主机发送 Set_Report(Feature, ID=5, data)
2. 设备接收并缓存配置块 0
3. 重复写入 Block 1、Block 2
4. 全部写完后，设备自动保存到 Flash

---

### Report ID 6：Device Info（设备信息）

**用途**：读取设备基本信息

**大小**：32 字节

**数据格式**（待完善）：
- 固件版本字符串
- 硬件版本
- SDK 版本
- 唯一 ID
- 编译时间

---

### Report ID 7：Control（控制命令）

**用途**：发送控制命令

**大小**：1 字节

**命令列表**（待完善）：
- 0x01: 保存配置到 Flash
- 0x02: 恢复默认配置
- 0x03: 重启设备
- 0x04: 进入工厂测试模式
- 0x05: 清除错误日志
- 0x06: 重置按键统计

---

### Report ID 10-13：Key Stats 0-3（按键统计）

**用途**：读取按键使用统计

| Report ID | 统计块 | 按键范围 |
|-----------|--------|----------|
| 10 | Block 0 | 键 0 - 15 |
| 11 | Block 1 | 键 16 - 31 |
| 12 | Block 2 | 键 32 - 47 |
| 13 | Block 3 | 键 48 - 63 |

**每个按键**：4 字节计数器（uint32_t，小端序）

**每块大小**：16 键 × 4 字节 = 64 字节（Report 数据 62 字节，可能需要调整）

---

### Report ID 14：Macro Config（宏配置）

**用途**：读写宏配置

**大小**：62 字节

**宏数量**：8 个
**每个宏动作数**：32 个
**动作类型**：6 种

**数据格式**（待完善）：
- 宏索引
- 动作索引
- 动作类型
- 动作参数

---

### Report ID 15：Perf System（性能监控 - 系统状态）

**用途**：读取系统性能状态

**大小**：62 字节

**数据内容**（待完善）：
- CPU 使用率（%）
- 主循环频率（Hz）
- 10 秒平均 CPU
- 30 秒平均 CPU
- 空闲 RAM
- 已用 Flash
- 运行时间

---

### Report ID 16：Perf Task（性能监控 - 任务统计）

**用途**：读取任务执行统计

**大小**：62 字节

**数据内容**（待完善）：
- 任务数量
- 每个任务的：
  - 执行次数
  - 总执行时间
  - 最大执行时间
  - 超时次数

---

### Report ID 17：Fault Info（错误日志 - 信息）

**用途**：读取错误日志基本信息

**大小**：62 字节

**数据内容**（待完善）：
- 日志总条数
- 未读条数
- 最新错误级别
- 最新错误时间
- 环形缓冲区读写指针

---

### Report ID 18：Fault Log（错误日志 - 读取日志）

**用途**：读取错误日志条目

**大小**：62 字节

**数据内容**（待完善）：
- 日志索引
- 错误级别
- 错误代码
- 发生时间
- 错误描述

---

## 🔄 通信流程

### 读取配置

```
主机                            设备
  |                               |
  |-- Get_Report(Feature, 5) ---->|  请求配置块 0
  |                               |
  |<-- 配置块 0 数据 -------------|  返回 62 字节
  |                               |
  |-- Get_Report(Feature, 8) ---->|  请求配置块 1
  |                               |
  |<-- 配置块 1 数据 -------------|  返回 62 字节
  |                               |
  |-- Get_Report(Feature, 9) ---->|  请求配置块 2
  |                               |
  |<-- 配置块 2 数据 -------------|  返回 62 字节
  |                               |
```

### 写入配置

```
主机                            设备
  |                               |
  |-- Set_Report(Feature, 5) ---->|  写入配置块 0
  |                               |
  |-- Set_Report(Feature, 8) ---->|  写入配置块 1
  |                               |
  |-- Set_Report(Feature, 9) ---->|  写入配置块 2
  |                               |
  |-- Set_Report(Feature, 7, 01)->|  发送保存命令
  |                               |
  |<-- ACK -----------------------|  保存完成
  |                               |
```

---

## 📌 注意事项

1. **Report ID 前缀**：所有 HID 报告的第一个字节都是 Report ID
2. **Feature vs Input**：
   - 标准 HID（键盘、鼠标等）使用 Input 报告，通过中断端点传输
   - 配置接口使用 Feature 报告，通过控制端点传输
3. **大小限制**：
   - 标准 HID 报告：遵循各自规范
   - 配置报告：最大 62 字节（Report ID 占 1 字节，共 63 字节）
4. **字节序**：多字节数值使用小端序（Little Endian）
5. **字符串编码**：字符串使用 UTF-8 编码

---

## 📚 参考资料

- [USB HID 规范](https://www.usb.org/document-library/device-class-definition-hid-111)
- [USB HID Usage Tables](https://www.usb.org/document-library/hid-usage-tables-15)
- [TinyUSB 文档](https://docs.tinyusb.org/)
- [Raspberry Pi Pico SDK](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html)

---

*最后更新：2026-08-09*
*文档版本：v0.1*
