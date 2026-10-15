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
    /// 发送 Feature 报告（支持取消）
    /// </summary>
    Task<bool> SendFeatureReportAsync(byte reportId, byte[] data, CancellationToken cancellationToken);

    /// <summary>
    /// 发送 Output 报告（中断传输，速度比 Feature Report 更快）
    /// 用于高端游戏外设的命令发送，走 USB 中断传输（Interrupt Transfer）
    /// </summary>
    /// <param name="reportId">报告 ID</param>
    /// <param name="data">报告数据（不含 Report ID）</param>
    Task<bool> SendOutputReportAsync(byte reportId, byte[] data);

    /// <summary>
    /// 发送 Output 报告（支持取消）
    /// </summary>
    Task<bool> SendOutputReportAsync(byte reportId, byte[] data, CancellationToken cancellationToken);

    /// <summary>
    /// 读取 Feature 报告
    /// </summary>
    Task<byte[]?> GetFeatureReportAsync(byte reportId);

    /// <summary>
    /// 读取 Feature 报告（支持取消）
    /// </summary>
    Task<byte[]?> GetFeatureReportAsync(byte reportId, CancellationToken cancellationToken);

    /// <summary>
    /// 读取 Input 报告（设备主动发送给主机的报告，如 ACK 响应）
    /// 注意：这是一个阻塞调用，直到收到报告或超时/取消。
    /// 调用方应在独立任务或带超时的 CancellationToken 中调用。
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>报告数据（第一个字节为 Report ID），失败或取消返回 null</returns>
    Task<byte[]?> ReadInputReportAsync(CancellationToken cancellationToken = default);
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
