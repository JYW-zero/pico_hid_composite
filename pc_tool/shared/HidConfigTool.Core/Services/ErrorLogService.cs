/*
 * ErrorLogService.cs
 * 错误日志服务
 *
 * 从 DeviceService.cs 提取，包含固件错误日志的读取和清除功能。
 * 支持读取日志信息、单条日志、所有日志，以及清除日志。
 *
 * 与固件 firmware/src/middleware/fault.c 中的错误日志记录逻辑保持同步。
 */

namespace HidConfigTool.Core.Services;

using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

/// <summary>
/// 错误日志服务
/// 负责固件错误日志的读取和清除
/// </summary>
public class ErrorLogService
{
    // ==================== 常量（Report ID 和命令 ID 统一从 ProtocolConstants 引用） ====================

    /// <summary>
    /// 错误日志信息的 Report ID
    /// </summary>
    public const byte REPORT_ID_FAULT_INFO = ProtocolConstants.ReportIdFaultInfo;

    /// <summary>
    /// 错误日志读取的 Report ID
    /// </summary>
    public const byte REPORT_ID_FAULT_LOG = ProtocolConstants.ReportIdFaultLog;

    /// <summary>
    /// 清除错误日志命令
    /// </summary>
    public const byte CMD_CLEAR_FAULT = ProtocolConstants.CmdClearFault;

    /// <summary>
    /// 错误日志报告大小（不含 Report ID）
    /// </summary>
    public const int FAULT_LOG_REPORT_SIZE = 62;

    /// <summary>
    /// 最大错误日志条数
    /// </summary>
    public const int MAX_ERROR_LOGS = 16;

    /// <summary>
    /// 模块名最大长度
    /// </summary>
    public const int MAX_MODULE_NAME_LENGTH = 31;

    /// <summary>
    /// 消息最大长度
    /// </summary>
    public const int MAX_MESSAGE_LENGTH = 21;

    /// <summary>
    /// 消息在报告中的偏移量
    /// </summary>
    public const int MESSAGE_OFFSET = 40;

    // ==================== 依赖注入 ====================

    private readonly IHidDriver _hidDriver;
    private readonly SemaphoreSlim _writeLock;
    private readonly Func<bool> _isConnectedFunc;
    private readonly Action<string, string> _logAction;
    private readonly Func<Task<bool>> _unlockConfigFunc;
    private readonly Func<byte, Task<bool>> _sendControlCommandFunc;

    // ==================== 状态 ====================

    private IReadOnlyList<ErrorLogEntry>? _lastErrorLogs;

    /// <summary>
    /// 构造函数
    /// </summary>
    /// <param name="hidDriver">HID 驱动</param>
    /// <param name="writeLock">写入锁（用于串行化 HID 写入操作）</param>
    /// <param name="isConnectedFunc">设备连接状态检查函数</param>
    /// <param name="logAction">日志记录函数（参数：级别, 消息）</param>
    /// <param name="unlockConfigFunc">解锁配置函数</param>
    /// <param name="sendControlCommandFunc">发送控制命令函数（参数：命令字节）</param>
    public ErrorLogService(
        IHidDriver hidDriver,
        SemaphoreSlim writeLock,
        Func<bool> isConnectedFunc,
        Action<string, string> logAction,
        Func<Task<bool>> unlockConfigFunc,
        Func<byte, Task<bool>> sendControlCommandFunc)
    {
        _hidDriver = hidDriver ?? throw new ArgumentNullException(nameof(hidDriver));
        _writeLock = writeLock ?? throw new ArgumentNullException(nameof(writeLock));
        _isConnectedFunc = isConnectedFunc ?? throw new ArgumentNullException(nameof(isConnectedFunc));
        _logAction = logAction ?? throw new ArgumentNullException(nameof(logAction));
        _unlockConfigFunc = unlockConfigFunc ?? throw new ArgumentNullException(nameof(unlockConfigFunc));
        _sendControlCommandFunc = sendControlCommandFunc ?? throw new ArgumentNullException(nameof(sendControlCommandFunc));
    }

    // ==================== 错误日志读取 ====================

