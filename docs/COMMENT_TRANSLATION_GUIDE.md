# 代码注释翻译规范

## 1. 目标
将项目中所有中文注释统一翻译为英文，符合国际开源项目标准，便于团队协作和代码维护。

## 2. 翻译原则

### 2.1 准确性
- 技术术语必须准确，使用行业标准英文表达
- 保留代码逻辑不变，只翻译注释文字
- 不确定的术语查阅相关技术文档

### 2.2 简洁性
- 注释应简洁明了，避免冗长
- 使用短语而非完整句子（如 `/* Initialize module */` 而非 `/* This function initializes the module */`）
- 函数注释使用 Doxygen 格式（`@brief`, `@param`, `@return`）

### 2.3 一致性
- 相同概念使用相同翻译（如"初始化"统一为 `Initialize`）
- 术语表见附录 A

## 3. 翻译范围

### 3.1 必须翻译
- 文件头注释（模块名称、功能描述）
- 函数注释（`@brief`, `@param`, `@return`）
- 常量、宏定义注释
- 结构体、枚举注释
- 复杂逻辑的行内注释

### 3.2 可以保留
- 调试用的临时注释（建议删除）
- 代码中的字符串字面量（如 `printf("中文")`，需要单独讨论是否国际化）

## 4. 翻译进度计划

### 阶段一：核心中间件层（middleware）- 本轮完成
- [x] perf_monitor.h
- [ ] perf_monitor.c
- [ ] fault.h
- [ ] fault.c
- [ ] watchdog.h
- [ ] watchdog.c
- [ ] scheduler.h
- [ ] scheduler.c
- [ ] crc32.h/c
- [ ] debounce.h/c
- [ ] flash_service.h/c
- [ ] shared_hw_data.h/c
- [ ] power_manager.h/c
- [ ] keyboard_event.h/c
- [ ] encoder_processor.h/c
- [ ] joystick_processor.h/c
- [ ] ipc.h
- [ ] sha256.h/c（已完成，新建时即为英文）
- [ ] hmac_sha256.h/c（已完成，新建时即为英文）

### 阶段二：协议层（protocol）
- config_hid.h/c
- config_validator.h/c
- config_block_receiver.h/c
- config_lock.h/c
- config_hid_helpers.h/c
- command_confirm.h/c
- usb_descriptors.h/c

### 阶段三：应用层（app）
- main.c
- core1_scanner.h/c
- keyboard_processor.h/c
- keymap.h/c
- macro.h/c
- led_manager.h/c
- key_stats.h/c
- factory_test.h/c
- usb_callbacks.h/c
- encoder_report_queue.h/c
- log.h

### 阶段四：设备驱动层（device）
- keypad_spi.h/c
- optical_sensor.h/c
- encoder.h/c
- joystick.h/c
- dfu/dfu_metadata.h/c

### 阶段五：板级支持层（board）
- board.h/c
- config.h/c
- flash_layout.h/c
- pins.h

### 阶段六：上位机（pc_tool）
- Core 层
- Hid 层
- ViewModels 层
- Views 层（WPF + Avalonia）

## 5. 翻译工具与方法

### 5.1 推荐工具
- 手动翻译：确保准确性，适合核心文件
- Python 脚本辅助：批量替换常见术语，适合大量文件
- IDE 翻译插件：如 CodeGPT、Comment Translator（需人工审核）

### 5.2 翻译流程
1. 读取文件，识别中文注释
2. 参考术语表进行翻译
3. 写回文件，保持代码格式不变
4. 编译验证，确保没有语法错误
5. 运行单元测试，确保功能正常

## 6. 质量检查

### 6.1 编译验证
- 翻译后必须通过编译
- 检查是否有因注释格式错误导致的编译警告

### 6.2 功能验证
- 运行单元测试，确保所有测试通过
- 关键模块进行硬件在环测试

### 6.3 术语一致性检查
- 使用脚本检查相同中文术语是否翻译一致
- 定期更新术语表

## 附录 A：常用术语翻译表

