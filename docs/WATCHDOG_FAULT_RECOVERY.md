# 看门狗与故障恢复机制

## 一、概述

本项目实现了分层看门狗 + 硬件看门狗的双重保护机制，确保设备在出现死循环、任务卡死等异常时能自动复位恢复。故障日志持久化存储在 Flash 中，复位后可通过上位机查看详细的故障原因。

## 二、架构设计

### 2.1 分层逻辑看门狗

采用三层独立监控，每层必须定期"喂狗"报告存活状态：

| 层级 | 名称 | 负责模块 | 喂狗位置 |
|------|------|----------|----------|
| 0 | BOARD | 板级初始化、硬件状态监控 | 主循环、电源管理 |
| 1 | DEVICE | 所有外设扫描任务 | Core1 扫描循环、配置写入 |
| 2 | APP | 主循环、HID 处理 | 主循环末尾 |

**工作原理**：
- `watchdog_tick()` 每 1ms 调用一次（最高优先级任务）
- 检查每层的最后喂狗时间戳，如果超过逻辑超时时间（默认 1 秒），触发复位
- 所有层都存活时，喂硬件看门狗

### 2.2 硬件看门狗

- RP2350 内置硬件看门狗
- 超时时间：逻辑超时的 2 倍（默认 2 秒）
- 作为最后防线，即使 `watchdog_tick()` 本身卡死也能复位
- 调试模式下自动暂停（`pause_on_debug = true`）

### 2.3 故障日志持久化

- 存储位置：Flash 独立扇区（4KB）
- 最大记录数：16 条（每条 256 字节，页对齐）
- 环形缓冲区：写满后保留最近 8 条，擦除后重新写入
- 掉电不丢失：所有 FATAL 级别故障同步写入 Flash

## 三、超时配置

### 3.1 当前配置

| 参数 | 值 | 说明 |
|------|-----|------|
| 逻辑超时 | 1000 ms (1 秒) | 分层看门狗检测阈值 |
| 硬件超时 | 2000 ms (2 秒) | 硬件看门狗复位阈值 |
| 复位前延迟 | 100 ms | 确保 Flash 写入完成 |

### 3.2 修改方法

在 `main.c` 中修改：

```c
// 逻辑超时 1 秒，硬件自动设为 2 秒
watchdog_init(1000);
```

### 3.3 超时选择建议

| 场景 | 推荐逻辑超时 | 说明 |
|------|-------------|------|
| 正常使用 | 1000 ms | 平衡响应速度和误触发率 |
| 调试开发 | 2000-5000 ms | 避免断点调试时误复位 |
| 高可靠性 | 500 ms | 更快的故障恢复，但需确保所有任务能及时喂狗 |

> **注意**：Flash 写入时 Core1 会被暂停约 50-100ms，这是正常行为。DEVICE 层由 Core0 代为喂狗，避免误触发。

## 四、故障记录格式

### 4.1 日志条目结构

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;          // 魔数 0x30544C46
    uint32_t timestamp_ms;   // 时间戳（启动后毫秒数）
    uint8_t  level;          // 故障级别
    uint8_t  module_len;     // 模块名长度
    uint16_t msg_len;        // 消息长度
    uint8_t  reserved[8];    // 保留
    char     module[32];     // 模块名
    char     msg[200];       // 故障消息
} fault_log_entry_t;
```

### 4.2 故障级别

| 级别 | 值 | 处理方式 | Flash 写入 |
|------|-----|----------|-----------|
| INFO | 0 | 仅记录 | 否（只输出串口） |
| WARN | 1 | 记录并提示 | 否（只输出串口） |
| ERROR | 2 | 记录并尝试恢复 | 是（限频 100ms） |
| FATAL | 3 | 记录后复位 | 是（不限频，立即写入） |

### 4.3 看门狗复位日志示例

```
[FAULT][FATAL] watchdog: APP layer timeout - system reset
[FAULT][WARN] watchdog: system recovered from watchdog reset
```

第一条是复位前记录的故障原因，第二条是复位后记录的恢复事件。

## 五、复位原因诊断

### 5.1 Scratch 寄存器

RP2350 的看门狗有 4 个 scratch 寄存器，复位后数据保留。本项目使用 `scratch[0]` 存储最后故障层：

| 值 | 含义 |
|----|------|
| 0 | BOARD 层超时 |
| 1 | DEVICE 层超时 |
| 2 | APP 层超时 |
| 0xFF | 手动复位 |

### 5.2 启动时检测

`watchdog_init()` 会自动检测 `watchdog_caused_reboot()`，如果是看门狗复位，会：
1. 从 scratch 寄存器读取最后故障层
2. 输出串口日志
3. 主循环初始化完成后，记录一条 WARN 级别故障到 Flash

### 5.3 上位机查看

通过上位机的"错误日志"页面可以查看所有历史故障，包括看门狗复位原因。

## 六、关键任务超时检测

### 6.1 性能监控模块

除了分层看门狗，项目还集成了性能监控模块（`perf_monitor`），用于检测单个任务的执行时间：

| 任务 | 默认阈值 | 说明 |
|------|---------|------|
| tud_task | 1000 us | USB 设备栈任务 |
| hid_config | 2000 us | 配置 HID 处理 |
| macro_task | 500 us | 宏执行任务 |
| scheduler | 60000 us | 调度器（写 Flash 时约 50ms） |

超时后记录 WARN 级别日志，但不触发复位。如果任务持续卡死，会导致该层无法喂狗，最终由分层看门狗触发复位。

### 6.2 自定义超时阈值

```c
// 设置 tud_task 超时阈值为 2ms
perf_set_threshold(0, 2000);

