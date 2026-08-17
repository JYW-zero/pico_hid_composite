using HidConfigTool.Core.Models;

namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 设备服务接口
/// </summary>
public interface IDeviceService : IDisposable
{
    /// <summary>
    /// 是否连接
    /// </summary>
    bool IsConnected { get; }

    /// <summary>
    /// 设备名称
    /// </summary>
    string? DeviceName { get; }

    /// <summary>
    /// 固件版本
    /// </summary>
    string? FirmwareVersion { get; }

    /// <summary>
    /// 当前配置
    /// </summary>
    DeviceConfig? CurrentConfig { get; }

    /// <summary>
    /// 操作状态变化事件（用于UI显示状态提示）
    /// 参数：状态消息
    /// </summary>
    event EventHandler<string>? OperationStatusChanged;

    /// <summary>
    /// 设备连接状态变化事件
    /// 参数：是否已连接
    /// </summary>
    event EventHandler<bool>? DeviceConnectionChanged;

    /// <summary>
    /// 启动自动连接和心跳检测
    /// </summary>
    void StartAutoConnect();

    /// <summary>
    /// 停止自动连接和心跳检测
    /// </summary>
    void StopAutoConnect();

    /// <summary>
    /// 通知设备已插入（由 UI 层监听到设备插拔后调用）
    /// 会延迟一段时间再尝试连接，等待系统驱动初始化完成
    /// </summary>
    void NotifyDevicePluggedIn();

    /// <summary>
    /// 获取设备列表
    /// </summary>
    Task<IReadOnlyList<HidDeviceInfo>> GetDevicesAsync();

    /// <summary>
    /// 连接设备
    /// </summary>
    Task<bool> ConnectAsync(HidDeviceInfo deviceInfo);

    /// <summary>
    /// 断开连接
    /// </summary>
    void Disconnect();

    /// <summary>
    /// 设置 DPI
    /// </summary>
    /// <param name="dpiIndex">DPI 档位索引 (0-3)</param>
    Task<bool> SetDpiAsync(int dpiIndex);

    /// <summary>设置任意DPI值（100-6400）</summary>
    Task<bool> SetDpiValueAsync(ushort dpi, CancellationToken cancellationToken = default);

    /// <summary>
    /// 设置指针加速
    /// </summary>
    Task<bool> SetAccelerationAsync(bool enabled, double threshold, double ratio);

    /// <summary>
    /// 设置摇杆死区
    /// </summary>
    Task<bool> SetJoystickDeadzoneAsync(ushort deadzone);

    /// <summary>
    /// 实时设置摇杆死区（不写Flash，立即生效）
    /// </summary>
    Task<bool> SetJoystickDeadzoneRealtimeAsync(ushort deadzone);

    /// <summary>
    /// 设置编码器方向反转
    /// </summary>
    Task<bool> SetEncoderReverseAsync(bool reverse);

    /// <summary>
    /// 保存配置到设备
    /// </summary>
    Task<bool> SaveConfigAsync(DeviceConfig config);

    /// <summary>
    /// 恢复默认配置
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> ResetConfigAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 导出配置到文件
    /// </summary>
    Task<bool> ExportConfigAsync(string filePath);

    /// <summary>
    /// 从文件导入配置
    /// </summary>
    Task<bool> ImportConfigAsync(string filePath);

    /// <summary>
    /// 获取按键统计数据
    /// 返回64个键的按下次数
    /// </summary>
    Task<uint[]?> GetKeyStatsAsync();

    /// <summary>
    /// 清零按键统计
    /// </summary>
    Task<bool> ResetKeyStatsAsync();

    /// <summary>
    /// 读取一个宏的配置数据
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <returns>宏的原始字节数据，失败返回null</returns>
    Task<byte[]?> GetMacroAsync(byte macroId);

    /// <summary>
    /// 写入一个宏的配置数据
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <param name="data">宏的原始字节数据</param>
    /// <returns>是否成功</returns>
    Task<bool> SetMacroAsync(byte macroId, byte[] data);

    /// <summary>
    /// 播放指定宏
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <returns>是否成功发送命令</returns>
    Task<bool> PlayMacroAsync(byte macroId);

    /// <summary>
    /// 停止指定宏
    /// </summary>
    /// <param name="macroId">宏ID (0-7)，0xFF 表示停止所有宏</param>
    /// <returns>是否成功发送命令</returns>
    Task<bool> StopMacroAsync(byte macroId);

    /// <summary>
    /// 获取错误日志信息
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>错误日志信息，失败返回null</returns>
    Task<ErrorLogInfo?> GetErrorLogInfoAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 读取指定索引的错误日志
    /// </summary>
    /// <param name="index">日志索引</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>错误日志条目，失败返回null</returns>
    Task<ErrorLogEntry?> ReadErrorLogEntryAsync(byte index, CancellationToken cancellationToken = default);

    /// <summary>
    /// 获取所有错误日志
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>错误日志列表，失败返回null</returns>
    Task<IReadOnlyList<ErrorLogEntry>?> GetAllErrorLogsAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 清除所有错误日志
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> ClearErrorLogsAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 获取系统性能统计
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>系统性能统计，失败返回null</returns>
    Task<PerfSystemStat?> GetPerfSystemStatAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 获取指定任务的性能统计
    /// </summary>
    /// <param name="index">任务索引</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>任务性能统计，失败返回null</returns>
    Task<PerfTaskStat?> GetPerfTaskStatAsync(byte index, CancellationToken cancellationToken = default);

    /// <summary>
    /// 获取实时按键状态（64位bitmap，bit=1表示按下）
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>按键状态bitmap，失败返回null</returns>
    Task<ulong?> GetKeyStateAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 获取实时摇杆状态
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>摇杆状态(x, y, btn)，失败返回null</returns>
    Task<(sbyte X, sbyte Y, bool Button)?> GetJoystickStateAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 重置性能统计
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> ResetPerfStatsAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 设置性能监控开关
    /// </summary>
    /// <param name="enabled">true=开启，false=关闭</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> SetPerfMonitorEnabledAsync(bool enabled, CancellationToken cancellationToken = default);

    /// <summary>
    /// 重启设备
    /// </summary>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> RebootAsync();

    /// <summary>
    /// 进入 BOOTSEL 烧录模式
    /// </summary>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> EnterBootselAsync();
}

