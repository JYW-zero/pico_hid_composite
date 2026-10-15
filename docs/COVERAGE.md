# 代码覆盖率报告

本文档说明固件和上位机的代码覆盖率集成方案，包括本地生成覆盖率报告、CI 集成、Codecov 上传和阈值检查。

## 概述

| 项目 | 覆盖率工具 | 报告格式 | CI 上传 |
|-----|-----------|---------|--------|
| 固件（C） | gcov + lcov + gcovr | Cobertura XML / HTML | Codecov |
| 上位机（.NET） | coverlet.collector | Cobertura XML | Codecov |

## 固件覆盖率

### 工具依赖

| 工具 | 用途 | 安装方法 |
|-----|------|---------|
| gcov | GCC 自带的覆盖率工具 | GCC 自带 |
| lcov | gcov 的前端，生成详细报告 | `sudo apt-get install lcov` |
| genhtml | 生成 HTML 报告 | lcov 自带 |
| gcovr | 生成 Cobertura XML | `pip install gcovr` |

### Makefile 目标

固件测试 Makefile（`firmware/tests/Makefile`）提供以下覆盖率相关目标：

| 目标 | 说明 |
|-----|------|
| `make coverage` | 运行带覆盖率的测试 |
| `make coverage-report` | 生成覆盖率报告（gcov + lcov + HTML） |
| `make coverage-check` | 覆盖率阈值检查（默认 60%） |
| `make coverage-cobertura` | 生成 Cobertura XML 格式（用于 Codecov） |
| `make clean` | 清理构建文件和覆盖率数据 |

### 本地使用

```bash
cd firmware/tests

# 1. 运行带覆盖率的测试并生成报告
make coverage-report

# 2. 查看 HTML 报告
open coverage/html/index.html  # macOS
# 或
xdg-open coverage/html/index.html  # Linux

# 3. 检查覆盖率阈值
make coverage-check COVERAGE_THRESHOLD=60

# 4. 生成 Cobertura XML（用于 Codecov）
make coverage-cobertura

# 5. 清理
make clean
```

### 覆盖率数据说明

- `.gcno` 文件：编译时生成的基本块信息
- `.gcda` 文件：运行测试后生成的覆盖率数据
- `*.gcov` 文件：gcov 生成的文本覆盖率报告
- `coverage/coverage.info`：lcov 生成的原始覆盖率数据
- `coverage/coverage_filtered.info`：过滤后的覆盖率数据（排除测试框架和 mock）
- `coverage/coverage.xml`：Cobertura XML 格式（用于 Codecov）
- `coverage/html/`：HTML 格式的可视化报告

### 覆盖率过滤

lcov 报告会自动过滤以下内容：
- `unity/`：Unity 测试框架
- `mock/`：Mock 函数
- `test_*`：测试文件本身

只统计实际的固件源代码（`src/` 目录下的文件）。

## 上位机覆盖率

### 工具依赖

| 工具 | 用途 | 安装方法 |
|-----|------|---------|
| coverlet.collector | .NET 覆盖率收集器 | NuGet 包（已集成） |
| Microsoft.NET.Test.Sdk | 测试 SDK | NuGet 包（已集成） |

### 项目配置

两个测试项目都已集成 coverlet.collector：

- `pc_tool/tests/HidConfigTool.Tests/HidConfigTool.Tests.csproj`
- `pc_tool/tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj`

```xml
<PackageReference Include="coverlet.collector" Version="6.0.4" />
```

### 本地使用

```bash
cd pc_tool

# 运行单元测试并收集覆盖率
dotnet test tests/HidConfigTool.Tests/HidConfigTool.Tests.csproj \
  --configuration Release \
  --collect:"XPlat Code Coverage"

# 运行集成测试并收集覆盖率
dotnet test tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj \
  --configuration Release \
  --collect:"XPlat Code Coverage"

# 覆盖率报告位置
# tests/HidConfigTool.Tests/TestResults/<guid>/coverage.cobertura.xml
# tests/HidConfigTool.IntegrationTests/TestResults/<guid>/coverage.cobertura.xml
```

### 覆盖率报告格式

coverlet 生成的是 Cobertura XML 格式，包含：
- 行覆盖率（line-rate）
- 分支覆盖率（branch-rate）
- 每个类、方法、行的覆盖情况

## CI 集成

### GitHub Actions 配置

CI 工作流（`.github/workflows/ci.yml`）已集成覆盖率：

#### 固件覆盖率步骤

```yaml
# 安装覆盖率工具
- name: Install coverage tools
  run: |
    sudo apt-get install -y lcov
    pip install gcovr

# 运行带覆盖率的测试
- name: Run firmware unit tests with coverage
  run: |
    cd firmware/tests
    make coverage-report

# 生成 Cobertura XML
- name: Generate Cobertura XML report
  run: |
    cd firmware/tests
    make coverage-cobertura

# 覆盖率阈值检查
- name: Check coverage threshold
  run: |
    cd firmware/tests
    make coverage-check COVERAGE_THRESHOLD=60

# 上传到 Codecov
- name: Upload coverage report (Codecov)
  uses: codecov/codecov-action@v5
  with:
    files: firmware/tests/coverage/coverage.xml
    flags: firmware
    name: firmware-coverage
```

