namespace HidConfigTool.Core.Models;

/// <summary>
/// 系统性能统计
/// </summary>
public class PerfSystemStat
{
    /// <summary>
    /// CPU使用率（0-100）
    /// </summary>
    public byte CpuUsage { get; set; }

    /// <summary>
    /// 主循环频率（Hz）
    /// </summary>
    public ushort LoopFreqHz { get; set; }

    /// <summary>
    /// 运行时间（秒）
    /// </summary>
    public uint UptimeSeconds { get; set; }

    /// <summary>
    /// 任务数量
    /// </summary>
    public byte TaskCount { get; set; }

    /// <summary>
    /// 10秒平均CPU使用率（0-100）
    /// </summary>
    public byte CpuUsageAvg10s { get; set; }

    /// <summary>
    /// 30秒平均CPU使用率（0-100）
    /// </summary>
    public byte CpuUsageAvg30s { get; set; }

    /// <summary>
    /// 10秒平均主循环频率（Hz）
    /// </summary>
    public ushort LoopFreqAvg10s { get; set; }

    /// <summary>
    /// 格式化的运行时间
    /// </summary>
    public string UptimeFormatted
    {
        get
        {
            var totalSeconds = UptimeSeconds;
            var hours = totalSeconds / 3600;
            var minutes = (totalSeconds % 3600) / 60;
            var seconds = totalSeconds % 60;

            if (hours > 0)
                return $"{hours}:{minutes:D2}:{seconds:D2}";
            else
                return $"{minutes}:{seconds:D2}";
        }
    }
}

/// <summary>
/// 任务性能统计
/// </summary>
public class PerfTaskStat
{
    /// <summary>
    /// 任务索引
    /// </summary>
    public byte Index { get; set; }

    /// <summary>
    /// 任务名称
    /// </summary>
    public string Name { get; set; } = string.Empty;

    /// <summary>
    /// 执行次数
    /// </summary>
    public uint ExecutionCount { get; set; }

    /// <summary>
    /// 最小执行时间（微秒）
    /// </summary>
    public uint MinTimeUs { get; set; }

    /// <summary>
    /// 最小执行时间显示文本（0xFFFFFFFF显示为"-"）
    /// </summary>
    public string MinTimeUsDisplay => MinTimeUs == 0xFFFFFFFF ? "-" : MinTimeUs.ToString();

    /// <summary>
    /// 最大执行时间（微秒）
    /// </summary>
    public uint MaxTimeUs { get; set; }

    /// <summary>
    /// 最大执行时间显示文本
    /// </summary>
    public string MaxTimeUsDisplay => MaxTimeUs == 0 ? "-" : MaxTimeUs.ToString();

    /// <summary>
    /// 平均执行时间（微秒）
    /// </summary>
    public uint AvgTimeUs { get; set; }

    /// <summary>
    /// 平均执行时间显示文本
    /// </summary>
    public string AvgTimeUsDisplay => AvgTimeUs == 0 ? "-" : AvgTimeUs.ToString();

    /// <summary>
    /// 最近执行时间（微秒）
    /// </summary>
    public uint LastTimeUs { get; set; }

    /// <summary>
    /// 最近执行时间显示文本
    /// </summary>
    public string LastTimeUsDisplay => LastTimeUs == 0 ? "-" : LastTimeUs.ToString();

    /// <summary>
    /// CPU占比（0-100，占总忙碌时间的百分比）
    /// </summary>
    public byte CpuPercent { get; set; }

    /// <summary>
    /// 超时次数
    /// </summary>
    public uint OverrunCount { get; set; }

    /// <summary>
    /// 超时阈值（微秒）
    /// </summary>
    public uint ThresholdUs { get; set; }
}
