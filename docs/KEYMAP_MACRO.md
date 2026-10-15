# 多层按键映射与宏功能文档

## 一、功能概述

实现 4 层按键映射和完整的宏功能，支持通过 Fn 键临时切换层，通过层切换键持久切换层。

## 二、4 层按键映射

### 2.1 层定义

| 层 | 索引 | 说明 | 切换方式 |
|----|------|------|---------|
| Layer 0 | 0 | 普通层（默认） | 持久活跃层 |
| Layer 1 | 1 | Fn 临时层 | 按住 Fn 键时临时激活 |
| Layer 2 | 2 | 扩展层 1 | 持久活跃层 |
| Layer 3 | 3 | 扩展层 2 | 持久活跃层 |

### 2.2 层切换机制

**临时切换（Fn 键）**：
- 按住 Fn 键时，临时使用 Layer 1 的映射
- 松开 Fn 键后，恢复到当前持久活跃层
- Fn 键本身不输出任何键码

**持久切换（层切换键）**：
- 在映射表中设置层切换键（0xFA-0xFD）
- 按下层切换键时，切换到对应层并保持
- 支持循环切换（0xF9）：0 → 2 → 3 → 0

**层回退机制**：
- 如果当前层（非 Layer 0）的某个键为空，则自动回退到 Layer 0 的映射
- 这样可以只在扩展层中定义差异键，其他键继承 Layer 0

### 2.3 层切换键码

| 键码 | 名称 | 功能 |
|------|------|------|
| 0xFD | KEYMAP_KEY_LAYER_0 | 切换到 Layer 0 |
| 0xFC | KEYMAP_KEY_LAYER_1 | 切换到 Layer 1 |
| 0xFB | KEYMAP_KEY_LAYER_2 | 切换到 Layer 2 |
| 0xFA | KEYMAP_KEY_LAYER_3 | 切换到 Layer 3 |
| 0xF9 | KEYMAP_KEY_LAYER_NEXT | 循环切换到下一层 |

### 2.4 配置存储

配置结构（v7）中存储 4 层映射表：

| 字段 | 大小 | 说明 |
|------|------|------|
| `keymap[64]` | 64 字节 | Layer 0 映射表 |
| `fn_keymap[64]` | 64 字节 | Layer 1 映射表 |
| `layer2_keymap[64]` | 64 字节 | Layer 2 映射表 |
| `layer3_keymap[64]` | 64 字节 | Layer 3 映射表 |

**配置版本**：`CONFIG_VERSION = 0x0007`（v7）
**配置大小**：1492 字节（v6 1364 + 128 字节）

### 2.5 API 接口

```c
/* 查询按键映射（自动处理层和 Fn 键） */
bool keymap_lookup(uint8_t key_index, bool fn_pressed, keymap_result_t* out_result);

/* 直接查询指定层的按键映射 */
bool keymap_lookup_layer(uint8_t key_index, uint8_t layer, keymap_result_t* out_result);

/* 获取/设置当前持久活跃层 */
uint8_t keymap_get_active_layer(void);
void keymap_set_active_layer(uint8_t layer);

/* 循环切换到下一层 */
void keymap_cycle_layer(void);

/* 获取/设置指定层的映射表 */
const uint8_t* keymap_get_layer_table(uint8_t layer);
void keymap_set_layer_table(uint8_t layer, const uint8_t* table);
```

## 三、宏功能

### 3.1 宏配置

| 参数 | 值 | 说明 |
|------|---|------|
| 最大宏数量 | 8 | MACRO_MAX_COUNT |
| 每宏最大动作数 | 32 | MACRO_MAX_ACTIONS |
| 宏名称最大长度 | 16 | MACRO_NAME_MAX_LEN |

### 3.2 宏动作类型

