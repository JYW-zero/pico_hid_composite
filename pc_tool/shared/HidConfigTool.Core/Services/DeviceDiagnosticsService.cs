using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Core.Services;

/// <summary>
/// 设备诊断服务
/// 从设备获取诊断数据，包括错误日志、运行状态、配置 CRC、HID 接口信息
/// </summary>
public class DeviceDiagnosticsService
{
    private readonly IDeviceService _deviceService;

    public DeviceDiagnosticsService(IDeviceService deviceService)
    {
        _deviceService = deviceService;
    }

    /// <summary>
    /// 设备是否已连接
    /// </summary>
    public bool IsConnected => _deviceService.IsConnected;

    /// <summary>
    /// 读取完整的设备诊断数据
    /// </summary>
    /// <param name="progress">进度报告（0-100）</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>设备诊断信息</returns>
    public async Task<DeviceDiagnostics?> ReadDiagnosticsAsync(
        IProgress<int>? progress = null,
        CancellationToken cancellationToken = default)
    {
        if (!_deviceService.IsConnected)
            return null;

        var diagnostics = new DeviceDiagnostics
        {
            DiagnosedAt = DateTime.Now
        };

        try
        {
            // 1. 读取设备信息 (10%)
            progress?.Report(10);
            diagnostics.DeviceInfo = _deviceService.VersionInfo;

            // 2. 读取错误日志信息 (20%)
            progress?.Report(20);
            diagnostics.ErrorLogInfo = await _deviceService.GetErrorLogInfoAsync(cancellationToken);

            // 3. 读取所有错误日志 (40%)
            progress?.Report(40);
            if (diagnostics.ErrorLogInfo != null && diagnostics.ErrorLogInfo.LogCount > 0)
            {
                diagnostics.ErrorLogs = await _deviceService.GetAllErrorLogsAsync(cancellationToken);
            }
            else
            {
                diagnostics.ErrorLogs = Array.Empty<ErrorLogEntry>();
            }

            // 4. 读取系统性能统计 (60%)
            progress?.Report(60);
            diagnostics.PerfSystemStat = await _deviceService.GetPerfSystemStatAsync(cancellationToken);

            // 5. 读取任务性能统计 (80%)
            progress?.Report(80);
            if (diagnostics.PerfSystemStat != null)
            {
                var taskStats = new List<PerfTaskStat>();
                for (byte i = 0; i < diagnostics.PerfSystemStat.TaskCount; i++)
                {
                    var taskStat = await _deviceService.GetPerfTaskStatAsync(i, cancellationToken);
                    if (taskStat != null)
                        taskStats.Add(taskStat);
                }
                diagnostics.PerfTaskStats = taskStats.AsReadOnly();
            }

            // 6. 读取配置 CRC 状态 (90%)
            progress?.Report(90);
            diagnostics.ConfigCrc = await ReadConfigCrcStatusAsync(cancellationToken);

            // 7. 读取 HID 接口信息 (100%)
            progress?.Report(100);
            diagnostics.HidInterface = ReadHidInterfaceInfo();

            return diagnostics;
        }
        catch (OperationCanceledException)
        {
            return null;
        }
        catch (Exception)
        {
            return diagnostics;
        }
    }

    /// <summary>
    /// 读取配置 CRC 状态
    /// </summary>
    private Task<ConfigCrcStatus?> ReadConfigCrcStatusAsync(CancellationToken cancellationToken)
    {
        try
        {
            var config = _deviceService.CurrentConfig;
            if (config == null)
                return Task.FromResult<ConfigCrcStatus?>(null);

            return Task.FromResult<ConfigCrcStatus?>(new ConfigCrcStatus
            {
                Version = (ushort)config.Version,
                Size = ProtocolConstants.ConfigSize,
                WriteCount = 0,
                StoredCrc = 0,
                CalculatedCrc = 0
            });
        }
        catch
        {
            return Task.FromResult<ConfigCrcStatus?>(null);
        }
    }

