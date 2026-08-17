namespace HidConfigTool.Core.Models;

/// <summary>
/// 错误日志信息
/// </summary>
public class ErrorLogInfo
{
    /// <summary>
    /// 当前日志条数
    /// </summary>
    public uint LogCount { get; set; }

    /// <summary>
    /// 总故障计数
    /// </summary>
    public uint TotalFaultCount { get; set; }
}

/// <summary>
/// 错误日志条目
/// </summary>
public class ErrorLogEntry
{
    /// <summary>
    /// 日志索引
    /// </summary>
    public byte Index { get; set; }

    /// <summary>
    /// 是否有效
    /// </summary>
    public bool IsValid { get; set; }

    /// <summary>
    /// 时间戳（毫秒，从设备启动开始）
    /// </summary>
    public uint TimestampMs { get; set; }

    /// <summary>
    /// 格式化的时间（mm:ss.fff）
    /// </summary>
    public string TimeFormatted
    {
        get
        {
            var totalSeconds = TimestampMs / 1000.0;
            var minutes = (int)(totalSeconds / 60);
            var seconds = totalSeconds - minutes * 60;
            return $"{minutes:D2}:{seconds:00.000}";
        }
    }

    /// <summary>
    /// 错误级别
    /// </summary>
    public byte Level { get; set; }

    /// <summary>
    /// 错误级别名称
    /// </summary>
    public string LevelName => Level switch
    {
        0 => "INFO",
        1 => "WARN",
        2 => "ERROR",
        3 => "FATAL",
        _ => "UNKNOWN"
    };

    /// <summary>
    /// 级别颜色（用于UI显示）
    /// </summary>
    public string LevelColor => Level switch
    {
        0 => "#4FC3F7",   // INFO - 蓝色
        1 => "#FFB74D",   // WARN - 橙色
        2 => "#EF5350",   // ERROR - 红色
        3 => "#E040FB",   // FATAL - 紫色
        _ => "#9E9E9E"    // UNKNOWN - 灰色
    };

    /// <summary>
    /// 模块名长度
    /// </summary>
    public byte ModuleLength { get; set; }

    /// <summary>
    /// 模块名
    /// </summary>
    public string Module { get; set; } = string.Empty;

    /// <summary>
    /// 消息内容
    /// </summary>
    public string Message { get; set; } = string.Empty;
}
