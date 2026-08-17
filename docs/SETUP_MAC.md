# macOS 固件开发环境安装教程

本文记录在 Apple Silicon Mac 上编译本项目固件时，实际安装过的软件和命令。  
上位机（Windows WPF 配置软件）在 Mac 上无法运行，本文只覆盖 **固件编译和烧录**。

实测环境：macOS 15（darwin 24.6.0）/ Apple Silicon / Homebrew 5.x。

---

## 0. 会装上什么

| 软件 | 版本（本次实际） | 用途 | 安装方式 |
|------|------------------|------|----------|
| Homebrew | 已有则跳过 | 包管理 | 见官网 |
| CMake | 4.4.2 | 生成编译工程 | `brew install cmake` |
| Ninja | 1.13.2 | 执行编译 | `brew install ninja` |
| picotool | 2.3.0 | 查设备 / 烧录 | `brew install picotool` |
| .NET SDK | 10.0 | 跨平台配置工具 | `brew install dotnet` |
| Arm GNU Toolchain | 15.3.Rel1 | 交叉编译（带 C 库） | `brew install --cask gcc-arm-embedded` |
| Pico SDK | 2.3.0 | 官方 SDK | git clone 到 `~/pico-sdk` |
| TinyUSB | SDK 锁定的子模块 | USB HID 协议栈 | SDK 的 `lib/tinyusb` |

**不要用** `brew install arm-none-eabi-gcc` 来编本项目。  
那个 formula（例如 16.2.0）**没有 newlib**，链接阶段会报 `cannot find -lc`。必须用 cask `gcc-arm-embedded`。

---

## 1. 安装 Homebrew（没有才装）

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

确认：

```bash
brew --version
```

---

## 2. 安装编译工具

```bash
brew install cmake ninja picotool
```

官方 ARM 工具链需要管理员密码（会弹出 macOS 安装器）：

```bash
brew install --cask gcc-arm-embedded
```

装好后编译器在：

```
/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi/bin/arm-none-eabi-gcc
```

验证：

```bash
cmake --version
ninja --version
picotool version
/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi/bin/arm-none-eabi-gcc --version
```

`PATH` 里如果先找到 Homebrew 的 `arm-none-eabi-gcc`，编固件仍会失败。编译时必须把官方工具链放到 PATH 最前面（见第 5 节）。

---

## 3. 安装 Pico SDK 2.3.0

本项目锁定 SDK **2.3.0**。放到家目录（不要放进 git 仓库）：

```bash
git clone --depth 1 --branch 2.3.0 git@github.com:raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk
git submodule update --init --depth 1 lib/tinyusb
```

如果 `git submodule` 用 HTTPS 失败（`Connection reset by peer`），改用 SSH 拉 TinyUSB：

```bash
# SDK 锁定的 TinyUSB 提交号
PIN=86ad6e56c1700e85f1c5678607a762cfe3aa2f47
rm -rf ~/pico-sdk/lib/tinyusb
git clone git@github.com:hathach/tinyusb.git ~/pico-sdk/lib/tinyusb
cd ~/pico-sdk/lib/tinyusb
git fetch --depth 1 origin "$PIN"
git checkout "$PIN"
```

验证：

```bash
test -f ~/pico-sdk/pico_sdk_init.cmake && echo SDK_OK
test -f ~/pico-sdk/lib/tinyusb/src/tusb.h && echo TINYUSB_OK
```

需要本机已配置 GitHub SSH（`ssh -T git@github.com` 能通）。HTTPS 访问 GitHub 不稳定时，用 SSH 更可靠。

---

## 4. 克隆本仓库

```bash
git clone git@github.com:JYW-zero/pico_hid_composite.git
cd pico_hid_composite
```

---

## 5. 编译固件

每次开新终端先设环境变量（或写进 `~/.zshrc`）：

```bash
export PATH="/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi/bin:$PATH"
export PICO_SDK_PATH="$HOME/pico-sdk"
```

在 **`firmware` 目录**编译：

```bash
cd /Users/gtm/github/pico_hid_composite/firmware
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=pico2
ninja -C build
```

以后改了代码，只需：

```bash
cd firmware
ninja -C build
```

产物（Cursor 默认隐藏 `build` 目录，用 Finder 看）：

| 文件 | 说明 |
|------|------|
| `firmware/build/pico_hid_composite.uf2` | **烧录用这个** |
| `firmware/build/pico_hid_composite.elf` | 调试用 |

Cursor 看不到 `build`：项目 `.vscode/settings.json` 里设置了 `"**/build": true`。这是编译产物，不是源码。

---

## 6. 烧录到 Pico 2

只把这一个文件拖进板子：

```
firmware/build/pico_hid_composite.uf2
```

1. 按住板上 **BOOTSEL**
2. 插入 USB
3. 电脑出现 U 盘（`RPI-RP2` 或类似名字）
4. 把 `.uf2` 拖进去，板子自动重启

或：

```bash
# 先按 BOOTSEL 再插上，然后：
picotool load firmware/build/pico_hid_composite.uf2 -f
```

烧进去之后，这块板在 Mac 上会变成 USB 键盘/鼠标等 HID 设备，**可以当输入设备用**。  
不能用的是 Windows 上那个改键、改 DPI 的配置软件。

---

## 7. 建议写进 ~/.zshrc（可选）

避免每次手动 `export`：

```bash
export PATH="/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi/bin:$PATH"
export PICO_SDK_PATH="$HOME/pico-sdk"
```

---

## 8. 本次机器上多装了、但不要用的包

安装过程中还执行过：

```bash
brew install arm-none-eabi-gcc
```

它会装上 `arm-none-eabi-gcc` 16.2.0 以及 binutils、gmp 等依赖，并且占用 `/opt/homebrew/bin/arm-none-eabi-gcc`。  
**编本项目请忽略它**，始终用第 2 节的 `gcc-arm-embedded`。

不想留着可以卸掉（可选）：

```bash
brew uninstall arm-none-eabi-gcc
```

---

## 9. 跨平台配置工具（C# / Avalonia）

WPF 版 `pc_tool/windows/HidConfigTool.App` 仍只能在 Windows 上运行。  
跨平台版本在 `pc_tool/desktop/HidConfigTool.Desktop`，**共享核心层**，Mac / Windows 各自编译。

先安装 .NET 10：

```bash
brew install dotnet
export DOTNET_ROOT="/opt/homebrew/opt/dotnet/libexec"
export PATH="/opt/homebrew/opt/dotnet/bin:$PATH"
```

运行：

```bash
cd pc_tool
dotnet run --project desktop/HidConfigTool.Desktop
```

Windows 上同样执行上面的 `dotnet run`（需安装 .NET 10 SDK）。  
旧的 WPF 工程仍可用 Visual Studio 打开，给同学在 Windows 上继续用。

---

## 10. 本次机器上多装了、但不要用的包
