# 单元测试

使用 [Unity](https://github.com/ThrowTheSwitch/Unity) 测试框架对纯逻辑模块进行单元测试。

## 目录结构

```
tests/
├── unity/              # Unity 测试框架源码
├── test_debounce.c     # 消抖模块测试
├── test_runner.c       # 测试主入口
├── build.bat           # Windows 构建脚本
├── Makefile            # Linux/Mac 构建脚本
└── README.md           # 本文档
```

## 运行测试

### Windows

需要安装 MinGW 或类似的 GCC 环境：

```cmd
cd tests
build.bat
```

### Linux / Mac

```bash
cd tests
make
```

## 测试结果示例

```
Unity test run 1 of 1
test_debounce_initial_state...PASS
test_debounce_single_key_press...PASS
test_debounce_single_key_release...PASS
test_debounce_multiple_keys...PASS
test_debounce_bounce...PASS
test_debounce_threshold_1...PASS

-----------------------
6 Tests 0 Failures 0 Ignored
OK
```

## 如何添加新的测试

1. 在 `tests/` 目录下新建 `test_xxx.c` 文件
2. 编写测试函数，每个测试函数以 `test_` 开头
3. 在 `test_runner.c` 中添加测试函数声明和 `RUN_TEST()` 调用
4. 在 `build.bat` 和 `Makefile` 的 `SRCS` 中添加新的源文件

## 可测试的模块

以下模块是纯逻辑，不依赖硬件，可以直接在 PC 上测试：

- ✅ `middleware/debounce` - 按键消抖
- ⏳ `middleware/scheduler` - 调度器（需要 mock time_us_32）
- ⏳ `app/keymap` - 按键映射（需要 mock config 模块）
- ⏳ `device/encoder` - 编码器状态机（需要 mock gpio_get）

## 为什么做单元测试？

1. **防止回归**：修改代码后跑一遍测试，确保原有功能没坏
2. **快速验证**：不用烧录到硬件，PC 上几秒就能验证逻辑
3. **代码质量**：写测试的过程本身就是对代码设计的检验
4. **CI 自动化**：配合 GitHub Actions，每次提交自动运行测试