// 启用/禁用性能监控
perf_set_enabled(true);
```

## 七、API 参考

### 7.1 看门狗 API

```c
// 初始化看门狗（timeout_ms: 逻辑超时时间）
void watchdog_init(uint32_t timeout_ms);

// 喂狗：某层报告存活
void watchdog_feed_layer(wdg_layer_t layer);

// 巡检：检查所有层是否超时（建议 1-5ms 调用一次）
void watchdog_tick(void);

// 获取最后触发复位的层
wdg_layer_t watchdog_get_last_fault_layer(void);

// 立即触发系统复位
void watchdog_system_reset(void);
```

### 7.2 故障记录 API

```c
// 初始化故障模块（从 Flash 加载历史日志）
void fault_init(void);

// 记录一条故障
void fault_record(fault_level_e level, const char *module, const char *msg);

// 获取总故障计数
uint32_t fault_get_count(void);

// 获取当前日志条数
uint32_t fault_get_log_count(void);

// 读取指定索引的日志
bool fault_get_log(uint32_t index, fault_log_entry_t* out_entry);

// 获取最新的 N 条日志
uint32_t fault_get_latest_logs(fault_log_entry_t* out_entries, uint32_t max_count);

// 清除所有故障记录
void fault_clear(void);
```

## 八、Flash 布局

```
Flash 末尾（从后向前）：
┌─────────────────────────────────┐
│ 配置 B 区 (4KB)                 │
├─────────────────────────────────┤
│ 配置 A 区 (4KB)                 │
├─────────────────────────────────┤
│ 错误日志区 (4KB)  ← 16 条故障  │
├─────────────────────────────────┤
│ 按键统计区 (4KB)                │
└─────────────────────────────────┘
```

## 九、测试验证

### 9.1 看门狗复位测试

**测试方法**：在代码中插入死循环，观察设备是否在 2 秒内自动复位。

```c
// 测试代码（仅用于测试，不要提交）
void test_watchdog_reset(void) {
    printf("Testing watchdog reset...\n");
    while (1) {
        // 死循环，不喂狗
    }
}
```

**预期结果**：
1. 1 秒后逻辑看门狗检测到超时
2. 记录 FATAL 日志到 Flash
3. 100ms 延迟后硬件复位
4. 设备重启，上位机可看到 "APP layer timeout" 故障日志

### 9.2 故障日志持久化测试

1. 触发一次看门狗复位
2. 重新连接设备
3. 上位机读取错误日志
4. 确认能看到复位前的 FATAL 日志和复位后的 WARN 日志

## 十、相关文件

| 文件 | 说明 |
|------|------|
| `firmware/include/middleware/watchdog.h` | 看门狗接口定义 |
| `firmware/src/middleware/watchdog.c` | 看门狗实现 |
| `firmware/include/middleware/fault.h` | 故障处理接口定义 |
| `firmware/src/middleware/fault.c` | 故障处理实现 |
| `firmware/src/app/main.c` | 主循环（看门狗初始化和喂狗） |
| `pc_tool/.../ErrorLogPage.xaml` | 上位机错误日志页面 |
| `pc_tool/.../DeviceService.cs` | 上位机错误日志读取接口 |
