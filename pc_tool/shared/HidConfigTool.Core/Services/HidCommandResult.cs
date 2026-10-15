/*
 * HidCommandResult.cs
 * HID 命令执行结果
 * 统一封装 ACK 响应中的状态、错误码和附加参数
 */

namespace HidConfigTool.Core.Services;

/// <summary>
/// HID 命令执行结果（对应固件端 ACK 响应）
/// </summary>
public class HidCommandResult
{
    /// <summary>命令是否执行成功</summary>
    public bool Success { get; set; }

    /// <summary>错误码（成功时为 0）</summary>
    public byte ErrorCode { get; set; }

    /// <summary>附加参数（如当前 DPI、层号、解锁进度等）</summary>
    public ushort Param { get; set; }

    /// <summary>创建成功结果</summary>
    public static HidCommandResult Ok(ushort param = 0) => new()
    {
        Success = true,
        ErrorCode = 0,
        Param = param
    };

    /// <summary>创建失败结果</summary>
    public static HidCommandResult Fail(byte errorCode, ushort param = 0) => new()
    {
        Success = false,
        ErrorCode = errorCode,
        Param = param
    };
}
