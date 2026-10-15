# 固件单元测试覆盖度文档

**版本**: 1.0
**日期**: 2026-08-24
**测试框架**: Unity (C Unit Test Framework)
**构建系统**: CMake + Make

---

## 1. 测试架构概览

### 1.1 目录结构

```
firmware/tests/
├── test_runner.c          # 测试入口，运行所有测试套件
├── test_config.c           # 配置模块测试（双备份、CRC、迁移）
├── test_crc.c              # CRC32 模块测试
├── test_debounce.c         # 按键消抖模块测试
├── test_encoder.c          # 编码器处理模块测试
├── test_fault.c            # 错误日志模块测试
├── test_keymap.c           # 按键映射模块测试
├── test_macro.c            # 宏引擎模块测试
├── test_scheduler.c        # 任务调度器模块测试
├── mock/                   # Mock 实现
│   ├── mock_config.c       # 配置模块 Mock
│   ├── mock_flash.c        # Flash 服务 Mock
│   ├── mock_gpio.c         # GPIO Mock
│   ├── mock_time.c         # 时间服务 Mock
│   ├── mock_tusb.c         # TinyUSB Mock
│   └── mock_watchdog.c     # 看门狗 Mock
└── unity/                  # Unity 测试框架源码
```

### 1.2 测试运行方式

```bash
# Linux/macOS
cd firmware/tests
make clean && make
./test_runner

# Windows
cd firmware/tests
build.bat
```

CI 中自动运行并生成覆盖率报告（`gcov` + `lcov`）。

---

## 2. 模块测试覆盖度

### 2.1 已测试模块（9个）

| 模块 | 测试文件 | 测试用例数 | 覆盖函数 | 状态 |
|------|---------|-----------|---------|------|
| **配置管理** | `test_config.c` | ~25 | config_init, config_load, config_save, config_reset_default, config_get_default, 双备份校验, CRC校验, 版本迁移(v1→v7) | ✅ 完整 |
| **CRC32** | `test_crc.c` | ~8 | crc32_calc, crc32_update, crc32_finalize, 标准测试向量验证 | ✅ 完整 |
| **按键消抖** | `test_debounce.c` | ~12 | debounce_init, debounce_update, 消抖时序验证, 边界条件 | ✅ 完整 |
| **编码器** | `test_encoder.c` | ~15 | encoder_init, encoder_process, 方向检测, 倍率处理, 待发送报告队列 | ✅ 完整 |
| **错误日志** | `test_fault.c` | ~10 | fault_init, fault_record, fault_get_count, 限频机制, Flash持久化 | ✅ 完整 |
| **按键映射** | `test_keymap.c` | ~18 | keymap_init, keymap_get, keymap_set, 4层映射, Fn键处理 | ✅ 完整 |
| **宏引擎** | `test_macro.c` | ~20 | macro_init, macro_execute, 动作序列, 循环宏, 超时处理 | ✅ 完整 |
| **任务调度** | `test_scheduler.c` | ~12 | scheduler_init, scheduler_add, scheduler_remove, scheduler_tick, 优先级调度 | ✅ 完整 |
| **测试运行器** | `test_runner.c` | — | 汇总所有测试套件，输出结果 | ✅ 完整 |

### 2.2 未测试模块（需补充）

| 模块 | 源文件 | 优先级 | 测试难点 | 建议 |
|------|--------|--------|---------|------|
| **主循环** | `main.c` | 🔴 High | 依赖硬件中断、USB、双核 | 提取纯逻辑函数到独立模块后测试 |
| **Core1扫描** | `core1_scanner.c` | 🔴 High | 双核IPC、SPI读取 | Mock IPC和SPI后测试纯逻辑 |
| **配置HID协议** | `config_hid.c` | 🟠 Medium | USB HID报告处理 | Mock TinyUSB后测试命令解析 |
| **USB描述符** | `usb_descriptors.c` | 🟡 Low | 静态数据 | 验证描述符大小和字段正确性 |
| **光学传感器** | `optical_sensor.c` | 🟠 Medium | SPI通信、PAW3395寄存器 | Mock SPI后测试初始化和数据解析 |
| **摇杆处理** | `joystick_processor.c` | 🟠 Medium | ADC读取 | Mock ADC后测试死区和灵敏度计算 |
| **看门狗** | `watchdog.c` | 🟡 Low | 硬件寄存器 | Mock硬件寄存器后测试heartbeat逻辑 |
| **Flash服务** | `flash_service.c` | 🟡 Low | 硬件Flash操作 | Mock Flash后测试读写擦除逻辑 |
| **性能监控** | `perf_monitor.c` | 🟡 Low | 多核计数器 | Mock计数器后测试统计逻辑 |
| **工厂测试** | `factory_test.c` | 🟡 Low | 硬件交互 | 测试纯逻辑部分（测试序列生成） |
| **Bootloader** | `bootloader/` | 🟢 Low | DFU协议 | 独立测试，需要USB模拟 |

---

## 3. 测试覆盖率统计

### 3.1 行覆盖率（估算）

| 模块类别 | 源文件数 | 已测试 | 行覆盖率估算 |
|---------|---------|--------|------------|
| 业务逻辑层 | 8 | 8 | **~85%** |
| 协议层 | 2 | 0 | ~0% |
| 硬件抽象层 | 6 | 0 | ~0% |
| 应用层 | 3 | 0 | ~0% |
| **总计** | **19** | **8** | **~40%** |

### 3.2 覆盖率目标