    /// <summary>
    /// 获取错误日志信息
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>错误日志信息，失败返回null</returns>
    public async Task<ErrorLogInfo?> GetErrorLogInfoAsync(CancellationToken cancellationToken = default)
    {
        if (!_isConnectedFunc())
            return null;

        try
        {
            await _writeLock.WaitAsync(cancellationToken);
            try
            {
                byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_FAULT_INFO);
                if (data == null || data.Length < 8)
                {
                    _logAction("ERROR", "读取错误日志信息失败");
                    return null;
                }

                var info = new ErrorLogInfo
                {
                    LogCount = BitConverter.ToUInt32(data, 0),
                    TotalFaultCount = BitConverter.ToUInt32(data, 4)
                };

                _logAction("INFO", $"读取错误日志信息成功: {info.LogCount} 条日志, 总故障 {info.TotalFaultCount} 次");
                return info;
            }
            finally
            {
                _writeLock.Release();
            }
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"读取错误日志信息异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 读取指定索引的错误日志
    /// </summary>
    /// <param name="index">日志索引 (0-15)</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>错误日志条目，失败返回null</returns>
    public async Task<ErrorLogEntry?> ReadErrorLogEntryAsync(byte index, CancellationToken cancellationToken = default)
    {
        if (!_isConnectedFunc())
            return null;

        try
        {
            await _writeLock.WaitAsync(cancellationToken);
            try
            {
                // 1. 先设置读取索引（SET_REPORT）
                byte[] indexData = new byte[FAULT_LOG_REPORT_SIZE];
                indexData[0] = index;
                bool ok = await _hidDriver.SendFeatureReportAsync(REPORT_ID_FAULT_LOG, indexData);
                if (!ok)
                {
                    _logAction("ERROR", $"设置错误日志读取索引 {index} 失败");
                    return null;
                }

                // 短暂延迟
                await Task.Delay(5, cancellationToken);

                // 2. 再读取数据（GET_REPORT）
                byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_FAULT_LOG);
                if (data == null || data.Length < 8)
                {
                    _logAction("ERROR", $"读取错误日志 {index} 失败");
                    return null;
                }

                var entry = new ErrorLogEntry
                {
                    Index = data[0],
                    IsValid = data[1] == 1
                };

                if (entry.IsValid)
                {
                    entry.TimestampMs = BitConverter.ToUInt32(data, 2);
                    entry.Level = data[6];
                    entry.ModuleLength = data[7];

                    // 读取模块名（最多32字节）
                    int moduleLen = Math.Min((int)entry.ModuleLength, MAX_MODULE_NAME_LENGTH);
                    if (moduleLen > 0 && data.Length >= 8 + moduleLen)
                    {
                        entry.Module = System.Text.Encoding.UTF8.GetString(data, 8, moduleLen);
                    }

                    // 读取消息（最多22字节）
                    int maxMsgLen = Math.Min(MAX_MESSAGE_LENGTH, data.Length - MESSAGE_OFFSET);
                    if (maxMsgLen > 0)
                    {
                        // 找到字符串结束位置
                        int msgLen = 0;
                        for (int i = 0; i < maxMsgLen; i++)
                        {
                            if (data[MESSAGE_OFFSET + i] == 0)
                                break;
                            msgLen++;
                        }
                        if (msgLen > 0)
                        {
                            entry.Message = System.Text.Encoding.UTF8.GetString(data, MESSAGE_OFFSET, msgLen);
                        }
                    }
                }

                _logAction("INFO", $"读取错误日志 {index} 成功: {entry.LevelName} - {entry.Module} - {entry.Message}");
                return entry;
            }
            finally
            {
                _writeLock.Release();
            }
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"读取错误日志 {index} 异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 获取所有错误日志
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>所有错误日志列表，失败返回null</returns>
    public async Task<IReadOnlyList<ErrorLogEntry>?> GetAllErrorLogsAsync(CancellationToken cancellationToken = default)
    {
        if (!_isConnectedFunc())
            return null;

        try
        {
            // 先获取日志数量
            var info = await GetErrorLogInfoAsync(cancellationToken);
            if (info == null)
                return null;

            var logs = new List<ErrorLogEntry>();
            int count = (int)Math.Min(info.LogCount, MAX_ERROR_LOGS);

            // 逐条读取
            for (byte i = 0; i < count; i++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                var entry = await ReadErrorLogEntryAsync(i, cancellationToken);
                if (entry != null && entry.IsValid)
                {
                    logs.Add(entry);
                }

                // 短暂延迟，避免太快
                await Task.Delay(10, cancellationToken);
            }

            _logAction("INFO", $"获取所有错误日志成功，共 {logs.Count} 条");
            _lastErrorLogs = logs.AsReadOnly();
            return _lastErrorLogs;
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"获取所有错误日志异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 清除所有错误日志
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>是否成功</returns>
    public async Task<bool> ClearErrorLogsAsync(CancellationToken cancellationToken = default)
    {
        if (!_isConnectedFunc())
            return false;

        try
        {
            // 清除日志是写操作，需要先解锁
            await _unlockConfigFunc();
            bool result = await _sendControlCommandFunc(CMD_CLEAR_FAULT);
            if (result)
            {
                _logAction("INFO", "清除错误日志成功");
                _lastErrorLogs = null;
            }
            else
            {
                _logAction("ERROR", "清除错误日志失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"清除错误日志异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 获取最后一次读取的错误日志缓存
    /// </summary>
    /// <returns>错误日志列表，未读取过返回null</returns>
    public IReadOnlyList<ErrorLogEntry>? GetLastErrorLogs()
    {
        return _lastErrorLogs;
    }
}
