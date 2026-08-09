namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// HID 驱动接口
/// </summary>
public interface IHidDriver : IDisposable
{
    /// <summary>
    /// 设备 VID
    /// </summary>
    ushort VendorId { get; }

    /// <summary>
    /// 设备 PID
    /// </summary>
    ushort ProductId { get; }

    /// <summary>
    /// 是否已连接
    /// </summary>
    bool IsConnected { get; }

    /// <summary>
    /// 查找所有匹配的 HID 设备
    /// </summary>
    Task<IReadOnlyList<HidDeviceInfo>> FindDevicesAsync(ushort vendorId, ushort productId);

    /// <summary>
    /// 打开设备
    /// </summary>
    Task<bool> OpenAsync(string devicePath);

    /// <summary>
    /// 关闭设备
    /// </summary>
    void Close();

    /// <summary>
    /// 发送 Feature 报告
    /// </summary>
    Task<bool> SendFeatureReportAsync(byte reportId, byte[] data);

    /// <summary>
    /// 读取 Feature 报告
    /// </summary>
    Task<byte[]?> GetFeatureReportAsync(byte reportId);
}

/// <summary>
/// HID 设备信息
/// </summary>
public class HidDeviceInfo
{
    public string DevicePath { get; set; } = string.Empty;
    public ushort VendorId { get; set; }
    public ushort ProductId { get; set; }
    public string ProductName { get; set; } = string.Empty;
    public string ManufacturerName { get; set; } = string.Empty;
    public string SerialNumber { get; set; } = string.Empty;
    public ushort UsagePage { get; set; }
    public ushort UsageId { get; set; }
}
