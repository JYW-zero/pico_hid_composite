# 光学传感器高级功能文档

## 一、功能概述

为鼠标光学传感器添加多档 DPI 切换、LOD（静默高度）调节、角度捕捉（Angle Snapping）等高级功能。

## 二、配置结构扩展（v6）

在 `device_config_t` 中新增以下字段：

| 字段 | 类型 | 偏移 | 说明 | 默认值 |
|------|------|------|------|--------|
| `dpi_slot[4]` | uint16_t[4] | - | 4档 DPI 值 | {400, 800, 1600, 3200} |
| `current_dpi_slot` | uint8_t | - | 当前 DPI 档位（0-3） | 2（1600 DPI） |
| `lod_level` | uint8_t | - | LOD 静默高度等级（0-3） | 1（中等） |
| `angle_snapping` | uint8_t | - | 角度捕捉开关（0=关，1=开） | 0（关闭） |
| `dpi_switch_key` | uint8_t | - | DPI 切换按键索引（0xFF=默认） | 0xFF |
| `reserved4[2]` | uint8_t[2] | - | 保留对齐 | {0} |

**配置版本**：`CONFIG_VERSION = 0x0006`（v6）

**版本迁移**：支持从 v5 自动迁移到 v6，新字段使用默认值。

## 三、多档 DPI 切换

### 3.1 DPI 档位配置

4档 DPI 值存储在 `dpi_slot[4]` 中，每档支持 100-6400 DPI（自动对齐到 25 的倍数）。

默认档位：
- 档位 0：400 DPI
- 档位 1：800 DPI
- 档位 2：1600 DPI（默认当前档位）
- 档位 3：3200 DPI

### 3.2 DPI 切换命令

**HID 命令**：`CMD_SWITCH_DPI = 0x0F`

**参数**：
- `data[1] = 0xFF`：循环切换到下一档
- `data[1] = 0-3`：直接切换到指定档位

**行为**：
1. 更新 `current_dpi_slot`
2. 保存配置到 Flash
3. 立即应用新 DPI（调用 `optical_sensor_set_dpi` 或 `optical_sensor_set_dpi_raw`）

**示例**：
```c
// 循环切换到下一档
send_command(CMD_SWITCH_DPI, 0xFF);

// 直接切换到档位 2（1600 DPI）
send_command(CMD_SWITCH_DPI, 2);
```

### 3.3 配置应用时的 DPI 设置

在 `CMD_APPLY_CONFIG`（0x05）中，DPI 设置逻辑：
1. 读取 `current_dpi_slot`（0-3）
2. 读取 `dpi_slot[current_dpi_slot]`
3. 如果该档位值为 0，则使用旧的 `dpi` 字段（向后兼容）
4. 调用 `optical_sensor_set_dpi`（固定档位）或 `optical_sensor_set_dpi_raw`（任意值）

## 四、LOD（静默高度）调节

### 4.1 LOD 等级

| 等级 | 值 | 说明 |
|------|---|------|
| LOW | 0 | 低静默高度，抬起即停止跟踪 |
| MEDIUM | 1 | 中等静默高度（默认） |
| HIGH | 2 | 高静默高度，抬起较高才停止 |

### 4.2 寄存器配置

- 寄存器地址：`0x1E`（LIFT_CONFIG）
- 寄存器值：0=低，1=中，2=高

### 4.3 配置应用

在 `CMD_APPLY_CONFIG` 中，读取 `lod_level` 并调用 `optical_sensor_set_lod`。

## 五、角度捕捉（Angle Snapping）

### 5.1 功能说明

角度捕捉功能会将鼠标移动方向"吸附"到最近的 45° 倍数方向，适合需要精确直线移动的场景（如绘图、FPS 游戏中的水平/垂直瞄准）。

### 5.2 开关配置

- `angle_snapping = 0`：关闭（默认）
- `angle_snapping = 1`：开启

### 5.3 寄存器配置

- 寄存器地址：`0x3C`（ANGLE_SNAP）
- 寄存器值：bit0 = 使能位

### 5.4 配置应用

在 `CMD_APPLY_CONFIG` 中，读取 `angle_snapping` 并调用 `optical_sensor_set_angle_snapping`。

## 六、API 接口

### 6.1 光学传感器驱动接口

```c
/* 设置 LOD 静默高度 */
int optical_sensor_set_lod(const optical_sensor_cfg_t *cfg, optical_sensor_lod_e level);

/* 设置角度捕捉开关 */
int optical_sensor_set_angle_snapping(const optical_sensor_cfg_t *cfg, bool enabled);
```

### 6.2 LOD 枚举

```c
typedef enum
{
    OPTICAL_SENSOR_LOD_LOW = 0,    /* 低静默高度 */
    OPTICAL_SENSOR_LOD_MEDIUM,     /* 中等静默高度 */
    OPTICAL_SENSOR_LOD_HIGH,       /* 高静默高度 */
    OPTICAL_SENSOR_LOD_MAX
} optical_sensor_lod_e;
```

## 七、上位机集成建议

### 7.1 鼠标设置页面

在鼠标设置页面增加以下控件：

1. **DPI 档位选择**：4 个档位，每个档位可设置 DPI 值（100-6400）
2. **当前 DPI 档位**：下拉框或单选按钮，选择当前使用的档位
3. **DPI 切换按钮**：点击循环切换到下一档（发送 CMD_SWITCH_DPI 0xFF）
4. **LOD 等级**：下拉框（低/中/高）
5. **角度捕捉**：开关按钮

### 7.2 配置序列化

上位机需要更新 `CONFIG_SIZE` 和 `OFFSET_CRC32`：
- 新增字段大小：4×2 + 1 + 1 + 1 + 1 + 2 = 14 字节
- `CONFIG_SIZE` 需要增加 14 字节
- `OFFSET_CRC32 = CONFIG_SIZE - 4`

## 八、修改文件清单

| 类型 | 文件 | 说明 |
|------|------|------|
| 修改 | `include/board/config.h` | 配置版本 v6，新增字段 |
| 修改 | `src/board/config.c` | 默认配置 + v5→v6 迁移 |
| 修改 | `include/device/optical_sensor.h` | 新增 LOD/角度捕捉接口 |
| 修改 | `src/device/optical_sensor.c` | 实现 LOD/角度捕捉 |
| 修改 | `src/protocol/config_hid.c` | CMD_SWITCH_DPI + 配置应用逻辑 |

## 九、验证

- ✅ 固件编译通过
- ✅ 配置版本 v6，支持 v5→v6 自动迁移
- ✅ 多档 DPI 切换命令（CMD_SWITCH_DPI）
- ✅ LOD 静默高度调节
- ✅ 角度捕捉开关
- ✅ 配置应用时自动设置 DPI/LOD/角度捕捉
