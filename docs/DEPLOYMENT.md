# 文档网站部署指南

本文档说明如何部署和维护指动 HID 复合设备的在线文档网站。

## 概述

本项目使用 [MkDocs](https://www.mkdocs.org/) + [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/) 构建文档网站，并通过 GitHub Pages 自动部署。

- **文档源文件**：`docs/` 目录下的 Markdown 文件
- **配置文件**：`mkdocs.yml`（项目根目录）
- **部署工作流**：`.github/workflows/deploy-docs.yml`
- **自动部署触发**：推送到 `main` 分支且修改了 `docs/` 或 `mkdocs.yml`

## 本地预览

### 1. 安装依赖

```bash
# 安装 Python（如未安装）
# Windows: 从 https://www.python.org/downloads/ 下载
# macOS: brew install python
# Linux: sudo apt-get install python3 python3-pip

# 安装 MkDocs 和插件
pip install --upgrade pip
pip install \
  mkdocs \
  mkdocs-material \
  mkdocs-video \
  mkdocs-glightbox \
  pillow \
  cairosvg
```

### 2. 启动本地预览服务器

```bash
# 在项目根目录执行
mkdocs serve
```

启动后，在浏览器中访问 `http://127.0.0.1:8000/` 查看文档网站。

修改 Markdown 文件后，页面会自动刷新。

### 3. 构建静态网站

```bash
# 构建到 site/ 目录
mkdocs build --clean

# 严格模式构建（遇到警告会报错）
mkdocs build --clean --strict
```

## 自动部署（GitHub Pages）

### 1. 启用 GitHub Pages

1. 进入 GitHub 仓库的 **Settings** → **Pages**
2. 在 **Build and deployment** 部分：
   - **Source**：选择 `GitHub Actions`
3. 保存设置

### 2. 触发部署

部署工作流会在以下情况自动触发：

- 推送到 `main` 分支，且修改了以下文件：
  - `docs/**`
  - `mkdocs.yml`
  - `README.md`
  - `THIRD_PARTY_LICENSES.md`
  - `.github/workflows/deploy-docs.yml`
- 手动触发（Actions 页面点击 **Run workflow**）
- Pull Request（仅构建，不部署）

### 3. 查看部署状态

1. 进入 GitHub 仓库的 **Actions** 页面
2. 点击 **Deploy Documentation** 工作流
3. 查看最新的运行记录
4. 部署成功后，在 **Settings** → **Pages** 页面查看网站 URL

### 4. 自定义域名（可选）

如需使用自定义域名：

1. 在域名服务商处添加 CNAME 记录，指向 `<username>.github.io`
2. 在 `docs/` 目录下创建 `CNAME` 文件，内容为自定义域名
3. 重新部署

## 文档结构

```
docs/
├── index.md                    # 文档首页
├── QUICKSTART.md               # 快速开始指南
├── SETUP.md                    # Windows 编译指南
├── SETUP_MAC.md                # macOS 编译指南
├── FEATURES.md                 # 功能列表
├── ROADMAP.md                  # 开发路线图
├── PROGRESS.md                 # 开发进度
├── DEVLOG.md                   # 开发日志
├── CODING-STANDARDS.md         # 编码规范
├── CODE_AUDIT.md               # 代码审计
├── CI-CD.md                    # CI/CD 说明
├── COVERAGE.md                 # 代码覆盖率
├── USB_COMPLIANCE.md           # USB 合规审计
├── CONFIG_HID_PROTOCOL.md      # HID 配置协议
├── DFU_UPGRADE.md              # DFU 升级协议
├── ...                         # 其他文档
├── user_manual/                # 用户手册
│   ├── README.md
│   ├── 用户手册_zh-CN.md
│   └── User_Manual_en-US.md
├── certification/              # 认证文档
│   ├── README.md
│   └── 认证准备清单.md
├── assets/                     # 静态资源（图片等）
├── stylesheets/                # 自定义 CSS
│   └── extra.css
└── javascripts/                # 自定义 JavaScript
    └── mathjax.js
```

## 添加新文档

### 1. 创建 Markdown 文件

在 `docs/` 目录下创建新的 `.md` 文件，或在子目录中创建。

### 2. 更新导航结构

编辑 `mkdocs.yml` 中的 `nav` 部分，添加新文档的引用。

```yaml
nav:
  - 新分类:
      - 新文档: new-document.md
```

### 3. 本地预览验证

```bash
mkdocs serve
```

在浏览器中检查新文档是否正确显示，导航是否正常。

### 4. 提交并推送

```bash
git add docs/new-document.md mkdocs.yml
git commit -m "docs: 添加新文档"
git push
```

推送到 `main` 分支后，GitHub Actions 会自动构建并部署。

## MkDocs 配置说明

### 主题配置

`mkdocs.yml` 中的 `theme` 部分配置了 Material 主题：

- **语言**：中文（`language: zh`）
- **配色**：支持浅色/深色模式自动切换
- **主色调**：靛蓝色（indigo）
- **字体**：Roboto + Roboto Mono

### 插件

- **search**：全文搜索（支持中文和英文）
- **mkdocs-video**：嵌入视频
- **glightbox**：图片灯箱效果

### Markdown 扩展

启用了多种 Markdown 扩展：

- **admonition**：提示框（注意、警告、提示等）
- **footnotes**：脚注
- **toc**：目录（带永久链接）
- **pymdownx.highlight**：代码高亮
- **pymdownx.superfences**：高级代码块（支持 Mermaid 图表）
- **pymdownx.tasklist**：任务列表
- **pymdownx.emoji**：表情符号
- **pymdownx.tabbed**：标签页

## 常见问题

### Q: 部署失败怎么办？

A: 
1. 检查 Actions 页面的错误日志
2. 常见原因：
   - `mkdocs.yml` 配置错误（如引用了不存在的文件）
   - Markdown 语法错误
   - 插件安装失败
3. 本地运行 `mkdocs build --strict` 检查错误

### Q: 如何修改网站标题和描述？

A: 编辑 `mkdocs.yml` 中的 `site_name`、`site_description` 等字段。

### Q: 如何添加网站图标？

A: 
1. 将图标文件放入 `docs/assets/` 目录
2. 在 `mkdocs.yml` 的 `theme` 部分设置 `favicon: assets/favicon.png`

### Q: 文档图片如何处理？

A: 
1. 将图片放入 `docs/assets/images/` 目录
2. 在 Markdown 中使用相对路径引用：`![图片描述](assets/images/image.png)`
3. 建议使用 WebP 或优化后的 PNG/JPG 格式

## 参考资源

- [MkDocs 官方文档](https://www.mkdocs.org/)
- [Material for MkDocs 官方文档](https://squidfunk.github.io/mkdocs-material/)
- [GitHub Pages 文档](https://docs.github.com/en/pages)
- [GitHub Actions 文档](https://docs.github.com/en/actions)

---

**指动科技 / Zhidong Technology**
