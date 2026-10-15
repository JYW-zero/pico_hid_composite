# 恢复出厂设置

## 一、概述

本项目提供两种恢复出厂设置的方式：
1. **组合键触发**：按住 Fn + Esc 键持续 3 秒
2. **上位机命令触发**：在设置页面点击"恢复默认"按钮

两种方式都会清除所有用户配置（按键映射、DPI、摇杆参数、宏定义等），恢复到出厂默认值，然后自动重启设备。

## 二、组合键触发

### 2.1 操作方法

1. 按住 **Fn 键**（默认键索引 59）
2. 同时按住 **Esc 键**（键索引 0）
3. 持续按住 **3 秒**
4. 设备会自动恢复出厂设置并重启

### 2.2 状态提示

通过串口输出可以看到操作状态：

```
[FACTORY_RESET] 检测到 Fn+Esc 组合键，长按 3 秒恢复出厂设置...
[FACTORY_RESET] 组合键释放（按下 1500 ms，未达到阈值）
[FACTORY_RESET] 组合键长按达到 3 秒，触发恢复出厂设置！
[CONFIG] 已保存到 Flash
[FACTORY_RESET] 重启设备...
```

- 组合键刚按下时：提示"长按 3 秒恢复出厂设置"
- 未达到 3 秒就释放：提示"未达到阈值"
- 达到 3 秒：触发恢复，记录故障日志，重启

### 2.3 设计考虑

| 设计点 | 说明 |
|--------|------|
| 组合键选择 | Fn + Esc，不常用，避免误触 |
| 长按时间 | 3 秒，足够用户确认意图 |
| 低电平有效 | 74HC165 读取，按下时 bit=0 |
| 优先检测 | 在主循环最开头检测，确保其他任务异常时也能触发 |
| 防止重复 | s_factory_reset_triggered 标志，触发后不再重复检测 |

## 三、上位机命令触发

### 3.1 操作方法

1. 打开上位机软件
2. 连接设备
3. 进入"设置"页面
4. 点击"恢复默认"按钮
5. 在确认对话框中点击"确定"
6. 设备会自动恢复出厂设置并重启
7. 上位机提示"恢复默认配置成功"

### 3.2 确认对话框

上位机会弹出确认对话框，防止误操作：

> **确认恢复**
> 恢复默认配置将清除所有自定义设置，确定要继续吗？

只有用户点击"确定"后才会发送命令。

### 3.3 协议定义

| 项目 | 值 |
|------|-----|
| Report ID | 7（控制命令） |
| 命令字节 | 0x02（CMD_RESET_CONFIG） |
| 参数 | 0x0000 |

命令格式：`[0x07, 0x02, 0x00, 0x00]`

### 3.4 上位机 API

```csharp
// 发送恢复出厂设置命令
public async Task<bool> ResetConfigAsync(CancellationToken cancellationToken = default)
{
    return await SendControlCommandAsync(CMD_RESET_CONFIG);
}
```

## 四、固件处理流程

### 4.1 组合键触发流程

```
主循环开头
  ↓
check_factory_reset_combo()
  ↓
读取按键状态 (shared_hw_get_keys())
  ↓
检测 Fn + Esc 是否同时按下
  ├─ 刚按下 → 记录开始时间，输出提示
  ├─ 持续按下 → 检查是否达到 3 秒
  │   ├─ 未达到 → 继续等待
  │   └─ 达到 3 秒 → 触发恢复
  │       ↓
  │       fault_record(WARN, "factory reset by key combo")
  │       ↓
  │       config_reset_default()  ← 写入默认配置到 Flash
  │       ↓
  │       延迟 100ms（确保 Flash 写入完成）
  │       ↓
  │       watchdog_reboot()  ← 重启设备
  └─ 释放 → 重置计时
```

### 4.2 上位机命令触发流程

```
收到 CMD_RESET_CONFIG (0x02)
  ↓
配置锁定检查（需先解锁）
  ↓
config_reset_default()  ← 写入默认配置到 Flash
  ↓
fault_record(WARN, "factory reset by command")
  ↓
延迟 100ms（确保 Flash 写入完成）
  ↓
watchdog_reboot()  ← 重启设备
```