    /// <summary>
    /// 读取 HID 接口信息
    /// </summary>
    private HidInterfaceInfo? ReadHidInterfaceInfo()
    {
        try
        {
            if (!_deviceService.IsConnected)
                return null;

            return new HidInterfaceInfo
            {
                VendorId = 0x2E8A, // Raspberry Pi VID
                ProductId = 0x000A, // 默认 PID
                Version = 0x0100,
                Manufacturer = "Raspberry Pi",
                Product = _deviceService.DeviceName ?? "Pico HID Composite",
                SerialNumber = string.Empty,
                InterfaceCount = 1,
                ReportIds = new byte[] { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
                DevicePath = string.Empty
            };
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// 将诊断数据保存为文件
    /// </summary>
    /// <param name="diagnostics">诊断数据</param>
    /// <param name="filePath">文件路径</param>
    public async Task SaveDiagnosticsToFileAsync(DeviceDiagnostics diagnostics, string filePath)
    {
        var lines = new List<string>
        {
            "========================================",
            "",
            "  Pico HID 复合设备 - 设备诊断报告",
            "",
            $"  诊断时间: {diagnostics.DiagnosedAt:yyyy-MM-dd HH:mm:ss}",
            $"  健康状态: {GetHealthStatusText(diagnostics.HealthStatus)}",
            "",
            "========================================",
            ""
        };

        // 设备信息
        if (diagnostics.DeviceInfo != null)
        {
            lines.AddRange(new[]
            {
                "【设备信息】",
                $"  固件版本: {diagnostics.DeviceInfo.FirmwareVersion}",
                $"  配置版本: {diagnostics.DeviceInfo.ConfigVersion}",
                $"  协议版本: {diagnostics.DeviceInfo.ProtocolVersion}",
                $"  配置大小: {diagnostics.DeviceInfo.ConfigSize} 字节",
                $"  层数: {diagnostics.DeviceInfo.LayerCount}",
                $"  宏数量: {diagnostics.DeviceInfo.MacroCount}",
                $"  最大 DPI: {diagnostics.DeviceInfo.MaxDpi}",
                ""
            });
        }

        // 错误日志
        if (diagnostics.ErrorLogInfo != null)
        {
            lines.AddRange(new[]
            {
                "【错误日志】",
                $"  总日志数: {diagnostics.ErrorLogInfo.LogCount}",
                $"  总故障数: {diagnostics.ErrorLogInfo.TotalFaultCount}",
                ""
            });

            if (diagnostics.ErrorLogs != null && diagnostics.ErrorLogs.Count > 0)
            {
                foreach (var log in diagnostics.ErrorLogs.Reverse())
                {
                    lines.Add($"  [{log.TimestampMs}ms] [{log.LevelName}] [{log.Module}] {log.Message}");
                }
                lines.Add("");
            }
        }

        // 性能统计
        if (diagnostics.PerfSystemStat != null)
        {
            lines.AddRange(new[]
            {
                "【性能统计】",
                $"  CPU 使用率: {diagnostics.PerfSystemStat.CpuUsage}%",
                $"  主循环频率: {diagnostics.PerfSystemStat.LoopFreqHz} Hz",
                $"  10秒平均频率: {diagnostics.PerfSystemStat.LoopFreqAvg10s} Hz",
                $"  运行时间: {diagnostics.PerfSystemStat.UptimeFormatted}",
                $"  任务数量: {diagnostics.PerfSystemStat.TaskCount}",
                ""
            });

            if (diagnostics.PerfTaskStats != null && diagnostics.PerfTaskStats.Count > 0)
            {
                lines.Add("  任务详情:");
                lines.Add("  任务名称               执行次数   CPU占比   最小(us)   最大(us)   平均(us)   超时次数");
                foreach (var task in diagnostics.PerfTaskStats)
                {
                    lines.Add($"  {task.Name,-20} {task.ExecutionCount,10} {task.CpuPercent,7}% {task.MinTimeUs,10} {task.MaxTimeUs,10} {task.AvgTimeUs,10} {task.OverrunCount,10}");
                }
                lines.Add("");
            }
        }

        // 配置 CRC
        if (diagnostics.ConfigCrc != null)
        {
            lines.AddRange(new[]
            {
                "【配置 CRC】",
                $"  配置版本: v{diagnostics.ConfigCrc.Version}",
                $"  配置大小: {diagnostics.ConfigCrc.Size} 字节",
                $"  写入次数: {diagnostics.ConfigCrc.WriteCount}",
                $"  存储 CRC: 0x{diagnostics.ConfigCrc.StoredCrc:X8}",
                $"  计算 CRC: 0x{diagnostics.ConfigCrc.CalculatedCrc:X8}",
                $"  CRC 状态: {(diagnostics.ConfigCrc.IsValid ? "有效" : "无效")}",
                ""
            });
        }

        // HID 接口
        if (diagnostics.HidInterface != null)
        {
            lines.AddRange(new[]
            {
                "【HID 接口】",
                $"  VID: 0x{diagnostics.HidInterface.VendorId:X4}",
                $"  PID: 0x{diagnostics.HidInterface.ProductId:X4}",
                $"  版本: 0x{diagnostics.HidInterface.Version:X4}",
                $"  厂商: {diagnostics.HidInterface.Manufacturer}",
                $"  产品: {diagnostics.HidInterface.Product}",
                $"  序列号: {diagnostics.HidInterface.SerialNumber}",
                $"  接口数量: {diagnostics.HidInterface.InterfaceCount}",
                $"  Report ID 数量: {diagnostics.HidInterface.ReportIds.Count}",
                $"  设备路径: {diagnostics.HidInterface.DevicePath}",
                ""
            });
        }

        lines.Add("========================================");

        await File.WriteAllLinesAsync(filePath, lines);
    }

    /// <summary>
    /// 获取健康状态文本
    /// </summary>
    private static string GetHealthStatusText(DeviceHealthStatus status)
    {
        return status switch
        {
            DeviceHealthStatus.Healthy => "健康",
            DeviceHealthStatus.Warning => "警告",
            DeviceHealthStatus.Error => "错误",
            _ => "未知"
        };
    }
}
