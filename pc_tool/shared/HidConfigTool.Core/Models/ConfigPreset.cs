namespace HidConfigTool.Core.Models;

using System.Text.Json.Serialization;

/// <summary>
/// 配置预设
/// </summary>
public class ConfigPreset
{
    /// <summary>
    /// 预设唯一标识
    /// </summary>
    public string Id { get; set; } = Guid.NewGuid().ToString();

    /// <summary>
    /// 预设名称
    /// </summary>
    public string Name { get; set; } = string.Empty;

    /// <summary>
    /// 预设描述
    /// </summary>
    public string Description { get; set; } = string.Empty;

    /// <summary>
    /// 预设分类（办公、游戏、设计等）
    /// </summary>
    public string Category { get; set; } = "自定义";

    /// <summary>
    /// 是否为内置预设（不可删除）
    /// </summary>
    public bool IsBuiltIn { get; set; } = false;

    /// <summary>
    /// 预设图标（emoji 或图标名称）
    /// </summary>
    public string Icon { get; set; } = "📋";

    /// <summary>
    /// 创建时间
    /// </summary>
    public DateTime CreatedAt { get; set; } = DateTime.Now;

    /// <summary>
    /// 最后修改时间
    /// </summary>
    public DateTime ModifiedAt { get; set; } = DateTime.Now;

    /// <summary>
    /// 预设配置数据
    /// </summary>
    public DeviceConfig Config { get; set; } = new DeviceConfig();

    /// <summary>
    /// 获取预设摘要信息（用于预览）
    /// </summary>
    [JsonIgnore]
    public string Summary
    {
        get
        {
            var parts = new List<string>
            {
                $"DPI: {Config.Dpi}"
            };

            if (Config.AccelerationEnabled)
                parts.Add("加速开启");

            if (Config.JoystickDeadzone > 0)
                parts.Add($"摇杆死区: {Config.JoystickDeadzone}");

            if (Config.EncoderReverse)
                parts.Add("编码器反转");

            return string.Join(" | ", parts);
        }
    }

    /// <summary>
    /// 创建预设的副本
    /// </summary>
    public ConfigPreset Clone()
    {
        return new ConfigPreset
        {
            Id = Guid.NewGuid().ToString(),
            Name = Name,
            Description = Description,
            Category = Category,
            IsBuiltIn = false,
            Icon = Icon,
            CreatedAt = DateTime.Now,
            ModifiedAt = DateTime.Now,
            Config = new DeviceConfig
            {
                Version = Config.Version,
                Dpi = Config.Dpi,
                DpiIndex = Config.DpiIndex,
                DpiLevels = (int[])Config.DpiLevels.Clone(),
                AccelerationEnabled = Config.AccelerationEnabled,
                AccelerationThreshold = Config.AccelerationThreshold,
                AccelerationRatio = Config.AccelerationRatio,
                JoystickDeadzone = Config.JoystickDeadzone,
                JoystickSensitivity = Config.JoystickSensitivity,
                JoystickInvertX = Config.JoystickInvertX,
                JoystickInvertY = Config.JoystickInvertY,
                JoystickCenterX = Config.JoystickCenterX,
                JoystickCenterY = Config.JoystickCenterY,
                JoystickCalibrated = Config.JoystickCalibrated,
                DpiSlots = (ushort[])Config.DpiSlots.Clone(),
                CurrentDpiSlot = Config.CurrentDpiSlot,
                LodLevel = Config.LodLevel,
                AngleSnapping = Config.AngleSnapping,
                DpiSwitchKey = Config.DpiSwitchKey,
                Layer2Keymap = (byte[])Config.Layer2Keymap.Clone(),
                Layer3Keymap = (byte[])Config.Layer3Keymap.Clone(),
                EncoderReverse = Config.EncoderReverse,
                EncoderStepsPerTick = Config.EncoderStepsPerTick,
                EncoderScrollSpeed = Config.EncoderScrollSpeed,
                Keymap = (byte[])Config.Keymap.Clone(),
                FnKeymap = (byte[])Config.FnKeymap.Clone()
            }
        };
    }
}