#### 上位机覆盖率步骤

```yaml
# 运行单元测试（带覆盖率）
- name: Run unit tests
  run: |
    dotnet test pc_tool/tests/HidConfigTool.Tests/HidConfigTool.Tests.csproj \
      --configuration Release \
      --collect:"XPlat Code Coverage"

# 运行集成测试（带覆盖率）
- name: Run integration tests
  run: |
    dotnet test pc_tool/tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj \
      --configuration Release \
      --collect:"XPlat Code Coverage"

# 上传到 Codecov
- name: Upload coverage to Codecov
  uses: codecov/codecov-action@v5
  with:
    files: pc_tool/tests/**/coverage.cobertura.xml
    flags: pc-tool
    name: pc-tool-coverage

# 覆盖率阈值检查
- name: Check coverage threshold
  shell: pwsh
  run: |
    # 解析 Cobertura XML，计算平均覆盖率
    # 低于 60% 发出警告
```

### Codecov 集成

#### 启用 Codecov

1. 访问 [Codecov.io](https://codecov.io/)
2. 使用 GitHub 账号登录
3. 授权访问你的仓库
4. 启用仓库的覆盖率收集

#### Codecov 配置文件

在仓库根目录创建 `codecov.yml`：

```yaml
codecov:
  require_ci_to_pass: yes

coverage:
  precision: 2
  round: down
  range: "60...100"

  status:
    project:
      default:
        target: 60%
        threshold: 5%
        if_no_uploads: error
    patch:
      default:
        target: 60%
        threshold: 5%
        if_no_uploads: error

flags:
  firmware:
    paths:
      - firmware/src/
  pc-tool:
    paths:
      - pc_tool/shared/

ignore:
  - "firmware/tests/"
  - "pc_tool/tests/"
  - "**/obj/**"
  - "**/bin/**"
```

#### PR 页面覆盖率展示

启用 Codecov 后，PR 页面会自动显示：
- 整体覆盖率变化
- 新增代码覆盖率
- 覆盖率趋势图
- 详细的文件覆盖率报告

### 覆盖率阈值

| 项目 | 阈值 | 低于阈值行为 |
|-----|------|------------|
| 固件 | 60% | CI 警告（不阻止合并） |
| 上位机 | 60% | CI 警告（不阻止合并） |
| Codecov project | 60% | Codecov 检查失败（可配置阻止合并） |
| Codecov patch | 60% | 新增代码覆盖率检查 |

## 覆盖率徽章

在 README.md 中添加覆盖率徽章：

```markdown
[![codecov](https://codecov.io/gh/<owner>/<repo>/branch/main/graph/badge.svg)](https://codecov.io/gh/<owner>/<repo>)
```

替换 `<owner>` 和 `<repo>` 为你的 GitHub 用户名和仓库名。

## 提高覆盖率的建议

### 固件

1. **边界条件测试**：测试函数的输入边界（最小值、最大值、空值）
2. **错误路径测试**：测试错误处理代码（内存分配失败、无效参数等）
3. **状态机测试**：测试状态机的所有状态转换
4. **配置模块测试**：测试配置的读写、校验、迁移
5. **通信协议测试**：测试协议解析、错误处理

### 上位机

1. **ViewModel 测试**：测试所有命令的执行逻辑
2. **服务层测试**：测试设备服务、配置服务的业务逻辑
3. **协议解析测试**：测试 HID 协议的封包/解包
4. **错误处理测试**：测试异常情况的处理
5. **转换器测试**：测试类型转换器、值转换器

## 常见问题

### Q: 固件覆盖率数据为空？

A: 确保使用了 `--coverage` 编译选项，并且运行了生成的可执行文件。检查 `.gcno` 和 `.gcda` 文件是否生成。

### Q: lcov 报错 "negative count"？

A: 这通常是因为多次运行测试导致的。运行 `make clean` 后重新生成覆盖率报告。

### Q: 上位机覆盖率不包含某些项目？

A: 确保被测试的项目引用了 `coverlet.collector`，并且测试项目引用了被测试项目。检查 `.csproj` 文件中的 `ProjectReference`。

### Q: Codecov 上传失败？

A: 检查 Codecov 是否启用了仓库，检查 `codecov.yml` 配置是否正确。CI 中的 Codecov 上传步骤设置了 `continue-on-error: true`，不会阻止合并。

### Q: 如何查看详细的覆盖率报告？

A: 
- 固件：打开 `firmware/tests/coverage/html/index.html`
- 上位机：使用 Visual Studio 的覆盖率报告功能，或使用 `dotnet reportgenerator` 工具
- Codecov：访问 Codecov 网站查看在线报告

## 相关文档

- [CI/CD 流水线说明](CI-CD.md)
- [固件测试说明](../firmware/tests/README.md)
- [上位机单元测试说明](../pc_tool/tests/HidConfigTool.Tests/README.md)
- [上位机集成测试说明](../pc_tool/tests/HidConfigTool.IntegrationTests/README.md)
- [gcov 文档](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)
- [lcov 文档](https://ltp.sourceforge.net/coverage/lcov.php)
- [coverlet 文档](https://github.com/coverlet-coverage/coverlet)
- [Codecov 文档](https://docs.codecov.com/)
