# CI/CD 流水线说明

本文档说明项目的持续集成/持续部署（CI/CD）流水线配置。

## 工作流概览

### 主工作流：`.github/workflows/ci.yml`

**触发条件**：
- Push 到 `main`、`master`、`develop` 分支
- Push 标签 `v*`（如 `v1.0.0`）
- Pull Request 到 `main`、`master`、`develop` 分支
- GitHub Release 发布
- 手动触发（workflow_dispatch）

**Jobs**：

| Job | 运行环境 | 说明 |
|-----|---------|------|
| `firmware` | Ubuntu | 固件构建 + Unity 测试 + cppcheck 静态分析 + clang-format 检查 |
| `pc-tool` | Windows | 上位机构建 + 单元测试 + 集成测试 + Roslyn Analyzers |
| `docs` | Ubuntu | 文档检查（可选） |
| `ci-status` | Ubuntu | CI 状态汇总，用于 PR 检查和徽章 |

---

## 固件 CI 详细说明

### 1. 系统依赖安装

```bash
sudo apt-get install -y \
  cmake \
  ninja-build \
  gcc-arm-none-eabi \
  libnewlib-arm-none-eabi \
  gcc \
  make \
  cppcheck \
  clang-format \
  git
```

### 2. Pico SDK 拉取

```bash
git clone --depth 1 --branch 2.3.0 \
  https://github.com/raspberrypi/pico-sdk.git $HOME/pico-sdk
cd $HOME/pico-sdk
git submodule update --init --depth 1
```

### 3. 代码格式检查（clang-format）

- 使用项目根目录的 `.clang-format` 配置文件
- 检查 `firmware/src` 和 `firmware/include` 下的所有 `.c` 和 `.h` 文件
- 格式错误会导致 CI 失败

### 4. cppcheck 静态分析

```bash
cppcheck \
  --enable=all \
  --inconclusive \
  --std=c11 \
  --platform=unix32 \
  --suppress=missingIncludeSystem \
  --suppress=unusedFunction \
  --suppress=variableScope \
  -I include \
  -I src \
  src/
```

**检查项**：
- 内存泄漏
- 空指针解引用
- 数组越界
- 未初始化变量
- 未使用的函数
- 变量作用域问题

**报告位置**：`firmware/build/cppcheck-report/`

### 5. Unity 单元测试

```bash
cd firmware/tests
make clean
make all
./run_tests
```

**测试覆盖**：
- 去抖逻辑（6 个测试）
- 按键映射（14 个测试）
- 编码器（8 个测试）
- 调度器（9 个测试）
- 故障记录（5 个测试）
- 配置模块（12 个测试）
- CRC32 校验（14 个测试）
- 宏功能（23 个测试）

**总计**：91 个测试用例

### 6. 固件构建

```bash
export PICO_SDK_PATH=$HOME/pico-sdk
cd firmware
mkdir -p build
cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

### 7. 固件大小检查

- 检查 `.uf2` 文件大小
- 超过 1.5MB 会发出警告

### 8. 构建产物

上传以下文件作为 Artifacts：
- `pico_hid_composite.uf2`
- `pico_hid_composite.elf`
- `pico_hid_composite.bin`
- `pico_hid_composite.hex`
- `pico_hid_composite.map`

---

## 上位机 CI 详细说明

### 1. .NET SDK 版本

- .NET 10.0.x

### 2. NuGet 包缓存

使用 GitHub Actions Cache 加速依赖还原：
- 缓存路径：`~/.nuget/packages`
- Cache Key：基于所有 `.csproj` 文件的哈希

### 3. Roslyn Analyzers 代码分析

```bash
dotnet build pc_tool/HidConfigTool.slnx \
  --configuration Release \
  -p:RunAnalyzers=true \
  -p:AnalysisLevel=latest-all \
  -p:EnforceCodeStyleInBuild=true
```

**分析级别**：`latest-all`（启用所有最新的分析规则）

**检查项**：
- 代码风格
- 命名规范
- 性能问题
- 安全问题
- 可维护性问题

### 4. 构建

```bash
dotnet build pc_tool/HidConfigTool.slnx --configuration Release
```

### 5. 单元测试

```bash
dotnet test pc_tool/tests/HidConfigTool.Tests/HidConfigTool.Tests.csproj \
  --configuration Release \
  --logger "trx;LogFileName=unit-test-results.trx" \
  --collect:"XPlat Code Coverage"
```

**测试覆盖**：
- MainViewModel（22 个测试）
- MousePageViewModel（22 个测试）
- ConfigImportExportService（35 个测试）
- HidErrorMapper（30 个测试）
- DeviceConfig、HidKeyConverter、KeyDefinition、Macro（20 个测试）

**总计**：143 个测试用例

### 6. 集成测试

```bash
dotnet test pc_tool/tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj \
  --configuration Release \
  --logger "trx;LogFileName=integration-test-results.trx" \
  --collect:"XPlat Code Coverage"
```

**测试覆盖**：
- DeviceConnectionTests（20 个测试）
- DeviceConfigReadWriteTests（25 个测试）
- FirmwareUpgradeTests（12 个测试）

**总计**：57 个测试用例

### 7. 测试结果和代码覆盖率

上传以下文件作为 Artifacts：
- `**/TestResults/**/*.trx`（测试结果）
- `**/TestResults/**/coverage.cobertura.xml`（代码覆盖率）

### 8. 发布版本

**依赖框架版本**（Framework-Dependent）：
```bash
dotnet publish pc_tool/windows/HidConfigTool.App/HidConfigTool.App.csproj \
  --configuration Release \
  --self-contained false \
  --output publish-framework-dependent