### 4.3 config_reset_default() 实现

```c
void config_reset_default(void)
{
    // 1. 构造默认配置
    device_config_t default_cfg;
    memcpy(&default_cfg, &s_default_config, sizeof(device_config_t));
    memcpy(default_cfg.keymap, s_default_keymap, 64);
    memcpy(default_cfg.fn_keymap, s_default_fn_keymap, 64);
    memset(default_cfg.macro_data, 0, CONFIG_MACRO_DATA_SIZE);

    // 2. 序列号加 1（区分新旧配置）
    default_cfg.seq = (uint16_t)(s_current_seq + 1u);

    // 3. 设置新字段
    default_cfg.length = sizeof(device_config_t);
    default_cfg.write_count = s_write_count + 1u;

    // 4. 重新计算 CRC
    default_cfg.crc32 = crc32_calc(...);

    // 5. 写入 Flash（两阶段提交：先写备份区，校验后切换）
    config_save(&default_cfg);
}
```

## 五、恢复后的默认配置

| 配置项 | 默认值 |
|--------|--------|
| DPI | 800 |
| 摇杆死区 | 100 |
| 编码器方向 | 正常（不反转） |
| 按键映射 | 默认键盘布局 |
| Fn 层映射 | 默认 Fn 功能（F1-F12、多媒体键等） |
| 宏定义 | 全部清除（无宏） |
| 摇杆灵敏度 | 1.0（1000） |
| 摇杆方向 | 正常（不反转 X/Y） |
| 编码器步数 | 3 |
| 编码器滚动速度 | 2 |

## 六、安全考虑

### 6.1 防止误触

- **组合键**：Fn + Esc 不常用，需要同时按下
- **长按时间**：3 秒，足够用户确认意图
- **上位机确认**：弹出确认对话框，需要用户主动点击"确定"

### 6.2 配置锁定

上位机命令触发时，需要先解锁配置（连续发送 3 次解锁命令），防止恶意程序随意恢复出厂设置。

### 6.3 故障日志

两种方式触发恢复出厂设置时，都会记录一条 WARN 级别的故障日志：
- 组合键触发：`"factory reset triggered by key combo (Fn+Esc)"`
- 上位机命令触发：`"factory reset triggered by command"`

复位后可通过上位机的错误日志页面查看历史记录。

### 6.4 Flash 写入保护

恢复出厂设置后，延迟 100ms 再重启，确保 Flash 写入完成。延迟期间持续喂硬件看门狗，防止提前复位。

## 七、相关文件

| 文件 | 说明 |
|------|------|
| `firmware/src/app/main.c` | 组合键检测逻辑（check_factory_reset_combo） |
| `firmware/src/protocol/config_hid.c` | CMD_RESET_CONFIG 命令处理 |
| `firmware/src/board/config.c` | config_reset_default() 实现 |
| `firmware/include/board/config.h` | 配置接口定义 |
| `pc_tool/.../SettingsPageViewModel.cs` | 上位机设置页面 ViewModel |
| `pc_tool/.../SettingsPage.xaml` | 上位机设置页面 UI |
| `pc_tool/.../DeviceService.cs` | 上位机 ResetConfigAsync 实现 |

## 八、测试验证

### 8.1 组合键测试

1. 烧录固件，设备正常启动
2. 按住 Fn + Esc，观察串口输出
3. 未达到 3 秒就释放：提示"未达到阈值"，设备不重启
4. 持续按住 3 秒：设备恢复出厂设置并重启
5. 重启后验证：所有配置恢复默认值
6. 查看错误日志：能看到 "factory reset triggered by key combo" 记录

### 8.2 上位机命令测试

1. 烧录固件，设备正常启动
2. 打开上位机，连接设备
3. 修改一些配置（如 DPI、按键映射），保存
4. 进入设置页面，点击"恢复默认"
5. 在确认对话框中点击"取消"：设备不恢复
6. 再次点击"恢复默认"，点击"确定"：设备恢复并重启
7. 重启后验证：所有配置恢复默认值
8. 查看错误日志：能看到 "factory reset triggered by command" 记录
