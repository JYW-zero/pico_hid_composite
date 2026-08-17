namespace HidConfigTool.Core.Models;

/// <summary>
/// 设备信息
/// </summary>
public class DeviceInfo
{
    /// <summary>
    /// 设备 VID
    /// </summary>
    public ushort VendorId { get; set; }

    /// <summary>
    /// 设备 PID
    /// </summary>
    public ushort ProductId { get; set; }

    /// <summary>
    /// 设备路径
    /// </summary>
    public string DevicePath { get; set; } = string.Empty;

    /// <summary>
    /// 产品名称
    /// </summary>
    public string ProductName { get; set; } = string.Empty;

    /// <summary>
    /// 制造商名称
    /// </summary>
    public string ManufacturerName { get; set; } = string.Empty;

    /// <summary>
    /// 序列号
    /// </summary>
    public string SerialNumber { get; set; } = string.Empty;

    /// <summary>
    /// 固件版本
    /// </summary>
    public Version FirmwareVersion { get; set; } = new(0, 0, 0);

    /// <summary>
    /// 硬件版本
    /// </summary>
    public Version HardwareVersion { get; set; } = new(0, 0, 0);

    /// <summary>
    /// 是否连接
    /// </summary>
    public bool IsConnected { get; set; }
}
