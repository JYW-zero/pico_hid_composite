/*
 * ConfigMigrationService.cs
 * 配置版本迁移服务
 *
 * 负责将旧版本的设备配置迁移到当前支持的最新版本（v8）。
 * 每个版本的迁移只处理该版本新增的字段，确保向后兼容。
 *
 * 版本历史：
 *   v1: 基础配置（magic, version, dpi, deadzone, encoder_reverse, seq, keymap, fn_keymap）
 *   v2: 新增 macro_data（1280字节）
 *   v3: 新增摇杆/编码器扩展字段（joy_invert, enc_steps, joy_sensitivity）
 *   v4: 新增 length, write_count
 *   v5: 新增摇杆校准数据（center_x, center_y, calibrated）
 *   v6: 新增光学传感器高级配置（dpi_slot, current_dpi_slot, lod, angle_snapping, dpi_switch_key）
 *   v7: 新增4层按键映射（layer2_keymap, layer3_keymap）
 *   v8: 新增性能监控阈值（perf_thresholds[8]）
 */

namespace HidConfigTool.Core.Services;

using HidConfigTool.Core.Models;

/// <summary>
/// 配置版本迁移服务
/// 将旧版本配置迁移到当前支持的最新版本
/// </summary>
public class ConfigMigrationService
{
    /// <summary>
    /// 当前支持的最新配置版本（统一从 ProtocolConstants.ConfigVersion 引用）
    /// </summary>
    public const int CurrentVersion = ProtocolConstants.ConfigVersion;

    /// <summary>
    /// 检查配置是否需要迁移
    /// </summary>
    /// <param name="config">设备配置</param>
    /// <returns>true=需要迁移，false=已是最新版本</returns>
    public bool IsMigrationNeeded(DeviceConfig config)
    {
        if (config == null) return false;
        return config.Version < CurrentVersion;
    }

    /// <summary>
    /// 获取当前支持的最新版本号
    /// </summary>
    /// <returns>最新版本号</returns>
    public int GetCurrentVersion()
    {
        return CurrentVersion;
    }

    /// <summary>
    /// 将配置迁移到最新版本
    /// </summary>
    /// <param name="config">设备配置（将被修改）</param>
    /// <returns>迁移后的配置（同一对象引用）</returns>
    public DeviceConfig Migrate(DeviceConfig config)
    {
        if (config == null) return null!;

        // 按版本逐步迁移，确保每个版本的迁移逻辑都被执行
        while (config.Version < CurrentVersion)
        {
            int nextVersion = config.Version + 1;
            MigrateToVersion(config, nextVersion);
            config.Version = nextVersion;
        }

        return config;
    }

    /// <summary>
    /// 迁移到指定版本
    /// </summary>
    /// <param name="config">设备配置</param>
    /// <param name="targetVersion">目标版本</param>
    private void MigrateToVersion(DeviceConfig config, int targetVersion)
    {
        switch (targetVersion)
        {
            case 2:
                MigrateV1ToV2(config);
                break;
            case 3:
                MigrateV2ToV3(config);
                break;
            case 4:
                MigrateV3ToV4(config);
                break;
            case 5:
                MigrateV4ToV5(config);
                break;
            case 6:
                MigrateV5ToV6(config);
                break;
            case 7:
                MigrateV6ToV7(config);
                break;
            case 8:
                MigrateV7ToV8(config);
                break;
            default:
                // 未知版本，不做处理
                break;
        }
    }

    /// <summary>
    /// v1 到 v2 迁移：新增 macro_data（1280字节）
    /// 注意：DeviceConfig 模型中 macro_data 由固件管理，上位机不直接操作
    /// </summary>
    private void MigrateV1ToV2(DeviceConfig config)
    {
        // v2 新增 macro_data，上位机模型中无需特殊处理
        // 固件会自动初始化 macro_data 为默认值
    }

    /// <summary>
    /// v2 到 v3 迁移：新增摇杆/编码器扩展字段
    /// </summary>
    private void MigrateV2ToV3(DeviceConfig config)
    {
        // v3 新增字段使用默认值
        config.JoystickInvertX = false;
        config.JoystickInvertY = false;
        config.EncoderStepsPerTick = 1;
        config.JoystickSensitivity = 1.0;
    }

    /// <summary>
    /// v3 到 v4 迁移：新增 length, write_count
    /// 注意：这些字段由固件管理，上位机不直接操作
    /// </summary>
    private void MigrateV3ToV4(DeviceConfig config)
    {
        // v4 新增 length, write_count，上位机模型中无需特殊处理
        // 固件会自动初始化这些字段
    }

    /// <summary>
    /// v4 到 v5 迁移：新增摇杆校准数据
    /// </summary>
    private void MigrateV4ToV5(DeviceConfig config)
    {
        // v5 新增摇杆校准字段使用默认值
        config.JoystickCenterX = 2048;
        config.JoystickCenterY = 2048;
        config.JoystickCalibrated = false;
    }

    /// <summary>
    /// v5 到 v6 迁移：新增光学传感器高级配置
    /// </summary>
    private void MigrateV5ToV6(DeviceConfig config)
    {
        // v6 新增光学传感器高级配置字段使用默认值
        config.DpiSlots = new ushort[] { 400, 800, 1600, 3200 };
        config.CurrentDpiSlot = 2;
        config.LodLevel = 1;
        config.AngleSnapping = false;
        config.DpiSwitchKey = 0xFF;

        // 如果旧版本有 Dpi 值，同步到 DpiSlots 的当前档位
        if (config.Dpi > 0 && config.DpiIndex >= 0 && config.DpiIndex < 4)
        {
            config.DpiSlots[config.DpiIndex] = config.Dpi;
        }
    }

    /// <summary>
    /// v6 到 v7 迁移：新增4层按键映射
    /// </summary>
    private void MigrateV6ToV7(DeviceConfig config)
    {
        // v7 新增 Layer2/Layer3 按键映射使用默认值（全0=未映射）
        config.Layer2Keymap = new byte[64];
        config.Layer3Keymap = new byte[64];
    }

    /// <summary>
    /// v7 到 v8 迁移：新增性能监控阈值
    /// </summary>
    private void MigrateV7ToV8(DeviceConfig config)
    {
        // v8 新增 perf_thresholds[8]，默认全 0 = 禁用告警（仅 dev 固件使用）
        config.PerfThresholds = new ushort[8];
    }
}
