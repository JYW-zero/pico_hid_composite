/*
 * DeviceBehavior.cs
 * 模拟设备行为配置
 * 支持正常、超时、错误响应等行为模式
 */

namespace HidConfigTool.IntegrationTests.Simulator;

/// <summary>
/// 设备行为模式
/// </summary>
public enum DeviceBehaviorMode
{
    /// <summary>
    /// 正常响应
    /// </summary>
    Normal,

    /// <summary>
    /// 超时（延迟超过超时时间）
    /// </summary>
    Timeout,

    /// <summary>
    /// 返回错误数据（CRC错误、数据不完整等）
    /// </summary>
    ErrorResponse,

    /// <summary>
    /// 设备断开（所有操作失败）
    /// </summary>
    Disconnected,

    /// <summary>
    /// 间歇性故障（随机失败）
    /// </summary>
    Intermittent
}

/// <summary>
/// 设备行为配置
/// </summary>
public class DeviceBehavior
{
    /// <summary>
    /// 当前行为模式
    /// </summary>
    public DeviceBehaviorMode Mode { get; set; } = DeviceBehaviorMode.Normal;

    /// <summary>
    /// 超时延迟（毫秒），仅在 Timeout 模式下生效
    /// </summary>
    public int TimeoutDelayMs { get; set; } = 5000;

    /// <summary>
    /// 间歇性故障概率（0-1），仅在 Intermittent 模式下生效
    /// </summary>
    public double IntermittentFailureProbability { get; set; } = 0.3;

    /// <summary>
    /// 错误响应类型
    /// </summary>
    public ErrorResponseType ErrorType { get; set; } = ErrorResponseType.CrcMismatch;

    /// <summary>
    /// 随机数生成器（用于间歇性故障）
    /// </summary>
    private readonly Random _random = new();

    /// <summary>
    /// 判断当前操作是否应该失败
    /// </summary>
    public bool ShouldFail()
    {
        return Mode switch
        {
            DeviceBehaviorMode.Normal => false,
            DeviceBehaviorMode.Timeout => false, // 超时通过延迟实现，不直接失败
            DeviceBehaviorMode.ErrorResponse => true,
            DeviceBehaviorMode.Disconnected => true,
            DeviceBehaviorMode.Intermittent => _random.NextDouble() < IntermittentFailureProbability,
            _ => false
        };
    }

    /// <summary>
    /// 获取当前操作的延迟（毫秒）
    /// </summary>
    public int GetDelayMs()
    {
        return Mode == DeviceBehaviorMode.Timeout ? TimeoutDelayMs : 0;
    }

    /// <summary>
    /// 重置为正常行为
    /// </summary>
    public void Reset()
    {
        Mode = DeviceBehaviorMode.Normal;
        TimeoutDelayMs = 5000;
        IntermittentFailureProbability = 0.3;
        ErrorType = ErrorResponseType.CrcMismatch;
    }
}

/// <summary>
/// 错误响应类型
/// </summary>
public enum ErrorResponseType
{
    /// <summary>
    /// CRC 校验失败
    /// </summary>
    CrcMismatch,

    /// <summary>
    /// 数据不完整（长度不足）
    /// </summary>
    IncompleteData,

    /// <summary>
    /// 全零数据
    /// </summary>
    AllZeros,

    /// <summary>
    /// 随机数据
    /// </summary>
    RandomData,

    /// <summary>
    /// 返回 null
    /// </summary>
    NullResponse
}
