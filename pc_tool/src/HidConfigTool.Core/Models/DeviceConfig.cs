namespace HidConfigTool.Core.Models;

/// <summary>
/// 设备配置
/// </summary>
public class DeviceConfig
{
    /// <summary>
    /// 配置版本
    /// </summary>
    public int Version { get; set; } = 1;

    /// <summary>
    /// 当前 DPI 值
    /// </summary>
    public ushort Dpi { get; set; } = 800;

    /// <summary>
    /// 当前 DPI 档位索引
    /// </summary>
    public int DpiIndex { get; set; } = 1;

    /// <summary>
    /// DPI 档位列表（4 档）
    /// </summary>
    public int[] DpiLevels { get; set; } = new[] { 400, 800, 1600, 3200 };

    /// <summary>
    /// 指针加速是否开启
    /// </summary>
    public bool AccelerationEnabled { get; set; } = false;

    /// <summary>
    /// 指针加速阈值
    /// </summary>
    public double AccelerationThreshold { get; set; } = 10;

    /// <summary>
    /// 指针加速比
    /// </summary>
    public double AccelerationRatio { get; set; } = 1.5;

    /// <summary>
    /// 摇杆死区（ADC 原始值）
    /// </summary>
    public ushort JoystickDeadzone { get; set; } = 100;

    /// <summary>
    /// 编码器方向是否反转
    /// </summary>
    public bool EncoderReverse { get; set; } = false;

    /// <summary>
    /// 普通层按键映射（64 键）
    /// </summary>
    public byte[] Keymap { get; set; } = new byte[64];

    /// <summary>
    /// Fn 层按键映射（64 键）
    /// </summary>
    public byte[] FnKeymap { get; set; } = new byte[64];
}
