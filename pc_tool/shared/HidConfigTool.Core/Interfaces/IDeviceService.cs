using HidConfigTool.Core.Models;

using HidConfigTool.Core.Services;
using HidConfigTool.Core.Utils;

namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 设备运行模式
/// </summary>
public enum DeviceMode
{
    /// <summary>未知模式（未连接）</summary>
    Unknown = 0,
    /// <summary>应用程序模式（正常运行）</summary>
    Application = 1,
    /// <summary>Bootloader DFU 模式（等待升级）</summary>
    Bootloader = 2,
    /// <summary>Bootrom UF2 模式（官方烧录模式，非HID设备）</summary>
    BootromUF2 = 3
}

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
    /// 设备版本信息（固件版本、协议版本、配置版本等）
    /// </summary>
    DeviceVersionInfo? VersionInfo { get; }

    /// <summary>
    /// 兼容性检查结果
    /// </summary>
    CompatibilityResult Compatibility { get; }

    /// <summary>
    /// 是否允许配置读写（基于兼容性检查结果）
    /// </summary>
    bool IsConfigAccessAllowed { get; }

    /// <summary>
    /// 当前配置
    /// </summary>
    DeviceConfig? CurrentConfig { get; }

    /// <summary>
    /// 设备断开时自动导出错误日志
    /// </summary>
    bool AutoExportErrorLog { get; set; }

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
    /// 版本检查完成事件
    /// 参数：兼容性检查结果
    /// </summary>
    event EventHandler<CompatibilityResult>? VersionCheckCompleted;

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
    /// 循环切换 DPI 档位（立即生效并保存）
    /// </summary>
    /// <param name="slot">目标档位（0-3），或 0xFF 表示循环切换到下一档</param>
    Task<bool> SwitchDpiSlotAsync(byte slot = 0xFF);

    /// <summary>
    /// 切换按键层（立即生效，不写Flash）
    /// </summary>
    /// <param name="layer">目标层号（0-3）</param>
    /// <returns>true=切换成功，false=失败（无效层号或设备错误）</returns>
    Task<bool> SwitchLayerAsync(byte layer);

    /// <summary>
    /// 触发摇杆中心点校准
    /// </summary>
    Task<bool> CalibrateJoystickAsync(ushort sampleCount = 50);

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
    /// 获取摇杆校准状态（Report ID 23）
    /// 与固件 config_hid.c 中 REPORT_ID_JOYSTICK_CALIB 的 GET_REPORT 处理一致。
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>校准状态(State, CenterX, CenterY, Calibrated, Deadzone, Sensitivity)，失败返回null</returns>
    Task<(byte State, ushort CenterX, ushort CenterY, bool Calibrated, ushort Deadzone, ushort Sensitivity)?> GetJoystickCalibrationAsync(CancellationToken cancellationToken = default);

    /// <summary>
    /// 读取 CONFIG_EXT2 子块数据（Report ID 25，子块索引机制）
    /// 子块定义: 0=v6字段, 1=layer2_keymap[0..31], 2=layer2_keymap[32..63], 3=layer3_keymap[0..31], 4=layer3_keymap[32..63]
    /// </summary>
    /// <param name="subBlock">子块索引 (0-4)</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>子块数据，失败返回null</returns>
    Task<byte[]?> GetConfigExt2Async(byte subBlock, CancellationToken cancellationToken = default);

    /// <summary>
    /// 写入 CONFIG_EXT2 子块数据（Report ID 25）
    /// </summary>
    /// <param name="subBlock">子块索引 (0-4)</param>
    /// <param name="data">要写入的数据</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>是否成功</returns>
    Task<bool> SetConfigExt2Async(byte subBlock, byte[] data, CancellationToken cancellationToken = default);

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
    /// 设置性能监控任务超时阈值
    /// </summary>
    /// <param name="taskIndex">任务索引（0-15）</param>
    /// <param name="thresholdUs">超时阈值（微秒，0=禁用告警，最大25500us=25.5ms）</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> SetPerfThresholdAsync(int taskIndex, int thresholdUs, CancellationToken cancellationToken = default);

    /// <summary>
    /// 重启设备
    /// </summary>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> RebootAsync();

    /// <summary>
    /// 进入 BOOTSEL 烧录模式（兼容旧接口，实际进入自定义 Bootloader DFU 模式）
    /// </summary>
    /// <returns>true=成功，false=失败</returns>
    Task<bool> EnterBootselAsync();

    /// <summary>
    /// 软件触发进入官方 BOOTSEL UF2 模式（相当于按住 BOOTSEL 键上电）
    /// 设备会枚举为 RPI-RP2 U 盘，用户可手动拖拽 .uf2 文件升级
    /// </summary>
    /// <returns>true=命令已发送，false=失败</returns>
    Task<bool> EnterBootselModeAsync();

    /// <summary>
    /// 获取当前设备模式（应用程序 / Bootloader / 未知）
    /// </summary>
    DeviceMode CurrentDeviceMode { get; }

    /// <summary>
    /// 获取 Bootloader 信息（版本、应用区大小等）
    /// 仅在设备处于 Bootloader 模式时有效
    /// </summary>
    /// <returns>Bootloader 状态信息，非 Bootloader 模式返回 null</returns>
    Task<DfuStatus?> GetBootloaderInfoAsync();

    /// <summary>
    /// 从 Bootloader 模式启动应用程序
    /// 仅在设备处于 Bootloader 模式时有效
    /// </summary>
    /// <returns>true=成功启动应用，false=失败或非 Bootloader 模式</returns>
    Task<bool> BootToApplicationAsync();

    /// <summary>
    /// 验证固件文件是否适合当前设备
    /// 检查 UF2 起始地址、文件大小、格式等
    /// </summary>
    /// <param name="firmwarePath">固件文件路径</param>
    /// <returns>(是否有效, 错误消息)</returns>
    Task<(bool Valid, string Message)> ValidateFirmwareFileAsync(string firmwarePath);

    /// <summary>
    /// DFU 固件升级（完整流程：进入DFU→连接Bootloader→擦除→写入→校验→重启）
    /// </summary>
    /// <param name="firmwarePath">固件文件路径（.uf2 或 .bin）</param>
    /// <param name="progress">进度回调（0-100）</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>true=升级成功，false=失败</returns>
    Task<bool> DfuUpgradeAsync(string firmwarePath, IProgress<int>? progress = null, CancellationToken cancellationToken = default);

    /// <summary>
    /// DFU 状态变化事件（用于UI显示升级状态）
    /// 参数：状态消息
    /// </summary>
    event EventHandler<string>? DfuStatusChanged;

    /// <summary>
    /// 获取指定能力的精确版本号（能力版本精确化）
    /// 发送 CMD_GET_CAPABILITY_INFO 命令，固件返回该能力的版本号
    /// </summary>
    /// <param name="capabilityId">能力ID</param>
    /// <returns>能力版本（null=获取失败）</returns>
    Task<Version?> GetCapabilityVersionAsync(CapabilityId capabilityId);

    /// <summary>
    /// 检查指定能力是否可用（考虑依赖关系）
    /// </summary>
    /// <param name="capabilityId">能力ID</param>
    /// <returns>true=可用，false=不可用</returns>
    bool IsCapabilityAvailable(CapabilityId capabilityId);

    /// <summary>
    /// 当前设备的能力列表（连接后填充）
    /// </summary>
    IReadOnlyDictionary<CapabilityId, CapabilityInfo> Capabilities { get; }
}

