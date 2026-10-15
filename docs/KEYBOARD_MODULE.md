# 键盘模块设计文档

## 一、概述

本项目使用 **74HC165 SPI 扩展芯片**读取 64 键状态，采用双核架构：
- **Core1**：硬件扫描 + 去抖 + 共享数据写入
- **Core0**：按键处理 + HID 报告生成 + 事件回调

支持两种键盘报告模式：
- **6KRO 模式**（默认）：标准键盘报告，最多 6 键 + 修饰键，兼容性最好
- **NKRO 模式**：位图报告，最多 256 键无冲，需要主机支持

## 二、硬件架构

### 2.1 74HC165 SPI 扩展

```
RP2350 SPI0 ──→ 74HC165 #1 ──→ 74HC165 #2 ──→ ... ──→ 74HC165 #8
                (键0-7)        (键8-15)                  (键56-63)
```

- 8 片 74HC165 级联，读取 64 键
- SPI 时钟读取，低电平有效（bit=0 表示按下）
- 扫描周期：5ms（200Hz）

### 2.2 双核架构

```
┌─────────────────────────────────────────────────────┐
│  Core1 (硬件扫描)                                    │
│  ┌──────────┐    ┌──────────┐    ┌──────────────┐ │
│  │ SPI 读取 │───→│  去抖    │───→│ 共享数据写入 │ │
│  │ 74HC165  │    │ (逐位)   │    │ (自旋锁保护) │ │
│  └──────────┘    └──────────┘    └──────────────┘ │
└──────────────────────────┬──────────────────────────┘
                           │ 共享数据
┌──────────────────────────▼──────────────────────────┐
│  Core0 (业务处理)                                    │
│  ┌──────────┐    ┌──────────┐    ┌──────────────┐ │
│  │ 事件回调 │───→│ 按键映射 │───→│ HID 报告生成 │ │
│  │ (按下/   │    │ (标准层/ │    │ (6KRO/NKRO)  │ │
│  │  释放)   │    │  Fn层)   │    │              │ │
│  └──────────┘    └──────────┘    └──────────────┘ │
└─────────────────────────────────────────────────────┘
```

## 三、去抖算法

### 3.1 逐位独立计数器

每个按键有独立的去抖计数器，多键同时按下时已稳定键不会被其他键的抖动重置。

```c
typedef struct {
    uint64_t last_raw;       /* 上次原始读数 */
    uint64_t stable_state;   /* 稳定状态输出 */
    uint8_t  count[64];      /* 每键独立消抖计数器 */
    uint8_t  debounce_threshold; /* 消抖阈值(采样次数) */
} debounce_64key_t;
```

### 3.2 去抖流程

1. 读取原始按键状态
2. 与上次原始状态比较，找出变化的位
3. 变化的位：计数器重置为 1
4. 未变化但未达阈值的位：计数器累加
5. 达到阈值的位：更新稳定状态

### 3.3 去抖时间计算

```
实际去抖时间 = 阈值 × 扫描周期
```

| 阈值 | 去抖时间（5ms扫描） | 说明 |
|------|---------------------|------|
| 1 | 5ms | 最小，响应最快，可能误触发 |
| 2 | 10ms | 默认，平衡响应和稳定性 |
| 3 | 15ms | 更稳定，响应稍慢 |
| 4 | 20ms | 最稳定，响应最慢 |

### 3.4 运行时配置

去抖阈值可运行时修改，通过共享数据传递给 Core1：

```c
/* Core0 调用：设置去抖阈值（采样次数） */
void shared_hw_set_keypad_debounce_threshold(uint8_t threshold);

/* Core1 调用：检查并应用阈值更新 */
bool shared_hw_check_and_clear_debounce_updated(void);
```

## 四、按键事件回调机制

### 4.1 事件类型

```c
typedef enum {
    KEYBOARD_EVENT_PRESS = 0,   /* 按键按下（上升沿） */
    KEYBOARD_EVENT_RELEASE,      /* 按键释放（下降沿） */
    KEYBOARD_EVENT_HOLD          /* 按键保持（持续按下） */
} keyboard_event_type_t;
```

### 4.2 事件结构体

```c
typedef struct {
    uint8_t key_index;              /* 键索引（0-63） */
    keyboard_event_type_t type;     /* 事件类型 */
    uint64_t timestamp_ms;          /* 事件时间戳 */
    uint64_t current_state;         /* 当前完整按键状态位图 */
} keyboard_event_t;
```

### 4.3 回调注册

```c
/* 注册回调，返回回调 ID（>=0 成功） */
int keyboard_event_register(keyboard_event_callback_t callback, void *user_data);

/* 注销回调 */
bool keyboard_event_unregister(int callback_id);
```

### 4.4 使用示例

