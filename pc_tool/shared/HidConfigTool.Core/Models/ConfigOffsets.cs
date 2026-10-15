/*
 * ConfigOffsets.cs
 * 配置结构偏移量常量定义
 *
 * 与固件 firmware/include/board/config.h 中的 device_config_t 结构体一一对应。
 * 修改固件配置结构体时，必须同步更新此文件。
 *
 * 版本历史：
 *   v1: 基础配置（magic, version, dpi, deadzone, encoder_reverse, seq, keymap, fn_keymap）
 *   v2: 新增 macro_data（1280字节）
 *   v3: 新增摇杆/编码器扩展字段（joy_invert, enc_steps, joy_sensitivity）
 *   v4: 新增 length, write_count
 *   v5: 新增摇杆校准数据（center_x, center_y, calibrated）
 *   v6: 新增光学传感器高级配置（dpi_slot, current_dpi_slot, lod, angle_snapping, dpi_switch_key）
 *   v7: 新增4层按键映射（layer2_keymap, layer3_keymap）
 *   v8: 新增性能监控阈值（perf_thresholds[8]）+ reserved5，crc32 顺延至 1506
 */

namespace HidConfigTool.Core.Models;

/// <summary>
/// 配置结构偏移量常量
/// 与固件 device_config_t 结构体保持同步，用于二进制配置数据的解析和生成
/// </summary>
public static class ConfigOffsets
{
    /// <summary>配置魔数偏移（uint32，值为 0x5A5A5A5A，与固件 CONFIG_MAGIC 一致）</summary>
    public const int Magic = 0;

    /// <summary>配置版本号偏移（uint16，当前版本 8）</summary>
    public const int Version = 4;

    /// <summary>DPI 值偏移（uint16，范围 100-6400）</summary>
    public const int Dpi = 6;

    /// <summary>摇杆死区偏移（uint16，范围 0-4095）</summary>
    public const int JoystickDeadzone = 8;

    /// <summary>编码器方向反转偏移（uint8，0=正常，1=反转）</summary>
    public const int EncoderReverse = 10;

    /// <summary>配置序列号偏移（uint16，用于双备份新旧判断）</summary>
    public const int Seq = 11;

    /// <summary>保留字段偏移（uint8[2]，对齐填充）</summary>
    public const int Reserved1 = 13;

    /// <summary>主键映射偏移（uint8[64]，64个按键的 HID 码）</summary>
    public const int Keymap = 14;

    /// <summary>Fn 层按键映射偏移（uint8[64]，Fn 键按下时的映射）</summary>
    public const int FnKeymap = 78;

    /// <summary>宏数据偏移（uint8[1280]，8个宏，每个宏 160 字节）</summary>
    public const int MacroData = 142;

    // ==================== v3 新增字段 ====================

    /// <summary>摇杆 X 轴反转偏移（uint8，0=正常，1=反转）</summary>
    public const int JoyInvertX = 1326;

    /// <summary>摇杆 Y 轴反转偏移（uint8，0=正常，1=反转）</summary>
    public const int JoyInvertY = 1327;

    /// <summary>编码器每格步数偏移（uint8，1/2/4 步）</summary>
    public const int EncoderSteps = 1328;

    /// <summary>编码器滚轮方向偏移（uint8，0=正常，1=反转）</summary>
    public const int EncoderScroll = 1329;

    /// <summary>摇杆灵敏度偏移（float，范围 0.1-10.0）</summary>
    public const int JoystickSensitivity = 1330;

    /// <summary>保留字段偏移（uint8[2]，对齐填充）</summary>
    public const int Reserved2 = 1334;

    // ==================== v4 新增字段 ====================

    /// <summary>配置结构总长度偏移（uint16，字节数）</summary>
    public const int Length = 1334;

    /// <summary>总写入次数偏移（uint16，Flash 磨损统计）</summary>
    public const int WriteCount = 1336;

    // ==================== v5 新增字段 ====================

    /// <summary>摇杆 X 轴中心点偏移（uint16，ADC 值 0-4095，默认 2048）</summary>
    public const int JoystickCenterX = 1340;

    /// <summary>摇杆 Y 轴中心点偏移（uint16，ADC 值 0-4095，默认 2048）</summary>
    public const int JoystickCenterY = 1342;

    /// <summary>摇杆是否已校准偏移（uint8，0=未校准，1=已校准）</summary>
    public const int JoystickCalibrated = 1344;

    /// <summary>保留字段偏移（uint8[1]，对齐填充，复用于编码器模式存储）</summary>
    public const int Reserved3 = 1345;

    // ==================== v6 新增字段 ====================

    /// <summary>DPI 档位偏移（uint16[4]，4档 DPI 值，默认 400/800/1600/3200）</summary>
    public const int DpiSlot = 1346;

    /// <summary>当前 DPI 档位偏移（uint8，0-3，默认 2=1600）</summary>
    public const int CurrentDpiSlot = 1354;

    /// <summary>LOD 静默高度等级偏移（uint8，0-3，默认 1）</summary>
    public const int LodLevel = 1355;

    /// <summary>角度捕捉开关偏移（uint8，0=关闭，1=开启）</summary>
    public const int AngleSnapping = 1356;

    /// <summary>DPI 切换按键索引偏移（uint8，0xFF=默认按键）</summary>
    public const int DpiSwitchKey = 1357;

    /// <summary>保留字段偏移（uint8[2]，对齐填充）</summary>
    public const int Reserved4 = 1358;

    // ==================== v7 新增字段 ====================

    /// <summary>Layer2 按键映射偏移（uint8[64]，第3层按键映射）</summary>
    public const int Layer2Keymap = 1360;

    /// <summary>Layer3 按键映射偏移（uint8[64]，第4层按键映射）</summary>
    public const int Layer3Keymap = 1424;

    // ==================== v8 新增字段 ====================

    /// <summary>性能监控任务超时阈值偏移（uint16[8]，单位100us，0=禁用告警；仅 dev 固件有意义）</summary>
    public const int PerfThresholds = 1488;

    /// <summary>保留字段偏移（uint8[2]，对齐填充）</summary>
    public const int Reserved5 = 1504;

    // ==================== 校验字段 ====================

    /// <summary>CRC32 校验值偏移（uint32，在所有字段之后，1510-4）</summary>
    public const int Crc32 = 1506;

    /// <summary>配置结构总大小（字节，含 CRC32，统一从 ProtocolConstants 引用）</summary>
    public const int TotalSize = ProtocolConstants.ConfigSize;

    /// <summary>配置魔数值（统一从 ProtocolConstants 引用，避免重复定义）</summary>
    public const uint MagicValue = ProtocolConstants.ConfigMagic;

    /// <summary>当前配置版本号（统一从 ProtocolConstants 引用）</summary>
    public const ushort CurrentVersion = ProtocolConstants.ConfigVersion;
}
