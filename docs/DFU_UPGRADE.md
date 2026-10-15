# USB DFU 固件升级功能说明

## 概述

本项目实现了基于自定义 HID 协议的 USB DFU 固件升级功能，采用两级启动架构：

- **Bootloader**：位于 Flash 起始位置（32KB），负责固件升级和应用程序启动
- **应用程序**：位于 0x10009000，包含主固件功能

## Flash 分区规划

当前硬件为 **2MB Flash**（后续可升级到 4MB）：

```
0x10000000 ┌─────────────────────────┐
           │   Bootloader (32KB)     │  两级启动第一级
0x10008000 ├─────────────────────────┤
           │   DFU 元数据 (4KB)      │  升级标志、CRC、版本、回滚信息
0x10009000 ├─────────────────────────┤
           │                         │
           │   应用程序区 (~1948KB)  │  主固件
           │                         │
0x101F0000 ├─────────────────────────┤
           │   用户配置区 (64KB)     │  配置存储（保持不变）
0x101FFFFF └─────────────────────────┘
```

> 升级到 4MB Flash 后，只需修改 `pico_flash_region.ld` 中的 LENGTH 和 `dfu_metadata.h` 中的 FLASH_SIZE，应用程序区会自动扩大。

## 文件结构

```
firmware/
├── bootloader/                    # Bootloader 独立项目
│   ├── CMakeLists.txt
│   ├── bootloader.ld              # Bootloader 链接脚本（32KB Flash）
│   ├── include/
│   │   └── dfu_handler.h
│   └── src/
│       ├── main.c                 # Bootloader 主程序
│       ├── dfu_handler.c          # DFU 协议处理
│       └── usb_descriptors.c      # Bootloader USB 描述符
├── include/device/dfu/
│   └── dfu_metadata.h             # 共享：DFU 元数据结构和命令定义
├── src/device/dfu/
│   └── dfu_metadata.c             # 共享：元数据读写、CRC、跳转
├── app.ld                         # 应用程序链接脚本（从 0x10009000 开始）
├── scripts/
│   └── merge_firmware.py          # UF2 合并脚本
└── build_all.bat                  # 一键编译脚本
```

## 编译方法

### 方法一：一键编译（推荐）

```bash
cd firmware
build_all.bat
```

输出文件：
- `bootloader/build/pico_hid_bootloader.uf2` - Bootloader
- `build/pico_hid_composite.uf2` - 应用程序（用于 DFU 升级）
- `build/pico_hid_composite_full.uf2` - 完整固件（Bootloader + 应用）

### 方法二：分别编译

```bash
# 编译应用程序
cd firmware/build
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
nmake

# 编译 Bootloader
cd firmware/bootloader/build
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
nmake

# 合并
cd firmware
python scripts/merge_firmware.py bootloader/build/pico_hid_bootloader.uf2 build/pico_hid_composite.uf2 build/pico_hid_composite_full.uf2
```

## 首次烧录

1. 按住 Pico 2 的 BOOTSEL 按钮，插入 USB
2. 将 `build/pico_hid_composite_full.uf2` 拖入 Pico 磁盘
3. 设备自动重启，进入应用程序

## DFU 升级流程

### 上位机升级步骤

1. 上位机连接设备（应用模式，PID 0x4004）
2. 发送 `CMD_ENTER_DFU` (0x04) 控制命令（需连续 3 次，5 秒内）
3. 设备重启，进入 Bootloader DFU 模式（PID 0x4005）
4. 上位机重新连接 Bootloader（VID 0xCafe, PID 0x4005）
5. 发送擦除命令（Report ID 0x31，4 字节固件大小）
6. 分块发送固件数据（Report ID 0x32，4 字节偏移 + 58 字节数据）
7. 发送校验命令（Report ID 0x33，4 字节大小 + 4 字节 CRC32）
8. 发送完成命令（Report ID 0x34，4 字节版本号）
9. 设备自动重启，进入新固件

### DFU HID 协议

| Report ID | 方向 | 说明 | 数据格式 |
|-----------|------|------|----------|
| 0x30 | GET/SET | 状态查询/响应 | 见 dfu_status_response_t |
| 0x31 | SET | 擦除应用区 | uint32 size |
| 0x32 | SET | 写入数据块 | uint32 offset + 58 bytes data |
| 0x33 | SET | CRC 校验 | uint32 size + uint32 crc32 |
| 0x34 | SET | 完成升级 | uint32 version |
| 0x35 | SET | 中止升级 | - |

### 状态响应结构（Report ID 0x30）

```c
typedef struct {
    uint8_t  state;          // 当前状态
    uint8_t  error;          // 错误码
    uint16_t reserved;
    uint32_t write_offset;   // 当前写入偏移
    uint32_t expected_size;  // 期望固件大小
    uint32_t received_crc;   // 已接收数据 CRC
    uint8_t  version_major;  // Bootloader 版本
    uint8_t  version_minor;
    uint16_t reserved2;
    uint32_t app_start;      // 应用起始地址 0x10009000
    uint32_t app_max_size;   // 应用最大大小
} dfu_status_response_t;
```

## 安全机制

### 1. 配置锁定
- 应用程序默认锁定配置写入
- `CMD_ENTER_DFU` 需连续 3 次确认（5 秒内），防止恶意 DoS

### 2. CRC32 校验
- 升级完成后校验整个固件 CRC32
- 校验失败不切换到新固件

### 3. 回滚机制
- 升级前保存旧固件信息（大小、CRC、版本）
- 新固件首次启动失败（5 秒内无活动）自动回滚
- 回滚通过元数据区的旧固件信息实现

### 4. 断电保护
- 升级中断电，Bootloader 检测到无效应用后进入 DFU 模式
- 可重新发送固件完成升级

## 固件版本号

- USB 设备描述符 `bcdDevice` = 0x0300（v3.0.0）
- 可通过上位机读取设备描述符获取版本
- DFU 完成命令中传入新版本号，存储在元数据区

## 注意事项

1. **首次烧录必须使用完整固件**（`pico_hid_composite_full.uf2`），否则设备无法启动
2. **应用程序 UF2**（`pico_hid_composite.uf2`）仅用于 DFU 升级，不能直接通过 BOOTSEL 烧录
3. Bootloader 编译需要 Pico SDK 2.3.0
4. 升级过程中不要断电，否则需重新进入 DFU 模式升级
5. 用户配置区（64KB）在升级过程中保持不变

## 上位机集成建议

1. 添加"固件升级"页面
2. 选择 `.uf2` 文件后，解析固件大小和计算 CRC32
3. 按上述流程发送命令
4. 显示升级进度（写入偏移 / 总大小）
5. 升级完成后自动重连设备