```c
/* 定义回调函数 */
void on_key_event(const keyboard_event_t *event, void *user_data)
{
    switch (event->type) {
        case KEYBOARD_EVENT_PRESS:
            printf("键 %d 按下\n", event->key_index);
            break;
        case KEYBOARD_EVENT_RELEASE:
            printf("键 %d 释放\n", event->key_index);
            break;
        case KEYBOARD_EVENT_HOLD:
            /* 持续按下，可选处理 */
            break;
    }
}

/* 注册回调 */
int cb_id = keyboard_event_register(on_key_event, NULL);
```

### 4.5 辅助接口

```c
/* 获取当前按键状态位图（bit=0 表示按下） */
uint64_t keyboard_event_get_state(void);

/* 获取指定键是否按下 */
bool keyboard_event_is_pressed(uint8_t key_index);

/* 获取当前按下的键数量 */
uint8_t keyboard_event_get_pressed_count(void);
```

## 五、NKRO 模式

### 5.1 报告格式

**6KRO 报告**（Report ID 1）：
```
[Report ID(1)] [Modifier(1)] [Reserved(1)] [Key1(1)] [Key2(1)] ... [Key6(1)] = 9 字节
```

**NKRO 报告**（Report ID 22）：
```
[Report ID(1)] [Modifier(1)] [Reserved(1)] [KeyBitmap(32)] = 35 字节
```

KeyBitmap：每个 bit 代表一个 HID 键码（0-255），bit=1 表示按下。

### 5.2 HID 报告描述符

NKRO 报告描述符包含：
- 修饰键输入（8 个 bit，对应左右 Ctrl/Shift/Alt/GUI）
- 保留字节（1 字节，常量）
- 键码位图（256 个 bit = 32 字节，覆盖 HID 键码 0-255）

### 5.3 模式切换

```c
/* 设置 NKRO 模式 */
void keyboard_set_nkro_enabled(bool enabled);

/* 获取 NKRO 模式状态 */
bool keyboard_get_nkro_enabled(void);
```

- `false`（默认）：只发送 6KRO 报告
- `true`：同时发送 6KRO 和 NKRO 报告

> **注意**：同时发送两种报告是为了兼容性。某些主机可能只识别其中一种。未来可根据主机能力选择只发送一种。

### 5.4 NKRO 位图生成

遍历 64 个物理键，查找对应的 HID 键码（根据 Fn 状态选择标准层或 Fn 层），设置位图中对应的 bit。

跳过：
- Fn 键本身
- 无效键码（0x00, 0xFF）
- 修饰键（0xE0-0xE7，已在 modifier 字节中处理）

## 六、幽灵键消除

由于使用 74HC165 并行输入扩展芯片，**不存在传统行列矩阵的幽灵键问题**。每个键独立连接到 74HC165 的输入引脚，读取时直接获取每个键的状态，无需行列扫描。

如果未来改用 GPIO 行列矩阵，需要：
1. 每个键串联二极管（防止电流回流）
2. 软件检测幽灵键组合（3 键同时按下时的虚假第 4 键）

## 七、相关文件

| 文件 | 说明 |
|------|------|
| `include/device/keypad_spi.h` | SPI 键盘驱动接口 |
| `src/device/keypad_spi.c` | SPI 键盘驱动实现 |
| `include/middleware/debounce.h` | 去抖模块接口 |
| `src/middleware/debounce.c` | 去抖模块实现 |
| `include/middleware/keyboard_event.h` | 按键事件回调接口 |
| `src/middleware/keyboard_event.c` | 按键事件回调实现 |
| `include/middleware/shared_hw_data.h` | 双核共享数据接口 |
| `src/middleware/shared_hw_data.c` | 双核共享数据实现 |
| `src/app/core1_scanner.c` | Core1 硬件扫描 |
| `src/app/main.c` | Core0 按键处理 + HID 报告 |
| `include/protocol/usb_descriptors.h` | USB 描述符（Report ID 定义） |
| `src/protocol/usb_descriptors.c` | USB 描述符（HID 报告描述符） |

## 八、测试验证

### 8.1 去抖测试

1. 快速连按 100 次，验证无漏键、无误触发
2. 按住一个键不放，验证稳定输出
3. 多键同时按下，验证每个键独立去抖

### 8.2 NKRO 测试

1. 启用 NKRO 模式
2. 同时按下 10 个以上键，验证所有键都能被主机识别
3. 切换回 6KRO 模式，验证最多 6 键

### 8.3 事件回调测试

1. 注册按键事件回调
2. 按下/释放键，验证回调正确触发
3. 注销回调，验证不再触发

### 8.4 去抖时间配置测试

1. 设置去抖阈值为 1（5ms），验证响应最快
2. 设置去抖阈值为 4（20ms），验证最稳定
3. 运行时修改阈值，验证 Core1 正确应用
