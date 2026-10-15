namespace HidConfigTool.Core.Models;

using HidConfigTool.Core.Utils;

/// <summary>
/// 设备诊断信息
/// </summary>
public class DeviceDiagnostics
{
    /// <summary>
    /// 设备基本信息
    /// </summary>
    public DeviceVersionInfo? DeviceInfo { get; set; }

    /// <summary>
    /// 错误日志信息
    /// </summary>
    public ErrorLogInfo? ErrorLogInfo { get; set; }

    /// <summary>
    /// 错误日志条目列表
    /// </summary>
    public IReadOnlyList<ErrorLogEntry>? ErrorLogs { get; set; }

    /// <summary>
    /// 系统性能统计
    /// </summary>
    public PerfSystemStat? PerfSystemStat { get; set; }

    /// <summary>
    /// 任务性能统计列表
    /// </summary>
    public IReadOnlyList<PerfTaskStat>? PerfTaskStats { get; set; }

    /// <summary>
    /// 配置 CRC 状态
    /// </summary>
    public ConfigCrcStatus? ConfigCrc { get; set; }

    /// <summary>
    /// HID 接口信息
    /// </summary>
    public HidInterfaceInfo? HidInterface { get; set; }

    /// <summary>
    /// 诊断时间
    /// </summary>
    public DateTime DiagnosedAt { get; set; } = DateTime.Now;

    /// <summary>
    /// 设备健康状态
    /// </summary>
    public DeviceHealthStatus HealthStatus
    {
        get
        {
            if (ErrorLogInfo?.TotalFaultCount > 0)
                return DeviceHealthStatus.Warning;
            if (PerfSystemStat?.CpuUsage > 80)
                return DeviceHealthStatus.Warning;
            if (ConfigCrc != null && !ConfigCrc.IsValid)
                return DeviceHealthStatus.Error;
            return DeviceHealthStatus.Healthy;
        }
    }
}

/// <summary>
/// 设备健康状态
/// </summary>
public enum DeviceHealthStatus
{
    Healthy,
    Warning,
    Error,
    Unknown
}

/// <summary>
/// 配置 CRC 状态
/// </summary>
public class ConfigCrcStatus
{
    /// <summary>
    /// 配置版本
    /// </summary>
    public ushort Version { get; set; }

    /// <summary>
    /// 配置大小
    /// </summary>
    public ushort Size { get; set; }

    /// <summary>
    /// 写入次数
    /// </summary>
    public uint WriteCount { get; set; }

    /// <summary>
    /// 存储的 CRC32
    /// </summary>
    public uint StoredCrc { get; set; }

    /// <summary>
    /// 计算的 CRC32
    /// </summary>
    public uint CalculatedCrc { get; set; }

    /// <summary>
    /// CRC 是否有效
    /// </summary>
    public bool IsValid => StoredCrc == CalculatedCrc;
}

/// <summary>
/// HID 接口信息
/// </summary>
public class HidInterfaceInfo
{
    /// <summary>
    /// 厂商 ID
    /// </summary>
    public ushort VendorId { get; set; }

    /// <summary>
    /// 产品 ID
    /// </summary>
    public ushort ProductId { get; set; }

    /// <summary>
    /// 版本号
    /// </summary>
    public ushort Version { get; set; }

    /// <summary>
    /// 厂商字符串
    /// </summary>
    public string Manufacturer { get; set; } = string.Empty;

    /// <summary>
    /// 产品字符串
    /// </summary>
    public string Product { get; set; } = string.Empty;

    /// <summary>
    /// 序列号
    /// </summary>
    public string SerialNumber { get; set; } = string.Empty;

    /// <summary>
    /// 接口数量
    /// </summary>
    public int InterfaceCount { get; set; }

    /// <summary>
    /// Report ID 列表
    /// </summary>
    public IReadOnlyList<byte> ReportIds { get; set; } = Array.Empty<byte>();

    /// <summary>
    /// 设备路径
    /// </summary>
    public string DevicePath { get; set; } = string.Empty;
}
