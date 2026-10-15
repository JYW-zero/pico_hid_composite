namespace HidConfigTool.Core.Models;

using System.Text.Json.Serialization;

/// <summary>
/// 设备配置
/// </summary>
public class DeviceConfig
{
    /// <summary>
    /// 配置版本
    /// </summary>
    public int Version { get; set; } = 8;

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
    /// 摇杆灵敏度（倍率，1.0=正常）
    /// </summary>
    public double JoystickSensitivity { get; set; } = 1.0;

    /// <summary>
    /// 摇杆X轴反转
    /// </summary>
    public bool JoystickInvertX { get; set; } = false;

    /// <summary>
    /// 摇杆Y轴反转
    /// </summary>
    public bool JoystickInvertY { get; set; } = false;

    /// <summary>
    /// 摇杆 X 轴中心点（ADC 值，0-4095，v5 新增）
    /// </summary>
    public ushort JoystickCenterX { get; set; } = 2048;

    /// <summary>
    /// 摇杆 Y 轴中心点（ADC 值，0-4095，v5 新增）
    /// </summary>
    public ushort JoystickCenterY { get; set; } = 2048;

    /// <summary>
    /// 摇杆是否已校准（v5 新增）
    /// </summary>
    public bool JoystickCalibrated { get; set; } = false;

    /// <summary>
    /// 4档 DPI 值（v6 新增，每档 100-6400）
    /// </summary>
    public ushort[] DpiSlots { get; set; } = new ushort[] { 400, 800, 1600, 3200 };

    /// <summary>
    /// 当前 DPI 档位（0-3，v6 新增）
    /// </summary>
    public byte CurrentDpiSlot { get; set; } = 2;

    /// <summary>
    /// LOD 静默高度等级（0=低,1=中,2=高，v6 新增）
    /// </summary>
    public byte LodLevel { get; set; } = 1;

    /// <summary>
    /// 角度捕捉开关（v6 新增）
    /// </summary>
    public bool AngleSnapping { get; set; } = false;

    /// <summary>
    /// DPI 切换按键索引（0xFF=默认，v6 新增）
    /// </summary>
    public byte DpiSwitchKey { get; set; } = 0xFF;

    /// <summary>
    /// Layer2 按键映射（64键，v7 新增）
    /// </summary>
    public byte[] Layer2Keymap { get; set; } = new byte[64];

    /// <summary>
    /// Layer3 按键映射（64键，v7 新增）
    /// </summary>
    public byte[] Layer3Keymap { get; set; } = new byte[64];

    /// <summary>
    /// 性能监控任务超时阈值（8项，单位100us，0=禁用告警；v8 新增，仅 dev 固件有意义）
    /// </summary>
    public ushort[] PerfThresholds { get; set; } = new ushort[8];

    /// <summary>
    /// 编码器方向是否反转
    /// </summary>
    public bool EncoderReverse { get; set; } = false;

    /// <summary>
    /// 编码器每格步数
    /// </summary>
    public int EncoderStepsPerTick { get; set; } = 1;

    /// <summary>
    /// 滚轮速度
    /// </summary>
    public int EncoderScrollSpeed { get; set; } = 3;

    /// <summary>
    /// 普通层按键映射（64 键）
    /// </summary>
    public byte[] Keymap { get; set; } = new byte[64];

    /// <summary>
    /// Fn 层按键映射（64 键）
    /// </summary>
    public byte[] FnKeymap { get; set; } = new byte[64];

    /// <summary>
    /// 版本警告标志（不序列化）：加载配置文件时如果版本高于当前支持版本则为 true
    /// </summary>
    [JsonIgnore]
    public bool HasVersionWarning { get; set; }
}