| 类型 | 值 | 说明 | 参数 |
|------|---|------|------|
| KEY_DOWN | 0 | 按键按下 | param1=键码 |
| KEY_UP | 1 | 按键释放 | param1=键码 |
| DELAY | 2 | 延迟 | param1-2=延迟ms（小端） |
| MOUSE_MOVE | 3 | 鼠标移动 | param1-2=X, param3=Y |
| MOUSE_CLICK | 4 | 鼠标点击 | param1=按钮位图 |
| MOUSE_SCROLL | 5 | 鼠标滚轮 | param1=滚轮值 |
| KEY_PRESS | 6 | 按键点击（按下+释放） | param1=键码 |
| TEXT_CHAR | 7 | 输入字符 | param1=ASCII码 |

### 3.3 宏执行特性

- **两阶段状态机**：KEY_PRESS 和 TEXT_CHAR 分两帧执行（按下→释放），确保主机正确识别
- **循环执行**：`repeat_count` 控制循环次数，0=无限循环（直到松开触发键）
- **独立键盘状态**：宏的按键状态与物理按键合并，不影响正常键盘输入
- **鼠标按钮合并**：宏的鼠标按钮与物理鼠标按钮通过按位或合并

### 3.4 宏存储

宏配置存储在配置结构的 `macro_data[1184]` 中：
- 8 个宏 × 148 字节/宏 = 1184 字节
- 每个宏：id(1) + trigger_key(1) + repeat_count(1) + action_count(1) + name(16) + actions(32×4=128) = 148 字节

### 3.5 API 接口

```c
/* 触发/停止宏 */
bool macro_trigger(uint8_t macro_id);
void macro_stop(uint8_t macro_id);
void macro_stop_all(void);

/* 根据触发键查找宏 */
uint8_t macro_find_by_trigger_key(uint8_t key_index);

/* 获取宏的键盘/鼠标状态（用于和物理按键合并） */
void macro_get_keyboard_state(uint8_t* out_modifier, uint8_t out_keys[6]);
uint8_t macro_get_mouse_buttons(void);

/* 配置持久化 */
int macro_load_from_config(const uint8_t* data, uint32_t len);
int macro_save_to_config(uint8_t* data, uint32_t len);
```

## 四、上位机集成

### 4.1 配置大小

- **v7 配置大小**：1492 字节
- **OFFSET_CRC32**：1488
- **Layer2 偏移**：1360
- **Layer3 偏移**：1424

### 4.2 按键设置页面

按键设置页面应支持：
1. 层选择下拉框（Layer 0/1/2/3）
2. 64 键映射表编辑
3. 层切换键配置（将某个键设置为层切换键）
4. 当前活跃层显示

### 4.3 宏配置页面

宏配置页面应支持：
1. 宏列表（8 个宏）
2. 宏名称编辑
3. 触发键设置
4. 动作列表编辑（添加/删除/排序）
5. 循环次数设置
6. 宏测试功能

## 五、修改文件清单

| 类型 | 文件 | 说明 |
|------|------|------|
| 修改 | `include/app/keymap.h` | 4 层映射接口定义 |
| 修改 | `src/app/keymap.c` | 4 层映射实现 |
| 修改 | `include/board/config.h` | 配置版本 v7，新增 Layer2/3 映射表 |
| 修改 | `src/board/config.c` | 默认配置 + v6→v7 迁移 |
| 修改 | `src/app/main.c` | 层切换键处理（边沿检测） |
| 修改 | `shared/HidConfigTool.Core/Services/DeviceService.cs` | 配置大小 + 序列化/反序列化 |
| 修改 | `shared/HidConfigTool.Core/Models/DeviceConfig.cs` | 新增 Layer2/Layer3 字段 |

## 六、验证

- ✅ 固件编译通过
- ✅ 上位机编译通过
- ✅ 4 层按键映射实现
- ✅ 层切换键支持（边沿检测）
- ✅ 层回退机制（空键回退到 Layer 0）
- ✅ 配置版本 v7，支持 v6→v7 自动迁移
- ✅ 宏功能完整（8 宏 × 32 动作，按键序列/延迟/循环）
- ✅ 宏执行不影响正常键盘输入（独立状态合并）
- ✅ 上位机支持 4 层配置读写