| 中文 | 英文 | 备注 |
|------|------|------|
| 初始化 | Initialize | 函数名用 init，注释用 Initialize |
| 反初始化 | Deinitialize | |
| 读取 | Read | |
| 写入 | Write | |
| 擦除 | Erase | |
| 配置 | Configuration | |
| 设置 | Setting | |
| 参数 | Parameter | |
| 状态 | State / Status | 状态机用 State，状态查询用 Status |
| 事件 | Event | |
| 中断 | Interrupt | |
| 定时器 | Timer | |
| 看门狗 | Watchdog | |
| 复位 | Reset | |
| 启动 | Boot / Startup | |
| 休眠 | Sleep | |
| 唤醒 | Wakeup | |
| 低功耗 | Low Power | |
| 调试 | Debug | |
| 测试 | Test | |
| 验证 | Verify / Validation | 签名验证用 Verify，需求验证用 Validation |
| 校验 | Checksum / Verification | CRC校验用 Checksum |
| 签名 | Signature | |
| 加密 | Encryption | |
| 解密 | Decryption | |
| 认证 | Authentication | |
| 授权 | Authorization | |
| 安全 | Security | |
| 错误 | Error | |
| 警告 | Warning | |
| 信息 | Info / Information | 日志级别用 Info |
| 日志 | Log | |
| 打印 | Print | |
| 输出 | Output | |
| 输入 | Input | |
| 缓冲区 | Buffer | |
| 队列 | Queue | |
| 链表 | Linked List | |
| 哈希表 | Hash Table | |
| 信号量 | Semaphore | |
| 互斥锁 | Mutex | |
| 自旋锁 | Spinlock | |
| 临界区 | Critical Section | |
| 原子操作 | Atomic Operation | |
| 内存屏障 | Memory Barrier | |
| 超时 | Timeout | |
| 溢出 | Overflow | |
| 越界 | Out of Bounds | |
| 空指针 | Null Pointer | |
| 内存泄漏 | Memory Leak | |
| 缓冲区溢出 | Buffer Overflow | |
| 死锁 | Deadlock | |
| 竞态条件 | Race Condition | |
| 数据竞争 | Data Race | |
| 并发 | Concurrency | |
| 并行 | Parallelism | |
| 多线程 | Multithreading | |
| 多任务 | Multitasking | |
| 调度器 | Scheduler | |
| 任务 | Task | |
| 线程 | Thread | |
| 回调 | Callback | |
| 句柄 | Handle | |
| 描述符 | Descriptor | |
| 上下文 | Context | |
| 平台 | Platform | |
| 架构 | Architecture | |
| 分层 | Layering | |
| 抽象 | Abstraction | |
| 封装 | Encapsulation | |
| 模块化 | Modularization | |
| 解耦 | Decoupling | |
| 内聚 | Cohesion | |
| 耦合 | Coupling | |
| 依赖 | Dependency | |
| 性能 | Performance | |
| 效率 | Efficiency | |
| 可靠性 | Reliability | |
| 可维护性 | Maintainability | |
| 可扩展性 | Scalability | |
| 可移植性 | Portability | |
| 可读性 | Readability | |
| 可测试性 | Testability | |
| 安全性 | Security | |
| 稳定性 | Stability | |
| 兼容性 | Compatibility | |
| 一致性 | Consistency | |
| 规范性 | Standard Compliance | |
| 重构 | Refactoring | |
| 优化 | Optimization | |
| 微秒 | Microseconds | 代码中用 us |
| 毫秒 | Milliseconds | 代码中用 ms |
| 秒 | Seconds | 代码中用 s |
| 字节 | Bytes | 代码中用 B |
| 位 | Bits | 代码中用 b |
| 指针 | Pointer | |
| 数组 | Array | |
| 结构体 | Structure | |
| 枚举 | Enumeration | |
| 函数 | Function | |
| 变量 | Variable | |
| 常量 | Constant | |
| 宏 | Macro | |
| 定义 | Definition | |
| 声明 | Declaration | |
| 实现 | Implementation | |
| 接口 | Interface | |
| 模块 | Module | |
| 驱动 | Driver | |
| 协议 | Protocol | |
| 固件 | Firmware | |
| 软件 | Software | |
| 中间件 | Middleware | |
| 应用层 | Application Layer | |
| 协议栈 | Protocol Stack | |
| 实时操作系统 | RTOS | |
| 裸机 | Bare Metal | |
| 启动加载程序 | Bootloader | |
| 固件升级 | Firmware Update / OTA | |
| 设备固件升级 | DFU | |
| 通用串行总线 | USB | |
| 人机接口设备 | HID | |
| 输入报告 | Input Report | |
| 输出报告 | Output Report | |
| 特征报告 | Feature Report | |
| 报告ID | Report ID | |
| 端点 | Endpoint | |
| 接口 | Interface | |
| 描述符 | Descriptor | |
| 传感器 | Sensor | |
| 执行器 | Actuator | |
| 控制器 | Controller | |
| 处理器 | Processor | |
| 微控制器 | MCU | |
| 外设 | Peripheral | |
| 寄存器 | Register | |
| 时钟 | Clock | |
| 频率 | Frequency | |
| 电压 | Voltage | |
| 电流 | Current | |
| 温度 | Temperature | |
| 功耗 | Power Consumption | |
| 电源管理 | Power Management | |
| 主循环 | Main Loop | |
| 事件循环 | Event Loop | |
| 任务调度 | Task Scheduling | |
| 上下文切换 | Context Switch | |
| 中断延迟 | Interrupt Latency | |
| 响应时间 | Response Time | |
| 吞吐量 | Throughput | |
| 带宽 | Bandwidth | |
| 延迟 | Latency | |
| 实时 | Real-Time | |
| 确定性 | Deterministic | |
| 可预测性 | Predictability | |
| CPU使用率 | CPU Usage | |
| 内存使用率 | Memory Usage | |
| 运行时间 | Uptime | |
| 执行时间 | Execution Time | |
| 执行次数 | Execution Count | |
| 统计 | Statistics | |
| 监控 | Monitoring | |
| 性能监控 | Performance Monitoring | |
| 滑动窗口 | Sliding Window | |
| 平均值 | Average | |
| 最大值 | Maximum | |
| 最小值 | Minimum | |
| 瞬时值 | Instantaneous | |
| 阈值 | Threshold | |
| 告警 | Alarm | |
| 计数 | Count | |
| 重置 | Reset | |
| 开关 | Enable/Disable | |
| 开启 | Enabled | |
| 关闭 | Disabled | |
| 成功 | Success | |
| 失败 | Failure / Failed | |
| 无效 | Invalid | |
| 有效 | Valid | |
| 索引 | Index | |
| 长度 | Length | |
| 大小 | Size | |
| 偏移 | Offset | |
| 地址 | Address | |
| 指针 | Pointer | |
| 句柄 | Handle | |
| 描述符 | Descriptor | |
| 上下文 | Context | |
| 环境 | Environment | |
| 平台 | Platform | |
| 架构 | Architecture | |
| 分层 | Layering | |
| 抽象 | Abstraction | |
| 封装 | Encapsulation | |
| 继承 | Inheritance | |
| 多态 | Polymorphism | |
| 面向对象 | Object-Oriented | |
| 面向过程 | Procedural | |
| 函数式 | Functional | |
| 响应式 | Reactive | |
| 事件驱动 | Event-Driven | |
| 数据驱动 | Data-Driven | |
| 模型驱动 | Model-Driven | |
| 测试驱动 | Test-Driven | |
| 行为驱动 | Behavior-Driven | |
| 持续集成 | Continuous Integration | |
| 持续部署 | Continuous Deployment | |
| 持续交付 | Continuous Delivery | |
| 版本控制 | Version Control | |
| 代码审查 | Code Review | |
| 重构 | Refactoring | |
| 优化 | Optimization | |
| 性能 | Performance | |
| 效率 | Efficiency | |
| 可靠性 | Reliability | |
| 可维护性 | Maintainability | |
| 可扩展性 | Scalability | |
| 可移植性 | Portability | |
| 可读性 | Readability | |
| 可测试性 | Testability | |
| 安全性 | Security | |
| 稳定性 | Stability | |
| 兼容性 | Compatibility | |
| 一致性 | Consistency | |
| 规范性 | Standard Compliance | |
| 模块化 | Modularization | |
| 解耦 | Decoupling | |
| 内聚 | Cohesion | |
| 耦合 | Coupling | |
| 依赖 | Dependency | |
| 引用 | Reference | |
| 包含 | Include | |
| 导入 | Import | |
| 导出 | Export | |
| 命名空间 | Namespace | |
| 作用域 | Scope | |
| 生命周期 | Lifecycle | |
| 所有权 | Ownership | |
| 借用 | Borrowing | |
| 移动 | Move | |
| 拷贝 | Copy | |
| 克隆 | Clone | |
| 比较 | Compare | |
| 相等 | Equal | |
| 不相等 | Not Equal | |
| 大于 | Greater Than | |
| 小于 | Less Than | |
| 大于等于 | Greater Than or Equal | |
| 小于等于 | Less Than or Equal | |
| 等于 | Equal To | |
| 不等于 | Not Equal To | |
| 与 | And | |
| 或 | Or | |
| 非 | Not | |
| 异或 | XOR | |
| 按位与 | Bitwise AND | |
| 按位或 | Bitwise OR | |
| 按位非 | Bitwise NOT | |
| 按位异或 | Bitwise XOR | |
| 左移 | Left Shift | |
| 右移 | Right Shift | |
| 循环左移 | Rotate Left | |
| 循环右移 | Rotate Right | |
| 加法 | Addition | |
| 减法 | Subtraction | |
| 乘法 | Multiplication | |
| 除法 | Division | |
| 取模 | Modulo | |
| 取余 | Remainder | |
| 幂 | Power | |
| 开方 | Square Root | |
| 绝对值 | Absolute Value | |
| 最大值 | Maximum Value | |
| 最小值 | Minimum Value | |
| 平均值 | Average Value | |
| 中位数 | Median | |
| 标准差 | Standard Deviation | |
| 方差 | Variance | |
| 求和 | Sum | |
| 计数 | Count | |
| 排序 | Sort | |
| 搜索 | Search | |
| 查找 | Lookup / Find | |
| 遍历 | Traverse / Iteration | |
| 迭代 | Iteration | |
| 递归 | Recursion | |
| 循环 | Loop | |
| 条件 | Condition | |
| 分支 | Branch | |
| 跳转 | Jump | |
| 返回 | Return | |
| 退出 | Exit | |
| 进入 | Enter | |
| 开始 | Start / Begin | |
| 结束 | End / Finish | |
| 停止 | Stop | |
| 暂停 | Pause | |
| 继续 | Continue | |
| 恢复 | Resume | |
| 挂起 | Suspend | |
| 唤醒 | Wake Up | |
| 阻塞 | Block | |
| 非阻塞 | Non-Blocking | |
| 同步 | Synchronous | |
| 异步 | Asynchronous | |
| 并行 | Parallel | |
| 串行 | Serial | |
| 顺序 | Sequential | |
| 随机 | Random | |
| 伪随机 | Pseudo-Random | |
| 确定性 | Deterministic | |
| 不确定性 | Non-Deterministic | |
| 幂等 | Idempotent | |
| 原子 | Atomic | |
| 可见性 | Visibility | |
| 有序性 | Ordering | |
| 一致性 | Consistency | |
| 持久性 | Durability | |
| 隔离性 | Isolation | |
| 事务 | Transaction | |
| 提交 | Commit | |
| 回滚 | Rollback | |
| 保存点 | Savepoint | |
| 锁 | Lock | |
| 解锁 | Unlock | |
| 加锁 | Acquire Lock | |
| 释放锁 | Release Lock | |
| 死锁 | Deadlock | |
| 活锁 | Livelock | |
| 饥饿 | Starvation | |
| 优先级反转 | Priority Inversion | |
| 优先级继承 | Priority Inheritance | |
| 优先级天花板 | Priority Ceiling | |
| 调度 | Scheduling | |
| 抢占 | Preemption | |
| 协作 | Cooperative | |
| 时间片 | Time Slice / Quantum | |
| 上下文切换 | Context Switch | |
| 中断延迟 | Interrupt Latency | |
| 响应时间 | Response Time | |
| 吞吐量 | Throughput | |
| 带宽 | Bandwidth | |
| 延迟 | Latency | |
| 抖动 | Jitter | |
| 实时 | Real-Time | |
| 硬实时 | Hard Real-Time | |
| 软实时 | Soft Real-Time | |
| 确定性 | Deterministic | |
| 可预测性 | Predictability | |
| 最坏情况执行时间 | WCET | |
| 最好情况执行时间 | BCET | |
| 平均执行时间 | Average Execution Time | |
| CPU使用率 | CPU Usage | |
| 内存使用率 | Memory Usage | |
| 磁盘使用率 | Disk Usage | |
| 网络使用率 | Network Usage | |
| 电源管理 | Power Management | |
| 功耗 | Power Consumption | |
| 能效 | Energy Efficiency | |
| 散热 | Thermal Management | |
| 温度 | Temperature | |
| 电压 | Voltage | |
| 电流 | Current | |
| 频率 | Frequency | |
| 时钟 | Clock | |
| 晶振 | Crystal Oscillator | |
| PLL | PLL | |
| 分频 | Frequency Division | |
| 倍频 | Frequency Multiplication | |
| 校准 | Calibration | |
| 补偿 | Compensation | |
| 校正 | Correction | |
| 调整 | Adjustment | |
| 配置 | Configuration | |
| 设置 | Setting | |
| 参数 | Parameter | |
| 选项 | Option | |
| 标志 | Flag | |
| 位掩码 | Bitmask | |
| 寄存器 | Register | |
| 外设 | Peripheral | |
| 片上外设 | On-Chip Peripheral | |
| 外部设备 | External Device | |
| 传感器 | Sensor | |
| 执行器 | Actuator | |
| 控制器 | Controller | |
| 处理器 | Processor | |
| 微控制器 | MCU | |
| 微处理器 | MPU | |
| 数字信号处理器 | DSP | |
| 现场可编程门阵列 | FPGA | |
| 专用集成电路 | ASIC | |
| 系统级芯片 | SoC | |
| 板级支持包 | BSP | |
| 硬件抽象层 | HAL | |
| 驱动程序 | Device Driver | |
| 固件 | Firmware | |
| 软件 | Software | |
| 中间件 | Middleware | |
| 应用层 | Application Layer | |
| 协议栈 | Protocol Stack | |
| 实时操作系统 | RTOS | |
| 裸机 | Bare Metal | |
| 启动加载程序 | Bootloader | |
| 引导程序 | Bootstrap | |
| 固件升级 | Firmware Update / OTA | |
| 在线升级 | OTA Update | |
| 设备固件升级 | DFU | |
| 通用串行总线 | USB | |
| 人机接口设备 | HID | |
| 大容量存储设备 | MSC | |
| 通信设备类 | CDC | |
| 厂商特定 | Vendor-Specific | |
| 类特定 | Class-Specific | |
| 标准请求 | Standard Request | |
| 类请求 | Class Request | |
| 厂商请求 | Vendor Request | |
| 控制传输 | Control Transfer | |
| 中断传输 | Interrupt Transfer | |
| 批量传输 | Bulk Transfer | |
| 同步传输 | Isochronous Transfer | |
| 端点 | Endpoint | |
| 接口 | Interface | |
| 配置描述符 | Configuration Descriptor | |
| 接口描述符 | Interface Descriptor | |
| 端点描述符 | Endpoint Descriptor | |
| 字符串描述符 | String Descriptor | |
| 设备描述符 | Device Descriptor | |
| 报告描述符 | Report Descriptor | |
| 输入报告 | Input Report | |
| 输出报告 | Output Report | |
| 特征报告 | Feature Report | |
| 报告ID | Report ID | |
| 用法页 | Usage Page | |
| 用法 | Usage | |
| 逻辑最小值 | Logical Minimum | |
| 逻辑最大值 | Logical Maximum | |
| 物理最小值 | Physical Minimum | |
| 物理最大值 | Physical Maximum | |
| 报告大小 | Report Size | |
| 报告计数 | Report Count | |
| 集合 | Collection | |
| 结束集合 | End Collection | |
| 应用集合 | Application Collection | |
| 物理集合 | Physical Collection | |
| 逻辑集合 | Logical Collection | |
| 报告集合 | Report Collection | |
| 按钮 | Button | |
| 键盘 | Keyboard | |
| 鼠标 | Mouse | |
| 游戏杆 | Joystick | |
| 游戏手柄 | Gamepad | |
| 触摸板 | Touchpad | |
| 触摸屏 | Touchscreen | |
| 数字化仪 | Digitizer | |
| 显示器 | Display | |