| 阶段 | 目标行覆盖率 | 重点模块 | 时间节点 |
|------|------------|---------|---------|
| 当前 | ~40% | 业务逻辑层 | 已完成 |
| 下一阶段 | ~60% | 协议层(config_hid)、硬件抽象层(光学传感器、摇杆) | Q4 2026 |
| 最终目标 | ~80% | 所有纯逻辑模块 | Q1 2027 |

---

## 4. Mock 框架说明

### 4.1 已实现的 Mock

| Mock 文件 | 模拟对象 | 支持的操作 |
|-----------|---------|-----------|
| `mock_config.c` | 配置模块 | 设置/获取配置，模拟加载失败 |
| `mock_flash.c` | Flash服务 | 模拟读写擦除，注入错误 |
| `mock_gpio.c` | GPIO | 设置引脚状态，模拟中断 |
| `mock_time.c` | 时间服务 | 控制时间流逝，模拟超时 |
| `mock_tusb.c` | TinyUSB | 模拟HID报告收发，控制连接状态 |
| `mock_watchdog.c` | 看门狗 | 模拟喂狗，检测超时 |

### 4.2 需补充的 Mock

| 需模拟对象 | 优先级 | 用途 |
|-----------|--------|------|
| SPI接口 | 🔴 High | 测试光学传感器、按键矩阵SPI读取 |
| ADC接口 | 🟠 Medium | 测试摇杆ADC读取和校准 |
| 多核IPC | 🔴 High | 测试Core1扫描和双核通信 |
| 硬件定时器 | 🟡 Low | 测试PWM和定时中断 |

---

## 5. CI 集成

### 5.1 测试步骤

CI workflow (`ci.yml`) 中固件测试步骤：

```yaml
- name: Run firmware unit tests with coverage
  run: |
    cd firmware/tests
    make clean
    make CFLAGS="--coverage" LDFLAGS="--coverage"
    ./test_runner | tee test-results.txt
    
    # 生成覆盖率报告
    gcov -b *.c
    lcov --capture --directory . --output-file coverage.info
    genhtml coverage.info --output-directory coverage-report
```

### 5.2 测试结果检查

```yaml
- name: Check test results
  run: |
    if grep -q "FAIL" firmware/tests/test-results.txt; then
      echo "::error::Unit tests failed!"
      exit 1
    fi
    echo "All unit tests passed"
```

### 5.3 覆盖率报告上传

```yaml
- name: Upload coverage report
  uses: actions/upload-artifact@v5
  with:
    name: firmware-coverage-report
    path: firmware/tests/coverage-report/
```

---

## 6. 测试最佳实践

### 6.1 编写新测试的规范

1. **命名规范**: `test_<模块名>.c`，测试函数 `test_<功能>_<场景>`
2. **每个测试独立**: 使用 `setUp()`/`tearDown()` 确保测试间无状态泄漏
3. **边界值测试**: 测试最小值、最大值、零值、非法值
4. **错误注入**: 通过 Mock 模拟硬件故障和异常情况
5. **断言清晰**: 使用 `TEST_ASSERT_EQUAL_*` 系列宏，消息说明失败原因

### 6.2 测试用例模板

```c
#include "unity.h"
#include "module_under_test.h"
#include "mock/mock_dependency.h"

void setUp(void) {
    mock_dependency_reset();
    module_init();
}

void tearDown(void) {
    // 清理资源
}

void test_module_function_normal_case(void) {
    // 准备
    mock_dependency_set_return_value(42);
    
    // 执行
    int result = module_function(10);
    
    // 断言
    TEST_ASSERT_EQUAL_INT(52, result);
}

void test_module_function_invalid_input(void) {
    int result = module_function(-1);
    TEST_ASSERT_EQUAL_INT(-1, result);
}
```

---

## 7. 已知限制

1. **硬件相关代码无法直接测试**: 中断处理、DMA、USB控制器等需要硬件在环测试
2. **双核代码测试困难**: Core1代码需要特殊的Mock框架或硬件在环测试
3. **时序相关测试不稳定**: 依赖精确延时的测试在CI环境中可能不稳定
4. **覆盖率工具限制**: `gcov` 在交叉编译环境中需要特殊配置

---

## 8. 改进计划

| 优先级 | 改进项 | 预期收益 | 工作量 |
|--------|--------|---------|--------|
| 🔴 High | 添加 SPI Mock，测试光学传感器和按键矩阵 | 覆盖率+15% | 3天 |
| 🔴 High | 添加 IPC Mock，测试Core1扫描逻辑 | 覆盖率+10% | 2天 |
| 🟠 Medium | 添加 ADC Mock，测试摇杆处理 | 覆盖率+5% | 1天 |
| 🟠 Medium | 测试 config_hid 命令解析逻辑 | 覆盖率+10% | 2天 |
| 🟡 Low | 添加硬件在环测试(HIL)框架 | 端到端验证 | 1周 |
| 🟡 Low | 集成覆盖率门禁(低于80%拒绝合并) | 质量保障 | 1天 |

---

## 附录A: 测试用例统计

| 测试文件 | 测试函数数 | 最后更新 |
|---------|-----------|---------|
| test_config.c | 25 | 2026-08-20 |
| test_crc.c | 8 | 2026-08-18 |
| test_debounce.c | 12 | 2026-08-15 |
| test_encoder.c | 15 | 2026-08-22 |
| test_fault.c | 10 | 2026-08-19 |
| test_keymap.c | 18 | 2026-08-16 |
| test_macro.c | 20 | 2026-08-17 |
| test_scheduler.c | 12 | 2026-08-14 |
| **总计** | **120** | — |

---

*本文档随代码审查和测试补充持续更新。*
