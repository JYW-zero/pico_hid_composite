/*
 * DeviceConnectionService.cs
 * 设备连接管理服务
 *
 * 从 DeviceService.cs 提取，包含设备连接状态检测、Ping、重连等功能。
 * 逐步提取设备连接管理相关方法，降低 DeviceService 的复杂度。
 *
 * 与固件 firmware/src/app/main.c 中的设备连接管理逻辑保持同步。
 */

namespace HidConfigTool.Core.Services;

using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

/// <summary>
/// 设备连接管理服务
/// 负责设备连接状态检测、Ping、重连等功能
/// </summary>
public class DeviceConnectionService
{
    // ==================== 常量 ====================

    /// <summary>
    /// 设备信息 Report ID（用于 Ping 检测设备是否响应）
    /// v3.0 协议：REPORT_ID_CONTROL(7) 已改为 Output Report，不支持 GET_REPORT，
    /// 所以使用 DEVICE_INFO(6) 进行 Ping 检测
    /// </summary>
    public const byte REPORT_ID_DEVICE_INFO = ProtocolConstants.ReportIdDeviceInfo;

    /// <summary>
    /// Ping 超时时间（毫秒）
    /// </summary>
    public const int PING_TIMEOUT_MS = 1000;

    /// <summary>
    /// 重连前等待设备稳定的时间（毫秒）
    /// </summary>
    public const int RECONNECT_STABLE_DELAY_MS = 50;

    // ==================== 依赖注入 ====================

    private readonly IHidDriver _hidDriver;
    private readonly Func<bool> _isConnectedFunc;
    private readonly Func<string> _getDevicePathFunc;
    private readonly Action _clearStateAction;
    private readonly Action<bool> _connectionChangedAction;
    private readonly Action<string, string> _logAction;

    /// <summary>
    /// 构造函数
    /// </summary>
    /// <param name="hidDriver">HID 驱动</param>
    /// <param name="isConnectedFunc">设备连接状态检查函数</param>
    /// <param name="getDevicePathFunc">获取当前设备路径函数</param>
    /// <param name="clearStateAction">清除设备状态的回调（_currentDevice、_currentConfig、FirmwareVersion、VersionInfo、Compatibility）</param>
    /// <param name="connectionChangedAction">设备连接变化事件回调（参数：true=已连接，false=已断开）</param>
    /// <param name="logAction">日志记录回调（参数：级别, 消息）</param>
    public DeviceConnectionService(
        IHidDriver hidDriver,
        Func<bool> isConnectedFunc,
        Func<string> getDevicePathFunc,
        Action clearStateAction,
        Action<bool> connectionChangedAction,
        Action<string, string> logAction)
    {
        _hidDriver = hidDriver ?? throw new ArgumentNullException(nameof(hidDriver));
        _isConnectedFunc = isConnectedFunc ?? throw new ArgumentNullException(nameof(isConnectedFunc));
        _getDevicePathFunc = getDevicePathFunc ?? throw new ArgumentNullException(nameof(getDevicePathFunc));
        _clearStateAction = clearStateAction ?? throw new ArgumentNullException(nameof(clearStateAction));
        _connectionChangedAction = connectionChangedAction ?? throw new ArgumentNullException(nameof(connectionChangedAction));
        _logAction = logAction ?? throw new ArgumentNullException(nameof(logAction));
    }

    // ==================== 设备连接检测 ====================

    /// <summary>
    /// Ping 设备，确认设备是否在线
    /// 通过读取控制状态报告确认设备是否响应
    /// </summary>
    /// <returns>true=设备在线，false=设备不在线</returns>
    public async Task<bool> PingDeviceAsync()
    {
        if (!_isConnectedFunc())
            return false;

        try
        {
            byte[]? response = await _hidDriver.GetFeatureReportAsync(REPORT_ID_DEVICE_INFO);
            return response != null && response.Length >= 1;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 检查设备是否已连接
    /// </summary>
    /// <returns>true=已连接，false=未连接</returns>
    public bool IsConnected()
    {
        return _isConnectedFunc();
    }

    // ==================== 设备重连 ====================

    /// <summary>
    /// 重新连接设备
    /// 关闭旧句柄，等待设备稳定后重新打开
    /// </summary>
    /// <returns>true=重连成功，false=重连失败</returns>
    public async Task<bool> ReconnectDeviceAsync()
    {
        try
        {
            // 获取设备路径
            string devicePath = _getDevicePathFunc();
            if (string.IsNullOrEmpty(devicePath))
            {
                _logAction("WARNING", "重连失败：没有设备路径");
                return false;
            }

            // 关闭旧句柄
            try
            {
                _hidDriver.Close();
            }
            catch (Exception ex)
            {
                _logAction("WARNING", $"关闭旧句柄时异常: {ex.Message}");
            }

            // 等待设备稳定
            await Task.Delay(RECONNECT_STABLE_DELAY_MS);

            // 重新打开
            bool reopened = await _hidDriver.OpenAsync(devicePath);
            if (!reopened)
            {
                // 重新打开失败，标记为断开连接
                _logAction("ERROR", "重连失败：无法打开设备");
                _clearStateAction();
                _connectionChangedAction(false);
                return false;
            }

            _logAction("INFO", "设备重连成功");
            return true;
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"重连异常: {ex.GetType().Name} - {ex.Message}");
            _clearStateAction();
            _connectionChangedAction(false);
            return false;
        }
    }
}
