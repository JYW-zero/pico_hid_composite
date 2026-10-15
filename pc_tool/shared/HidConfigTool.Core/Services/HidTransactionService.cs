/*
 * HidTransactionService.cs
 * 统一 HID 事务服务
 * 封装 Feature Report 读写、命令发送与 ACK 响应、统一超时与重试策略
 *
 * 设计目标：
 * 1. 统一所有 HID 读写操作，消除各服务中的重复代码
 * 2. 统一超时、重试、错误处理和日志格式
 * 3. 自包含 Input 报告监听与 ACK 匹配，避免各服务重复实现
 * 4. 便于单元测试和后续协议升级
 */

using System.Collections.Concurrent;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Core.Services;

/// <summary>
/// 统一 HID 事务服务
/// 所有 HID 读写操作应通过此服务进行，而非直接操作 IHidDriver
/// </summary>
public class HidTransactionService : IDisposable
{
    private readonly IHidDriver _hidDriver;
    private readonly Action<string, string> _logAction;

    // ==================== ACK 等待器 ====================
    // key: 命令码, value: 等待器
    private readonly ConcurrentDictionary<byte, TaskCompletionSource<HidCommandResult>> _ackWaiters = new();

    // ==================== Input 监听任务 ====================
    private CancellationTokenSource? _inputCts;
    private Task? _inputTask;
    private volatile bool _isInputRunning;

    // ==================== 默认超时配置 ====================
    private const int DefaultFeatureReadTimeoutMs = 2000;
    private const int DefaultFeatureWriteTimeoutMs = 2000;
    private const int DefaultCommandTimeoutMs = 1500;
    private const int DefaultRetryCount = 3;
    private const int DefaultRetryDelayMs = 200;

    // ==================== 报告 ID（统一从 ProtocolConstants 引用，避免重复定义） ====================
    private const byte REPORT_ID_CONTROL = ProtocolConstants.ReportIdControl;
    private const byte REPORT_ID_ACK = ProtocolConstants.ReportIdAck;

    public HidTransactionService(IHidDriver hidDriver, Action<string, string> logAction)
    {
        _hidDriver = hidDriver;
        _logAction = logAction;
    }

    // ==================== Input 监听生命周期 ====================

    /// <summary>
    /// 启动 Input 报告监听任务（设备连接成功后调用）
    /// </summary>
    public void StartInputMonitor()
    {
        if (_isInputRunning) return;

        _inputCts = new CancellationTokenSource();
        _isInputRunning = true;
        _inputTask = Task.Run(() => InputMonitorLoop(_inputCts.Token));
        _logAction("INFO", "HID Input 监听任务已启动");
    }

    /// <summary>
    /// 停止 Input 报告监听任务（设备断开时调用）
    /// </summary>
    public void StopInputMonitor()
    {
        if (!_isInputRunning) return;

        _isInputRunning = false;
        _inputCts?.Cancel();

        // 取消所有等待中的 ACK
        foreach (var kvp in _ackWaiters)
        {
            kvp.Value.TrySetCanceled();
        }
        _ackWaiters.Clear();

        try
        {
            _inputTask?.Wait(1000);
        }
        catch { /* 忽略停止时的异常 */ }

        _inputCts?.Dispose();
        _inputCts = null;
        _inputTask = null;
        _logAction("INFO", "HID Input 监听任务已停止");
    }

    // ==================== Input 监听主循环 ====================