```

**自包含单文件版本**（Self-Contained Single-File）：
```bash
dotnet publish pc_tool/windows/HidConfigTool.App/HidConfigTool.App.csproj \
  --configuration Release \
  --self-contained true \
  --runtime win-x64 \
  -p:PublishSingleFile=true \
  -p:IncludeNativeLibrariesForSelfExtract=true \
  -p:EnableCompressionInSingleFile=true \
  --output publish-self-contained
```

---

## PR 自动检查

### 触发条件

所有 Pull Request 到 `main`、`master`、`develop` 分支都会自动触发 CI 检查。

### 必需检查项

以下 Job 必须通过才能合并：
- ✅ `firmware`（固件构建 + 测试 + 分析）
- ✅ `pc-tool`（上位机构建 + 测试 + 分析）

`docs` Job 为可选检查，不会阻止合并。

### 分支保护设置

建议在 GitHub 仓库设置中配置分支保护：

1. 进入 **Settings → Branches → Branch protection rules**
2. 点击 **Add rule**
3. 配置以下选项：
   - **Branch name pattern**：`main`（或 `master`、`develop`）
   - ✅ **Require a pull request before merging**
   - ✅ **Require approvals**（1 个审批）
   - ✅ **Require status checks to pass before merging**
     - 搜索并添加：`Firmware (Build + Test + Analysis)`
     - 搜索并添加：`PC Tool (Build + Test + Analysis)`
   - ✅ **Require branches to be up to date before merging**
   - ✅ **Do not allow bypassing the above settings**
4. 点击 **Create**

---

## 自动发布

### 触发条件

- Push 标签 `v*`（如 `v1.0.0`、`v1.1.0-beta`）
- GitHub Release 发布

### 发布内容

**固件**：
- `pico_hid_composite.uf2`
- `pico_hid_composite.elf`
- `pico_hid_composite.bin`

**上位机**：
- `pc-tool-windows-framework-dependent.zip`
- `pc-tool-windows-self-contained.zip`

### 发布步骤

1. 更新版本号（固件和上位机）
2. 提交更改
3. 创建标签：`git tag v1.0.0`
4. 推送标签：`git push origin v1.0.0`
5. CI 自动构建并发布到 GitHub Releases

---

## 徽章

### CI 状态徽章

```markdown
[![CI](https://github.com/JYW-zero/pico_hid_composite/actions/workflows/ci.yml/badge.svg)](https://github.com/JYW-zero/pico_hid_composite/actions/workflows/ci.yml)
```

### 其他徽章

```markdown
[![Firmware](https://img.shields.io/badge/Firmware-Build%20%7C%20Test%20%7C%20Analysis-blue)]
[![PC Tool](https://img.shields.io/badge/PC%20Tool-Build%20%7C%20Test%20%7C%20Analysis-purple)]
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-RP2350-red)]
[![SDK](https://img.shields.io/badge/Pico%20SDK-2.3.0-green)]
[![.NET](https://img.shields.io/badge/.NET-10.0-purple)]
```

---

## 本地运行 CI 检查

### 固件

```bash
# 代码格式检查
cd firmware
find src include -name "*.c" -o -name "*.h" | xargs clang-format --dry-run --Werror --style=file

# 静态分析
cppcheck --enable=all --std=c11 -I include -I src src/

# 单元测试
cd tests
make clean && make all && ./run_tests

# 构建
cd ..
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

### 上位机

```bash
cd pc_tool

# 还原依赖
dotnet restore HidConfigTool.slnx

# 代码分析
dotnet build HidConfigTool.slnx --configuration Release -p:RunAnalyzers=true -p:AnalysisLevel=latest-all

# 构建
dotnet build HidConfigTool.slnx --configuration Release

# 单元测试
dotnet test tests/HidConfigTool.Tests/HidConfigTool.Tests.csproj --configuration Release

# 集成测试
dotnet test tests/HidConfigTool.IntegrationTests/HidConfigTool.IntegrationTests.csproj --configuration Release
```

---

## 常见问题

### Q: CI 失败怎么办？

A: 
1. 查看 CI 日志，定位失败的步骤
2. 本地运行相同的命令复现问题
3. 修复问题后提交，CI 会自动重新运行

### Q: 如何跳过 CI？

A: 在 commit message 中添加 `[skip ci]` 或 `[ci skip]`。

### Q: 如何手动触发 CI？

A: 
1. 进入 GitHub Actions 页面
2. 选择 `CI` workflow
3. 点击 `Run workflow`
4. 选择分支，点击 `Run workflow`

### Q: 构建产物在哪里下载？

A: 
1. 进入 GitHub Actions 页面
2. 选择对应的 CI 运行
3. 页面底部的 `Artifacts` 部分可以下载

---

## 相关文档

- [开发日志](DEVLOG.md)
- [固件测试说明](../firmware/tests/README.md)
- [上位机单元测试说明](../pc_tool/tests/HidConfigTool.Tests/README.md)
- [上位机集成测试说明](../pc_tool/tests/HidConfigTool.IntegrationTests/README.md)
