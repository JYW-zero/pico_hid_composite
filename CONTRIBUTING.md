# 贡献指南

首先，感谢你花时间为这个项目做贡献！🎉

本文档提供了为项目做贡献的指南和最佳实践。

---

## 📋 行为准则

参与本项目即表示你同意遵守我们的行为准则：

- 尊重他人，友善沟通
- 接受不同的观点和经验
- 专注于对社区最有利的事情
- 对其他贡献者保持同理心

---

## 🚀 如何开始

### 1. Fork 并克隆仓库

```bash
# Fork 本仓库后，克隆你的 Fork
git clone https://github.com/your-username/pico_hid_composite.git
cd pico_hid_composite

# 添加上游仓库
git remote add upstream https://github.com/original-username/pico_hid_composite.git
```

### 2. 创建分支

```bash
# 从 main 分支创建新分支
git checkout -b feature/your-feature-name
# 或
git checkout -b fix/your-bug-fix
```

### 3. 设置开发环境

参考 [开发环境配置指南](docs/SETUP.md) 搭建你的开发环境。

---

## 💻 开发规范

### 分支命名规范

| 类型 | 前缀 | 示例 |
|------|------|------|
| 新功能 | `feature/` | `feature/macro-recording` |
| Bug 修复 | `fix/` | `fix/flash-write-alignment` |
| 文档 | `docs/` | `docs/update-readme` |
| 重构 | `refactor/` | `refactor/protocol-layer` |
| 性能优化 | `perf/` | `perf/spi-speedup` |
| 测试 | `test/` | `test/add-unit-tests` |
| 构建/CI | `ci/` | `ci/improve-cache` |

### 提交信息规范

我们遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

**类型（type）：**

- `feat` - 新功能
- `fix` - Bug 修复
- `docs` - 文档变更
- `style` - 代码格式（不影响功能）
- `refactor` - 代码重构
- `perf` - 性能优化
- `test` - 测试相关
- `build` - 构建系统或依赖
- `ci` - CI 配置
- `chore` - 其他杂项

**示例：**

```
feat(macro): 添加宏录制功能

实现了宏录制功能，支持按键录制和鼠标动作录制。

Closes #123
```

```
fix(flash): 修复 Flash 写入页对齐问题

Flash 写入大小需要对齐到 256 字节页边界，否则会导致读取失败。

Fixes #456
```

---

## 🔧 编码规范

### 固件（C 语言）

详细规范请参考 [编码规范文档](docs/CODING-STANDARDS.md)。

**要点：**

- 使用 C11 标准
- 缩进使用 4 个空格
- 大括号换行（Allman 风格）
- 变量和函数使用 snake_case
- 宏使用 UPPER_SNAKE_CASE
- 头文件使用 include guard
- 函数必须有返回值检查
- 禁止使用全局变量（除非必要）

### 上位机（C#）

- 遵循 .NET 编码规范
- 使用 PascalCase 命名类、方法、属性
- 使用 camelCase 命名局部变量和参数
- 遵循 MVVM 模式
- 使用 XML 文档注释

---

## 🧪 测试

### 固件测试

- 新增功能必须添加单元测试
- 使用 Unity 测试框架
- 测试文件放在 `firmware/tests/` 目录

```bash
# 运行单元测试
cd firmware/tests
make
./run_tests
```

### 上位机测试

- 使用 xUnit 测试框架
- 测试项目放在 `pc_tool/tests/HidConfigTool.Tests/`

```bash
# 运行单元测试
dotnet test pc_tool/HidConfigTool.slnx
```

---

## 📝 提交 Pull Request

### PR 准备清单

提交 PR 前，请确认：

- [ ] 代码遵循项目编码规范
- [ ] 已自我审查代码
- [ ] 已添加必要的注释
- [ ] 已更新相关文档
- [ ] 没有引入新的编译警告
- [ ] 已添加测试（如适用）
- [ ] 所有测试通过
- [ ] 分支已与最新的 main 同步

### PR 描述

PR 描述应包含：

1. **变更描述** - 清晰说明做了什么
2. **关联 Issue** - 关联相关的 Issue
3. **变更类型** - Bug 修复 / 新功能 / 文档等
4. **测试说明** - 如何测试，测试结果
5. **截图** - UI 变更请附截图

---

## 🐛 报告 Bug

报告 Bug 时，请提供：

- 清晰的标题和描述
- 复现步骤
- 预期行为和实际行为
- 环境信息（硬件型号、固件版本、操作系统等）
- 相关日志或截图

使用 [Bug 报告模板](.github/ISSUE_TEMPLATE/bug_report.md) 提交 Issue。

---

## ✨ 功能请求

提出功能请求时，请说明：

- 功能描述
- 解决的问题
- 预期的解决方案
- 替代方案（如有考虑）

使用 [功能请求模板](.github/ISSUE_TEMPLATE/feature_request.md) 提交 Issue。

---

## ❓ 寻求帮助

如果你有问题：

1. 先查看 [文档](docs/)
2. 搜索现有 [Issues](../../issues)
3. 如果没有找到答案，提交一个 [Question Issue](../../issues/new?template=question.md)

---

## 📄 许可证

通过贡献代码，你同意你的贡献将根据项目的 [MIT 许可证](LICENSE) 进行许可。

---

## 🙏 感谢

再次感谢你的贡献！每一个贡献都很重要，无论大小。

如果你觉得这个项目有帮助，请给个 ⭐ Star！