    private async Task InputMonitorLoop(CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            try
            {
                byte[]? report = await _hidDriver.ReadInputReportAsync(token);
                if (report == null || report.Length == 0)
                {
                    await Task.Delay(10, token);
                    continue;
                }

                // 处理 ACK 报告（Report ID = 24）
                if (report[0] == REPORT_ID_ACK && report.Length >= 3)
                {
                    HandleAckReport(report);
                }
                // 其他 Input 报告（如键盘/鼠标）忽略，由系统 HID 栈处理
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (Exception ex)
            {
                _logAction("ERROR", $"HID Input 监听异常: {ex.Message}");
                try
                {
                    await Task.Delay(100, token);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
            }
        }
    }

    /// <summary>
    /// 处理 ACK 报告，匹配并唤醒对应的等待器
    /// ACK 格式: [report_id=24, cmd, status(0=成功), error_code, param_low, param_high, reserved[2]]
    /// </summary>
    private void HandleAckReport(byte[] report)
    {
        byte cmd = report[1];
        bool success = report[2] == 0;
        byte errorCode = report.Length > 3 ? report[3] : (byte)0;
        ushort param = report.Length >= 6 ? (ushort)(report[4] | (report[5] << 8)) : (ushort)0;

        var result = success
            ? HidCommandResult.Ok(param)
            : HidCommandResult.Fail(errorCode, param);

        if (_ackWaiters.TryRemove(cmd, out var waiter))
        {
            _logAction("DEBUG", $"收到 ACK: cmd=0x{cmd:X2}, success={success}, error=0x{errorCode:X2}, param=0x{param:X4}");
            waiter.TrySetResult(result);
        }
        else
        {
            _logAction("DEBUG", $"收到无等待者的 ACK: cmd=0x{cmd:X2}, 已丢弃");
        }
    }

    // ==================== Feature Report 读取 ====================

    /// <summary>
    /// 读取 Feature Report（统一超时）
    /// </summary>
    /// <param name="reportId">报告 ID</param>
    /// <param name="timeoutMs">超时时间（毫秒），null 使用默认值</param>
    /// <returns>报告数据（不含 Report ID），失败返回 null</returns>
    public async Task<byte[]?> ReadFeatureAsync(byte reportId, int? timeoutMs = null)
    {
        try
        {
            using var cts = new CancellationTokenSource(timeoutMs ?? DefaultFeatureReadTimeoutMs);
            return await _hidDriver.GetFeatureReportAsync(reportId, cts.Token);
        }
        catch (OperationCanceledException)
        {
            _logAction("ERROR", $"读取 Feature 报告超时: reportId=0x{reportId:X2}, timeout={timeoutMs ?? DefaultFeatureReadTimeoutMs}ms");
            return null;
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"读取 Feature 报告异常: reportId=0x{reportId:X2}, {ex.Message}");
            return null;
        }
    }

    // ==================== Feature Report 写入 ====================

    /// <summary>
    /// 写入 Feature Report（统一超时）
    /// </summary>
    /// <param name="reportId">报告 ID</param>
    /// <param name="data">要写入的数据（不含 Report ID）</param>
    /// <param name="timeoutMs">超时时间（毫秒），null 使用默认值</param>
    /// <returns>是否成功</returns>
    public async Task<bool> WriteFeatureAsync(byte reportId, byte[] data, int? timeoutMs = null)
    {
        try
        {
            using var cts = new CancellationTokenSource(timeoutMs ?? DefaultFeatureWriteTimeoutMs);
            return await _hidDriver.SendFeatureReportAsync(reportId, data, cts.Token);
        }
        catch (OperationCanceledException)
        {
            _logAction("ERROR", $"写入 Feature 报告超时: reportId=0x{reportId:X2}, timeout={timeoutMs ?? DefaultFeatureWriteTimeoutMs}ms");
            return false;
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"写入 Feature 报告异常: reportId=0x{reportId:X2}, {ex.Message}");
            return false;
        }
    }

    // ==================== 命令发送（带 ACK） ====================

    /// <summary>
    /// 发送控制命令并等待 ACK 响应
    /// </summary>
    /// <param name="command">命令码</param>
    /// <param name="param">附加参数（可选，1-2字节）</param>
    /// <param name="timeoutMs">ACK 等待超时（毫秒），null 使用默认值</param>
    /// <returns>命令执行结果，超时或失败返回 null</returns>
    public async Task<HidCommandResult?> SendCommandAsync(byte command, byte[]? param = null, int? timeoutMs = null)
    {
        // 构造命令数据: [command, param...]
        byte[] data = param != null && param.Length > 0
            ? new byte[1 + param.Length]
            : new byte[1];
        data[0] = command;
        if (param != null && param.Length > 0)
        {
            Array.Copy(param, 0, data, 1, param.Length);
        }

        // 注册 ACK 等待器
        var tcs = new TaskCompletionSource<HidCommandResult>(TaskCreationOptions.RunContinuationsAsynchronously);
        if (!_ackWaiters.TryAdd(command, tcs))
        {
            // 如果已有同命令的等待器，先取消旧的
            if (_ackWaiters.TryRemove(command, out var oldWaiter))
            {
                oldWaiter.TrySetCanceled();
            }
            _ackWaiters.TryAdd(command, tcs);
        }

        try
        {
            // 发送命令
            bool sent = await WriteFeatureAsync(REPORT_ID_CONTROL, data);
            if (!sent)
            {
                _ackWaiters.TryRemove(command, out _);
                _logAction("ERROR", $"发送命令失败: cmd=0x{command:X2}");
                return null;
            }

            // 等待 ACK
            using var cts = new CancellationTokenSource(timeoutMs ?? DefaultCommandTimeoutMs);
            var completedTask = await Task.WhenAny(tcs.Task, Task.Delay(timeoutMs ?? DefaultCommandTimeoutMs, cts.Token));

            if (completedTask == tcs.Task)
            {
                return await tcs.Task;
            }

            // 超时
            _ackWaiters.TryRemove(command, out _);
            _logAction("ERROR", $"命令等待 ACK 超时: cmd=0x{command:X2}, timeout={timeoutMs ?? DefaultCommandTimeoutMs}ms");
            return null;
        }
        catch (Exception ex)
        {
            _ackWaiters.TryRemove(command, out _);
            _logAction("ERROR", $"发送命令异常: cmd=0x{command:X2}, {ex.Message}");
            return null;
        }
    }

    // ==================== 统一重试策略 ====================

    /// <summary>
    /// 带重试的异步操作
    /// </summary>
    /// <typeparam name="T">返回值类型</typeparam>
    /// <param name="action">要执行的操作</param>
    /// <param name="maxRetries">最大重试次数（不含首次）</param>
    /// <param name="delayMs">重试间隔（毫秒）</param>
    /// <returns>操作结果，所有重试都失败返回 default</returns>
    public async Task<T?> RetryAsync<T>(Func<Task<T?>> action, int maxRetries = DefaultRetryCount, int delayMs = DefaultRetryDelayMs)
        where T : class
    {
        T? result = null;
        for (int attempt = 0; attempt <= maxRetries; attempt++)
        {
            result = await action();
            if (result != null)
            {
                if (attempt > 0)
                {
                    _logAction("INFO", $"操作重试成功: 第{attempt + 1}次尝试");
                }
                return result;
            }

            if (attempt < maxRetries)
            {
                _logAction("WARNING", $"操作失败，{delayMs}ms后重试: 第{attempt + 1}/{maxRetries}次");
                await Task.Delay(delayMs);
            }
        }

        _logAction("ERROR", $"操作最终失败: 共尝试{maxRetries + 1}次");
        return result;
    }

    /// <summary>
    /// 带重试的布尔操作
    /// </summary>
    public async Task<bool> RetryBoolAsync(Func<Task<bool>> action, int maxRetries = DefaultRetryCount, int delayMs = DefaultRetryDelayMs)
    {
        for (int attempt = 0; attempt <= maxRetries; attempt++)
        {
            bool result = await action();
            if (result)
            {
                if (attempt > 0)
                {
                    _logAction("INFO", $"操作重试成功: 第{attempt + 1}次尝试");
                }
                return true;
            }

            if (attempt < maxRetries)
            {
                _logAction("WARNING", $"操作失败，{delayMs}ms后重试: 第{attempt + 1}/{maxRetries}次");
                await Task.Delay(delayMs);
            }
        }

        _logAction("ERROR", $"操作最终失败: 共尝试{maxRetries + 1}次");
        return false;
    }

    // ==================== IDisposable ====================

    public void Dispose()
    {
        StopInputMonitor();
    }
}
