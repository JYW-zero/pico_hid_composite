using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;
using HidConfigTool.Core.Utils;
using System.IO;

namespace HidConfigTool.Core.Services;

/// <summary>
/// HID 设备服务实现
/// 负责与设备通信，读写配置
/// 协议：基于 Feature 报告
///   Report ID 5/8/9 = 配置数据块（每块62字节，共1510字节，v8）
///   Report ID 6 = 设备信息（32字节）
///   Report ID 7 = 控制命令（3字节：cmd + param_low + param_high）
///   Report ID 21 = 配置扩展字段（18字节）
///   Report ID 24 = ACK响应（8字节，Input报告）
///   Report ID 25 = 配置扩展块2（63字节，子块索引）
/// </summary>
public class DeviceService : IDeviceService
{
    #region 错误记录机制

    /// <summary>
    /// 错误类型枚举
    /// </summary>
    public enum ErrorType
    {
        /// <summary>通信错误（HID读写失败、超时）</summary>
        Communication,
        /// <summary>协议错误（Report ID不匹配、数据长度异常、CRC校验失败）</summary>
        Protocol,
        /// <summary>配置错误（配置结构不一致、字段偏移错误、魔数不匹配）</summary>
        Config,
        /// <summary>命令错误（命令执行失败、ACK返回错误、未知命令）</summary>
        Command,
        /// <summary>DFU错误（升级失败、校验失败、固件无效）</summary>
        Dfu,
        /// <summary>设备错误（设备未连接、设备模式不匹配、版本不兼容）</summary>
        Device,
        /// <summary>内部错误（空引用、数组越界、异常未捕获）</summary>
        Internal
    }

    /// <summary>
    /// 错误记录项
    /// </summary>
    public class ErrorRecord
    {
        public DateTime Timestamp { get; set; }
        public ErrorType Type { get; set; }
        public string Location { get; set; } = string.Empty;
        public string Message { get; set; } = string.Empty;
        public string? Details { get; set; }
        public override string ToString() => $"[{Timestamp:HH:mm:ss}] [{Type}] {Location}: {Message}";
    }

    /// <summary>
    /// 错误发生事件（UI可订阅显示）
    /// </summary>
    public event EventHandler<ErrorRecord>? ErrorOccurred;

    /// <summary>
    /// 最近的错误记录列表（用于审计和优化）
    /// </summary>
    private readonly List<ErrorRecord> _errorHistory = new();
    private readonly object _errorLock = new();

    /// <summary>
    /// 日志文件写入锁（保护 File.AppendAllText 的并发访问，防止 IOException 和日志行交错）
    /// </summary>
    private readonly object _logLock = new();

    /// <summary>
    /// 记录错误（同时写入日志、历史列表、触发事件）
    /// </summary>
    private void RecordError(ErrorType type, string location, string message, string? details = null)
    {
        var record = new ErrorRecord
        {
            Timestamp = DateTime.Now,
            Type = type,
            Location = location,
            Message = message,
            Details = details
        };

        // 写入日志文件
        Log("ERROR", $"[{type}] {location}: {message}" + (details != null ? $" | {details}" : ""));

        // 加入历史列表（最多保留500条）
        lock (_errorLock)
        {
            _errorHistory.Add(record);
            if (_errorHistory.Count > 500)
                _errorHistory.RemoveAt(0);
        }

        // 触发事件（UI显示）
        try { ErrorOccurred?.Invoke(this, record); } catch { }
    }

    /// <summary>
    /// 获取错误历史记录（用于审计报告）
    /// </summary>
    public IReadOnlyList<ErrorRecord> GetErrorHistory()
    {
        lock (_errorLock) { return _errorHistory.ToList(); }
    }

    /// <summary>
    /// 清除错误历史
    /// </summary>
    public void ClearErrorHistory()
    {
        lock (_errorLock) { _errorHistory.Clear(); }
    }

    /// <summary>
    /// 获取错误统计（按类型分组）
    /// </summary>
    public Dictionary<ErrorType, int> GetErrorStatistics()
    {
        lock (_errorLock)
        {
            return _errorHistory.GroupBy(e => e.Type)
                .ToDictionary(g => g.Key, g => g.Count());
        }
    }

    #endregion

    #region 协议常量

    // 注意：所有 Report ID、命令 ID、协议常量已统一移至 ProtocolConstants.cs（单一权威来源）
    // 以下常量为兼容旧代码的别名，直接引用 ProtocolConstants 中的值

    // Report ID（分块协议，统一从 ProtocolConstants 引用）
    private const byte REPORT_ID_CONFIG_BLOCK0 = ProtocolConstants.ReportIdConfigBlock0;
    private const byte REPORT_ID_DEVICE_INFO = ProtocolConstants.ReportIdDeviceInfo;
    private const byte REPORT_ID_CONTROL = ProtocolConstants.ReportIdControl;
    private const byte REPORT_ID_CONFIG_BLOCK1 = ProtocolConstants.ReportIdConfigBlock1;
    private const byte REPORT_ID_CONFIG_BLOCK2 = ProtocolConstants.ReportIdConfigBlock2;
    private const byte REPORT_ID_PERF_SYSTEM = ProtocolConstants.ReportIdPerfSystem;
    private const byte REPORT_ID_PERF_TASK = ProtocolConstants.ReportIdPerfTask;
    private const byte REPORT_ID_KEY_STATE = ProtocolConstants.ReportIdKeyState;
    private const byte REPORT_ID_JOYSTICK_STATE = ProtocolConstants.ReportIdJoystickState;
    private const byte REPORT_ID_CONFIG_EXT = ProtocolConstants.ReportIdConfigExt;
    private const byte REPORT_ID_JOYSTICK_CALIB = ProtocolConstants.ReportIdJoystickCalib;
    private const byte REPORT_ID_CONFIG_EXT2 = ProtocolConstants.ReportIdConfigExt2;
    private const byte REPORT_ID_ACK = ProtocolConstants.ReportIdAck;

    // ACK 等待超时（毫秒）
    // 注意：USB 传输+固件处理+Input 报告轮询需要时间，500ms 容易偶发超时
    // 设置为 1500ms 确保稳定，同时不会让用户等待太久
    private const int ACK_TIMEOUT_MS = 1500;

    // 上位机支持的 HID 配置协议版本（统一从 ProtocolConstants 引用，避免重复定义）
    // v0.1: 初始版本，包含 Device Index、Software ID、标准化错误码
    private const ushort HOST_PROTOCOL_VERSION = ProtocolConstants.ProtocolVersion;

    // v3.1 协议：Device Index（有线版本固定 0xFF=直连设备，统一从 ProtocolConstants 引用）
    private const byte DEVICE_INDEX_DIRECT = ProtocolConstants.DeviceIndexDirect;

    // v3.1 协议：Software ID 计数器（1~15循环，0=广播/通知）
    private byte _nextSwId = 1;
    private readonly object _swIdLock = new();

    // 控制命令码（统一从 ProtocolConstants 引用）
    private const byte CMD_SAVE_CONFIG = ProtocolConstants.CmdSaveConfig;
    private const byte CMD_RESET_CONFIG = ProtocolConstants.CmdResetConfig;
    private const byte CMD_REBOOT = ProtocolConstants.CmdReboot;
    private const byte CMD_ENTER_DFU = ProtocolConstants.CmdEnterDfu;
    private const byte CMD_APPLY_CONFIG = ProtocolConstants.CmdApplyConfig;
    private const byte CMD_RESET_PERF = ProtocolConstants.CmdResetPerf;
    private const byte CMD_SET_PERF_ENABLE = ProtocolConstants.CmdSetPerfEnable;
    private const byte CMD_SET_JOYSTICK_DZ_RT = ProtocolConstants.CmdSetJoystickDzRt;
    private const byte CMD_UNLOCK_CONFIG = ProtocolConstants.CmdUnlockConfig;
    private const byte CMD_JOYSTICK_CALIBRATE = ProtocolConstants.CmdJoystickCalibrate;
    private const byte CMD_SWITCH_DPI = ProtocolConstants.CmdSwitchDpi;
    private const byte CMD_GET_VERSION = ProtocolConstants.CmdGetVersion;
    private const byte CMD_NEGOTIATE_VERSION = ProtocolConstants.CmdNegotiateVersion;
    private const byte CMD_SWITCH_LAYER = ProtocolConstants.CmdSwitchLayer;
    private const byte CMD_GET_CAPABILITY_INFO = ProtocolConstants.CmdGetCapabilityInfo;
    private const byte CMD_ENTER_BOOTSEL = ProtocolConstants.CmdEnterBootsel;
    private const byte CMD_SET_PERF_THRESHOLD = ProtocolConstants.CmdSetPerfThreshold;

    // 配置魔数（统一从 ProtocolConstants 引用）
    private const uint CONFIG_MAGIC = ProtocolConstants.ConfigMagic;

    // HID 操作超时（毫秒）
    private const int HID_TIMEOUT_MS = 5000;

    // 配置结构体大小
    private const int CONFIG_SIZE = ProtocolConstants.ConfigSize;  // v8: 与固件 device_config_t 对齐（1510，含 perf_thresholds）

    // 每个块的大小
    private const int BLOCK_SIZE = 62;

    // 配置字段偏移量（引用公共 ConfigOffsets 类，与固件 device_config_t 保持同步）
    private const int OFFSET_MAGIC = ConfigOffsets.Magic;
    private const int OFFSET_VERSION = ConfigOffsets.Version;
    private const int OFFSET_DPI = ConfigOffsets.Dpi;
    private const int OFFSET_JOYSTICK_DEADZONE = ConfigOffsets.JoystickDeadzone;
    private const int OFFSET_ENCODER_REVERSE = ConfigOffsets.EncoderReverse;
    private const int OFFSET_SEQ = ConfigOffsets.Seq;
    private const int OFFSET_KEYMAP = ConfigOffsets.Keymap;
    private const int OFFSET_FN_KEYMAP = ConfigOffsets.FnKeymap;
    // v3 新增字段（macro_data之后，crc32之前）
    private const int OFFSET_JOY_INV_X = ConfigOffsets.JoyInvertX;
    private const int OFFSET_JOY_INV_Y = ConfigOffsets.JoyInvertY;
    private const int OFFSET_ENC_STEPS = ConfigOffsets.EncoderSteps;
    private const int OFFSET_ENC_SCROLL = ConfigOffsets.EncoderScroll;
    private const int OFFSET_JOY_SENS = ConfigOffsets.JoystickSensitivity;
    private const int OFFSET_LENGTH = ConfigOffsets.Length;   // v4: 配置结构总长度
    private const int OFFSET_WRITE_COUNT = ConfigOffsets.WriteCount; // v4: 总写入次数
    // v5 新增字段：摇杆校准数据
    private const int OFFSET_JOY_CENTER_X = ConfigOffsets.JoystickCenterX;
    private const int OFFSET_JOY_CENTER_Y = ConfigOffsets.JoystickCenterY;
    private const int OFFSET_JOY_CALIBRATED = ConfigOffsets.JoystickCalibrated;
    // v6 新增字段：光学传感器高级配置
    private const int OFFSET_DPI_SLOT = ConfigOffsets.DpiSlot;      // 4档 DPI 值（每档2字节）
    private const int OFFSET_CURRENT_DPI_SLOT = ConfigOffsets.CurrentDpiSlot; // 当前 DPI 档位
    private const int OFFSET_LOD_LEVEL = ConfigOffsets.LodLevel;      // LOD 静默高度等级
    private const int OFFSET_ANGLE_SNAPPING = ConfigOffsets.AngleSnapping; // 角度捕捉开关
    private const int OFFSET_DPI_SWITCH_KEY = ConfigOffsets.DpiSwitchKey;  // DPI 切换按键索引
    // v7 新增字段：4层按键映射
    private const int OFFSET_LAYER2_KEYMAP = ConfigOffsets.Layer2Keymap;  // Layer2 按键映射（64字节）
    private const int OFFSET_LAYER3_KEYMAP = ConfigOffsets.Layer3Keymap;  // Layer3 按键映射（64字节）
    private const int OFFSET_CRC32 = ConfigOffsets.Crc32;  // v7: crc32 在所有字段之后

    #endregion

    private readonly IHidDriver _hidDriver;
    private readonly ConfigSerializerService _configSerializer = new();
    private MacroService? _macroService;
    private ErrorLogService? _errorLogService;
    private KeyStatsService? _keyStatsService;
    private DeviceConnectionService? _deviceConnectionService;
    private HidDeviceInfo? _currentDevice;
    private DeviceConfig? _currentConfig;
    private bool _disposed;
    private bool _autoExportErrorLog = true;

    /// <summary>
    /// 写入操作锁，防止并发调用
    /// </summary>
    private readonly SemaphoreSlim _writeLock = new(1, 1);

    /// <summary>
    /// 上次写入时间（用于写入冷却，防止疯狂写入）
    /// </summary>
    private DateTime _lastWriteTime = DateTime.MinValue;

    /// <summary>
    /// 最小写入间隔（毫秒），防止写入太频繁
    /// </summary>
    /// <summary>
    /// 配置写入最小间隔（毫秒），防止写入太频繁
    /// 注意：必须与固件 CONFIG_SAVE_MIN_INTERVAL_MS（500ms）保持一致，
    /// 否则固件会返回"too frequent"错误拒绝写入
    /// </summary>
    private const int MinWriteIntervalMs = 500;

    /// <summary>
    /// 连接操作锁，防止并发重连
    /// </summary>
    private readonly SemaphoreSlim _connectLock = new(1, 1);

    /// <summary>
    /// 状态锁，保护 _currentDevice / _currentConfig / _lastWriteTime 等共享状态的多线程访问
    /// </summary>
    private readonly object _stateLock = new();

    /// <summary>
    /// 配置解锁状态缓存
    /// 固件端解锁后30秒自动锁定，上位机缓存解锁状态避免每次保存都重新解锁
    /// 缓存有效期25秒（留5秒余量防止边界竞态）
    /// </summary>
    private volatile bool _configUnlocked;
    private DateTime _lastUnlockTime = DateTime.MinValue;
    private const int UnlockCacheValidSeconds = 25;

    /// <summary>
    /// 心跳检测取消令牌
    /// </summary>
    private CancellationTokenSource? _heartbeatCts;

    /// <summary>
    /// ACK 等待器字典（key=命令码，value=等待该命令ACK的TaskCompletionSource）
    /// 后台 Input 监听任务收到 ACK 后，会触发对应的等待器
    /// </summary>
    private readonly Dictionary<byte, TaskCompletionSource<byte[]>> _ackWaiters = new();

    /// <summary>
    /// ACK 等待器字典的锁
    /// </summary>
    private readonly object _ackLock = new();

    /// <summary>
    /// 输入监听器启停锁（保护 _inputListenerCts / _inputListenerTask 的原子操作）
    /// </summary>
    private readonly object _listenerLock = new();

    /// <summary>
    /// 自动重连启停锁（保护 _heartbeatCts 的原子操作）
    /// </summary>
    private readonly object _autoConnectLock = new();

    /// <summary>
    /// 后台 Input 报告监听取消令牌
    /// </summary>
    private CancellationTokenSource? _inputListenerCts;

    /// <summary>
    /// 后台 Input 报告监听任务
    /// </summary>
    private Task? _inputListenerTask;

    /// <summary>
    /// 心跳间隔（毫秒）
    /// </summary>
    private const int HeartbeatIntervalMs = 3000;

    /// <summary>
    /// 设备插入后延迟连接时间（毫秒）
    /// 等待 Windows 驱动初始化完成
    /// </summary>
    private const int PlugInDelayMs = 500;

    /// <summary>
    /// 操作状态变化事件
    /// </summary>
    public event EventHandler<string>? OperationStatusChanged;

    /// <summary>
    /// 设备连接状态变化事件
    /// </summary>
    public event EventHandler<bool>? DeviceConnectionChanged;

    /// <summary>
    /// 版本检查完成事件
    /// </summary>
    public event EventHandler<CompatibilityResult>? VersionCheckCompleted;

    /// <summary>
    /// 日志文件路径
    /// </summary>
    private readonly string _logFilePath;

    /// <summary>
    /// 日志保留天数
    /// </summary>
    private const int LogRetentionDays = 30;

    public bool IsConnected => _currentDevice != null;
    public string? DeviceName => _currentDevice?.ProductName;
    public string? FirmwareVersion { get; private set; }

    /// <summary>
    /// 当前设备运行模式（应用程序 / Bootloader / 未知）
    /// </summary>
    public DeviceMode CurrentDeviceMode { get; private set; } = DeviceMode.Unknown;

    /// <summary>
    /// 设备版本信息（固件版本、协议版本、配置版本等）
    /// </summary>
    public DeviceVersionInfo? VersionInfo { get; private set; }

    /// <summary>
    /// 兼容性检查结果
    /// </summary>
    public CompatibilityResult Compatibility { get; private set; } = CompatibilityResult.Unknown;

    /// <summary>
    /// 是否允许配置读写（基于兼容性检查结果）
    /// </summary>
    public bool IsConfigAccessAllowed => VersionComparer.IsConfigAccessAllowed(Compatibility);

    public DeviceConfig? CurrentConfig => _currentConfig;

    /// <summary>
    /// 设备断开时自动导出错误日志
    /// </summary>
    public bool AutoExportErrorLog
    {
        get => _autoExportErrorLog;
        set => _autoExportErrorLog = value;
    }

    /// <summary>
    /// 设备 VID
    /// </summary>
    public const ushort DeviceVendorId = 0x1209;  // 新固件 VID (pid.codes)

    /// <summary>
    /// 设备 PID
    /// </summary>
    public const ushort DeviceProductId = 0x4004;  // 应用程序 PID

    public DeviceService(IHidDriver hidDriver)
    {
        _hidDriver = hidDriver ?? throw new ArgumentNullException(nameof(hidDriver));

        // 初始化日志文件路径
        string logDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "HIDConfigTool", "Logs");
        Directory.CreateDirectory(logDir);
        _logFilePath = Path.Combine(logDir, $"device_service_{DateTime.Now:yyyyMMdd}.log");

        // 清理旧日志
        CleanOldLogs(logDir);

        // 初始化宏管理服务
        _macroService = new MacroService(
            _hidDriver,
            _writeLock,
            () => IsConnected,
            (level, message) => Log(level, message),
            async (cmd, param) => await SendControlCommandAsync(cmd, param));

        // 初始化错误日志服务
        _errorLogService = new ErrorLogService(
            _hidDriver,
            _writeLock,
            () => IsConnected,
            (level, message) => Log(level, message),
            async () => await UnlockConfigAsync(),
            async (cmd) => await SendControlCommandAsync(cmd));

        // 初始化按键统计服务
        _keyStatsService = new KeyStatsService(
            _hidDriver,
            () => IsConnected,
            (level, message) => Log(level, message),
            async (cmd) => await SendControlCommandAsync(cmd));

        // 初始化设备连接管理服务
        _deviceConnectionService = new DeviceConnectionService(
            _hidDriver,
            () => IsConnected,
            () =>
            {
                lock (_stateLock)
                {
                    return _currentDevice?.DevicePath ?? "";
                }
            },
            () =>
            {
                lock (_stateLock)
                {
                    _currentDevice = null;
                    _currentConfig = null;
                }
                FirmwareVersion = null;
                VersionInfo = null;
                Compatibility = CompatibilityResult.Unknown;
            },
            (connected) => DeviceConnectionChanged?.Invoke(this, connected),
            (level, message) => Log(level, message));

        Log("INFO", "DeviceService 初始化完成");
    }

    /// <summary>
    /// 清理过期日志文件
    /// </summary>
    private void CleanOldLogs(string logDir)
    {
        try
        {
            var cutoff = DateTime.Now.AddDays(-LogRetentionDays);
            var files = Directory.GetFiles(logDir, "*.log");
            int deletedCount = 0;

            foreach (var file in files)
            {
                try
                {
                    if (File.GetCreationTime(file) < cutoff)
                    {
                        File.Delete(file);
                        deletedCount++;
                    }
                }
                catch
                {
                    // 单个文件删除失败忽略
                }
            }

            if (deletedCount > 0)
            {
                Log("INFO", $"清理了 {deletedCount} 个过期日志文件");
            }
        }
        catch (Exception ex)
        {
            // 日志清理失败不影响主功能
            System.Diagnostics.Debug.WriteLine($"清理日志失败: {ex.Message}");
        }
    }

    /// <summary>
    /// 写日志
    /// </summary>
    private void Log(string level, string message)
    {
        try
        {
            string logLine = $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff}] [{level}] {message}";
            // 使用锁保护文件写入，防止多线程并发导致 IOException 或日志行交错
            lock (_logLock)
            {
                File.AppendAllText(_logFilePath, logLine + Environment.NewLine);
            }
        }
        catch
        {
            // 日志写入失败忽略，不影响主功能
        }
    }

    /// <summary>
    /// 触发操作状态变化事件
    /// </summary>
    private void OnOperationStatusChanged(string status)
    {
        try
        {
            OperationStatusChanged?.Invoke(this, status);
        }
        catch
        {
            // 事件处理异常忽略
        }
    }

    /// <summary>
    /// 获取设备列表
    /// </summary>
    public async Task<IReadOnlyList<HidDeviceInfo>> GetDevicesAsync()
    {
        // 如果已连接，直接返回当前设备，避免独占模式下重新枚举失败导致重复
        lock (_stateLock)
        {
            if (_currentDevice != null)
            {
                return new List<HidDeviceInfo> { _currentDevice };
            }
        }

        // 同时扫描应用程序和 Bootloader 设备（两种模式都需要能被上位机识别）
        try
        {
            var appDevices = await _hidDriver.FindDevicesAsync(DeviceVendorId, DeviceProductId);
            var bootloaderDevices = await _hidDriver.FindDevicesAsync((ushort)DfuUpdater.BOOTLOADER_VID, (ushort)DfuUpdater.BOOTLOADER_PID);

            var allDevices = appDevices.Concat(bootloaderDevices).ToList();

            // 只返回配置接口的设备（UsagePage = 0xFF00 Vendor Defined）
            // 过滤掉键盘、鼠标、消费者控制、游戏手柄等其他集合
            // Bootloader 设备的 UsagePage 也是 0xFF00（Vendor Defined）
            var configDevices = allDevices.Where(d => d.UsagePage == 0xFF00).ToList();

            return configDevices;
        }
        catch (Exception ex)
        {
            RecordError(ErrorType.Communication, "GetDevicesAsync", ex.Message, ex.StackTrace);
            return new List<HidDeviceInfo>();
        }
    }

    /// <summary>
    /// 连接设备
    /// </summary>
    public async Task<bool> ConnectAsync(HidDeviceInfo deviceInfo)
    {
        if (_currentDevice != null)
        {
            Disconnect();
        }

        try
        {
            Log("INFO", $"正在连接设备: {deviceInfo.DevicePath}");
            bool opened = await _hidDriver.OpenAsync(deviceInfo.DevicePath);
            if (!opened)
            {
                Log("ERROR", "打开设备失败");
                return false;
            }
            Log("INFO", "设备打开成功");

            // 检测设备运行模式（通过 VID/PID 区分应用程序和 Bootloader）
            if (deviceInfo.VendorId == DeviceVendorId && deviceInfo.ProductId == DeviceProductId)
            {
                CurrentDeviceMode = DeviceMode.Application;
                Log("INFO", "设备模式: 应用程序");
            }
            else if (deviceInfo.VendorId == DfuUpdater.BOOTLOADER_VID && deviceInfo.ProductId == DfuUpdater.BOOTLOADER_PID)
            {
                CurrentDeviceMode = DeviceMode.Bootloader;
                Log("INFO", "设备模式: Bootloader DFU");
                // Bootloader 模式下不需要读取配置，但需要设置当前设备信息并触发连接事件
                lock (_stateLock)
                {
                    _currentDevice = deviceInfo;
                    _currentConfig = new DeviceConfig();
                }
                VersionInfo = new DeviceVersionInfo();
                Compatibility = CompatibilityResult.Unknown;
                FirmwareVersion = "Bootloader DFU";
                DeviceConnectionChanged?.Invoke(this, true);
                Log("INFO", $"Bootloader连接成功: {deviceInfo.ProductName} (VID=0x{deviceInfo.VendorId:X4}, PID=0x{deviceInfo.ProductId:X4})");
                return true;
            }
            else
            {
                CurrentDeviceMode = DeviceMode.Unknown;
                Log("WARNING", $"未知设备模式: VID=0x{deviceInfo.VendorId:X4}, PID=0x{deviceInfo.ProductId:X4}");
            }

            // 启动后台 Input 报告监听（用于接收 ACK 等设备主动发送的报告）
            StartInputListener();

            // 读取配置
            var config = await ReadConfigFromDeviceAsync();
            if (config == null)
            {
                config = new DeviceConfig();
            }

            // 原子更新状态（在锁内设置设备和配置，避免多线程竞态）
            lock (_stateLock)
            {
                _currentDevice = deviceInfo;
                _currentConfig = config;
            }

            // 读取设备信息并进行版本检查
            try
            {
                byte[]? deviceInfoData = await _hidDriver.GetFeatureReportAsync(REPORT_ID_DEVICE_INFO);
                if (deviceInfoData != null && deviceInfoData.Length >= 12)
                {
                    // 解析设备信息（Report ID 6，16字节格式）
                    // byte[0] = report_id, byte[1-2] = firmware_version, byte[3-4] = config_version
                    // byte[5-6] = protocol_version, byte[7-8] = config_size, ...
                    VersionInfo = VersionComparer.ParseDeviceInfo(deviceInfoData);

                    // 设置固件版本字符串
                    FirmwareVersion = $"v{VersionInfo.FirmwareVersion.Major}.{VersionInfo.FirmwareVersion.Minor}.{VersionInfo.FirmwareVersion.Build}";

                    // 发送版本协商命令（CMD_NEGOTIATE_VERSION），与固件确认协议兼容性
                    // 参数：上位机支持的协议版本（高字节=主版本，低字节=次版本）
                    // 固件通过 ACK 返回实际支持的协议版本
                    try
                    {
                        bool negotiateOk = await SendControlCommandAsync(CMD_NEGOTIATE_VERSION, HOST_PROTOCOL_VERSION);
                        if (negotiateOk)
                        {
                            Log("INFO", $"版本协商成功：上位机=v{(HOST_PROTOCOL_VERSION >> 8) & 0xFF}.{HOST_PROTOCOL_VERSION & 0xFF}, 固件=v{VersionInfo.ProtocolVersion.Major}.{VersionInfo.ProtocolVersion.Minor}");
                        }
                        else
                        {
                            Log("WARNING", "版本协商失败或协议不兼容，将使用设备信息中的协议版本进行判断");
                        }
                    }
                    catch (Exception ex)
                    {
                        Log("WARNING", $"版本协商异常: {ex.Message}，将使用设备信息中的协议版本进行判断");
                    }

                    // 进行兼容性检查
                    Compatibility = VersionComparer.CheckCompatibility(VersionInfo);

                    // 记录版本检查结果
                    string compatMsg = VersionComparer.GetCompatibilityMessage(Compatibility, VersionInfo);
                    Log("INFO", $"版本检查: {compatMsg}");
                    Log("INFO", $"设备信息: {VersionInfo}");

                    // 能力协商摘要（Capability Negotiation）
                    string capSummary = VersionComparer.GetCapabilitySummary(VersionInfo);
                    Log("INFO", $"能力协商: {capSummary}");
                    if (VersionInfo.CapabilityBitmap != 0)
                    {
                        var availableCaps = VersionInfo.Capabilities.Values.Where(c => c.IsAvailable).Select(c => c.Name);
                        Log("INFO", $"可用能力: {string.Join(", ", availableCaps)}");
                    }

                    // 能力版本精确化：逐个查询每个能力的精确版本号
                    try
                    {
                        await RefreshCapabilityVersionsAsync();
                        // 刷新后重新输出能力摘要
                        string capSummary2 = VersionComparer.GetCapabilitySummary(VersionInfo);
                        Log("INFO", $"能力版本精确化: {capSummary2}");
                    }
                    catch (Exception ex)
                    {
                        Log("WARNING", $"能力版本刷新失败（不影响基础功能）: {ex.Message}");
                    }

                    // 触发版本检查完成事件
                    VersionCheckCompleted?.Invoke(this, Compatibility);

                    // 如果协议不兼容，提示用户
                    if (Compatibility == CompatibilityResult.ProtocolIncompatible)
                    {
                        OnOperationStatusChanged($"⚠️ {compatMsg}");
                    }
                    else if (Compatibility == CompatibilityResult.FirmwareOutdated)
                    {
                        OnOperationStatusChanged($"💡 {compatMsg}");
                    }
                }
                else
                {
                    // 旧固件或无法读取版本信息
                    VersionInfo = new DeviceVersionInfo();
                    Compatibility = CompatibilityResult.Unknown;
                    FirmwareVersion = "v1.0.0 (旧固件)";
                    Log("WARNING", "无法读取设备版本信息，可能是旧固件");
                    VersionCheckCompleted?.Invoke(this, Compatibility);
                }
            }
            catch (Exception ex)
            {
                VersionInfo = new DeviceVersionInfo();
                Compatibility = CompatibilityResult.Unknown;
                FirmwareVersion = "v1.0.0 (未知)";
                Log("ERROR", $"读取设备信息失败: {ex.Message}");
                VersionCheckCompleted?.Invoke(this, Compatibility);
            }

            // 触发连接状态变化事件
            DeviceConnectionChanged?.Invoke(this, true);

            Log("INFO", $"设备连接成功: {deviceInfo.ProductName} (VID=0x{deviceInfo.VendorId:X4}, PID=0x{deviceInfo.ProductId:X4})");
            return true;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"设备连接异常: {ex.Message}");
            lock (_stateLock)
            {
                _currentDevice = null;
                _currentConfig = null;
            }
            return false;
        }
    }

    /// <summary>
    /// 断开连接
    /// </summary>
    /// <summary>
    /// 自动导出错误日志到本地文件（设备断开时调用）
    /// </summary>
    private async Task AutoExportLogsAsync()
    {
        var lastErrorLogs = _errorLogService?.GetLastErrorLogs();
        if (!_autoExportErrorLog || lastErrorLogs == null || lastErrorLogs.Count == 0)
            return;

        try
        {
            string logDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "HIDConfigTool", "ErrorLogs");
            Directory.CreateDirectory(logDir);

            string fileName = $"error_log_{DateTime.Now:yyyyMMdd_HHmmss}.txt";
            string filePath = Path.Combine(logDir, fileName);

            var lines = new List<string>
            {
                "========================================",
                "  Pico HID 复合设备 - 自动导出错误日志",
                $"  导出时间: {DateTime.Now:yyyy-MM-dd HH:mm:ss}",
                $"  日志条数: {lastErrorLogs.Count}",
                "========================================",
                ""
            };

            foreach (var log in lastErrorLogs.Reverse())
            {
                lines.Add($"[{log.TimeFormatted}] [{log.LevelName}] [{log.Module}] {log.Message}");
            }

            await File.WriteAllLinesAsync(filePath, lines);
            Log("INFO", $"错误日志已自动导出: {filePath}");
        }
        catch (Exception ex)
        {
            Log("ERROR", $"自动导出错误日志失败: {ex.Message}");
        }
    }

    public void Disconnect()
    {
        bool hadDevice;
        lock (_stateLock)
        {
            hadDevice = _currentDevice != null;
            if (hadDevice)
            {
                _currentDevice = null;
                _currentConfig = null;
                // 重置解锁状态缓存
                _configUnlocked = false;
                _lastUnlockTime = DateTime.MinValue;
            }
        }

        if (hadDevice)
        {
            // 停止后台 Input 监听
            StopInputListener();

            // 取消所有挂起的 ACK 等待器，避免断开后仍需等待 500ms 超时
            // 遍历字典副本，因为 TrySetCanceled 可能触发 finally 块修改字典
            KeyValuePair<byte, TaskCompletionSource<byte[]>>[] pendingWaiters;
            lock (_ackLock)
            {
                pendingWaiters = _ackWaiters.ToArray();
                _ackWaiters.Clear();
            }
            foreach (var kv in pendingWaiters)
            {
                kv.Value.TrySetCanceled();
            }
            if (pendingWaiters.Length > 0)
            {
                Log("INFO", $"设备断开，取消 {pendingWaiters.Length} 个挂起的 ACK 等待");
            }

            // 重置设备模式
            CurrentDeviceMode = DeviceMode.Unknown;

            // 设备断开前自动导出错误日志
            _ = AutoExportLogsAsync();

            try
            {
                _hidDriver.Close();
            }
            catch { }
            FirmwareVersion = null;
            VersionInfo = null;
            Compatibility = CompatibilityResult.Unknown;

            // 触发连接状态变化事件
            DeviceConnectionChanged?.Invoke(this, false);
        }
    }

    /// <summary>
    /// 启动后台 Input 报告监听任务
    /// 持续读取设备发送的 Input 报告（如 ACK），并分发给等待者
    /// </summary>
    private void StartInputListener()
    {
        lock (_listenerLock)
        {
            StopInputListenerInternal(); // 先停止旧的监听（在锁内调用，避免重复获取锁）

            _inputListenerCts = new CancellationTokenSource();
            var token = _inputListenerCts.Token;
            _inputListenerTask = Task.Run(() => InputListenerLoopAsync(token), token);
            Log("DEBUG", "后台 Input 报告监听已启动");
        }
    }

    /// <summary>
    /// 停止后台 Input 报告监听任务
    /// </summary>
    private void StopInputListener()
    {
        lock (_listenerLock)
        {
            StopInputListenerInternal();
        }
    }

    /// <summary>
    /// 停止后台 Input 报告监听任务的内部实现（调用方必须已持有 _listenerLock）
    /// </summary>
    private void StopInputListenerInternal()
    {
        // 取消所有等待中的 ACK
        lock (_ackLock)
        {
            foreach (var waiter in _ackWaiters.Values)
            {
                waiter.TrySetCanceled();
            }
            _ackWaiters.Clear();
        }

        if (_inputListenerCts != null)
        {
            try
            {
                _inputListenerCts.Cancel();
            }
            catch { }

            try
            {
                _inputListenerTask?.Wait(500);
            }
            catch { }

            _inputListenerCts.Dispose();
            _inputListenerCts = null;
            _inputListenerTask = null;
            Log("DEBUG", "后台 Input 报告监听已停止");
        }
    }

    /// <summary>
    /// 后台 Input 报告监听循环
    /// 持续读取 Input 报告，收到 ACK 后触发对应的等待器
    /// </summary>
    private async Task InputListenerLoopAsync(CancellationToken token)
    {
        try
        {
            while (!token.IsCancellationRequested)
            {
                try
                {
                    byte[]? report = await _hidDriver.ReadInputReportAsync(token);
                    if (report == null || report.Length == 0)
                    {
                        // 空报告，短暂等待后重试
                        await Task.Delay(10, token);
                        continue;
                    }

                    // 检查是否是 ACK 报告（Report ID = 24）
                    if (report[0] == REPORT_ID_ACK && report.Length >= 2)
                    {
                        byte cmd = report[1];
                        TaskCompletionSource<byte[]>? waiter = null;
                        lock (_ackLock)
                        {
                            if (_ackWaiters.TryGetValue(cmd, out waiter))
                            {
                                _ackWaiters.Remove(cmd);
                            }
                        }

                        if (waiter != null)
                        {
                            Log("DEBUG", $"收到 ACK: cmd=0x{cmd:X2}, status={report[2]}, error_code={report[3]}");
                            waiter.TrySetResult(report);
                        }
                        else
                        {
                            Log("DEBUG", $"收到无等待者的 ACK: cmd=0x{cmd:X2}, 已丢弃");
                        }
                    }
                    // 其他 Input 报告（如键盘/鼠标输入）忽略
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Log("ERROR", $"Input 监听异常: {ex.Message}");
                    // 短暂延迟后重试，避免频繁异常
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
        catch (OperationCanceledException)
        {
            // 正常取消
        }
        Log("DEBUG", "Input 监听循环已退出");
    }

    /// <summary>
    /// 从设备读取配置（分块读取）
    /// </summary>
    private async Task<DeviceConfig?> ReadConfigFromDeviceAsync()
    {
        try
        {
            // 分 3 块读取（使用带超时和异常处理的包装方法）
            byte[]? block0 = await GetFeatureReportWithTimeoutAsync(REPORT_ID_CONFIG_BLOCK0);
            byte[]? block1 = await GetFeatureReportWithTimeoutAsync(REPORT_ID_CONFIG_BLOCK1);
            byte[]? block2 = await GetFeatureReportWithTimeoutAsync(REPORT_ID_CONFIG_BLOCK2);

            if (block0 == null || block1 == null || block2 == null)
            {
                RecordError(ErrorType.Communication, "ReadConfigFromDeviceAsync",
                    "配置块读取失败",
                    $"block0={(block0 != null ? "OK" : "null")}, block1={(block1 != null ? "OK" : "null")}, block2={(block2 != null ? "OK" : "null")}");
                return null;
            }

            // 拼接
            byte[] fullData = new byte[CONFIG_SIZE];
            int copyLen0 = Math.Min(block0.Length, BLOCK_SIZE);
            int copyLen1 = Math.Min(block1.Length, BLOCK_SIZE);
            int copyLen2 = Math.Min(block2.Length, BLOCK_SIZE);

            Array.Copy(block0, 0, fullData, 0, Math.Min(copyLen0, CONFIG_SIZE));
            if (BLOCK_SIZE < CONFIG_SIZE)
            {
                int offset1 = BLOCK_SIZE;
                int len1 = Math.Min(copyLen1, CONFIG_SIZE - offset1);
                if (len1 > 0)
                    Array.Copy(block1, 0, fullData, offset1, len1);
            }
            if (BLOCK_SIZE * 2 < CONFIG_SIZE)
            {
                int offset2 = BLOCK_SIZE * 2;
                int len2 = Math.Min(copyLen2, CONFIG_SIZE - offset2);
                if (len2 > 0)
                    Array.Copy(block2, 0, fullData, offset2, len2);
            }

            // 读取v4扩展字段（偏移1326+，18字节，包含length+write_count+crc32）
            byte[]? extBlock = await GetFeatureReportWithTimeoutAsync(REPORT_ID_CONFIG_EXT);
            if (extBlock != null)
            {
                int extLen = Math.Min(extBlock.Length, 18);
                if (extLen > 0 && OFFSET_JOY_INV_X + extLen <= CONFIG_SIZE)
                {
                    Array.Copy(extBlock, 0, fullData, OFFSET_JOY_INV_X, extLen);
                }
            }

            // 读取v6/v7扩展字段（CONFIG_EXT2，5个子块，覆盖偏移1346-1487）
            // 子块定义: 0=v6字段(1346,14B), 1=layer2_keymap[0..31](1360,32B),
            //           2=layer2_keymap[32..63](1392,32B), 3=layer3_keymap[0..31](1424,32B),
            //           4=layer3_keymap[32..63](1456,32B)
            var ext2Blocks = new (byte SubBlock, int Offset, int Length)[]
            {
                (0, ProtocolConstants.ConfigExt2V6Offset, ProtocolConstants.ConfigExt2V6Length),
                (1, ProtocolConstants.ConfigExt2Layer2Part1Offset, ProtocolConstants.ConfigExt2KeymapPartLength),
                (2, ProtocolConstants.ConfigExt2Layer2Part2Offset, ProtocolConstants.ConfigExt2KeymapPartLength),
                (3, ProtocolConstants.ConfigExt2Layer3Part1Offset, ProtocolConstants.ConfigExt2KeymapPartLength),
                (4, ProtocolConstants.ConfigExt2Layer3Part2Offset, ProtocolConstants.ConfigExt2KeymapPartLength),
            };
            foreach (var (subBlock, offset, length) in ext2Blocks)
            {
                byte[]? ext2Data = await GetConfigExt2Async(subBlock);
                if (ext2Data != null)
                {
                    int copyLen = Math.Min(ext2Data.Length, length);
                    if (copyLen > 0 && offset + copyLen <= CONFIG_SIZE)
                    {
                        Array.Copy(ext2Data, 0, fullData, offset, copyLen);
                    }
                }
            }

            return _configSerializer.ParseConfig(fullData);
        }
        catch (Exception ex)
        {
            RecordError(ErrorType.Internal, "ReadConfigFromDeviceAsync",
                "读取配置异常", ex.Message);
            return null;
        }
    }


    /// <summary>
    /// 写入配置到设备（分块写入 + 应用命令 + 重试重连容错）
    /// 由于RP2350写Flash时CPU会挂起，可能导致USB句柄失效
    /// 采用重试+重连机制确保写入可靠
    /// </summary>
    private async Task<bool> WriteConfigToDeviceAsync(DeviceConfig config)
    {
        // 重入保护，防止并发调用
        if (!await _writeLock.WaitAsync(0))
        {
            Log("WARNING", "写入操作正在进行中，忽略重复调用");
            OnOperationStatusChanged("正在保存配置，请稍候...");
            return false;
        }

        try
        {
            // 版本兼容性检查：协议不兼容时禁止写入
            if (!IsConfigAccessAllowed)
            {
                string compatMsg = VersionComparer.GetCompatibilityMessage(Compatibility, VersionInfo);
                Log("ERROR", $"配置写入被拒绝：{compatMsg}");
                OnOperationStatusChanged($"❌ 无法写入配置：{compatMsg}");
                return false;
            }

            // 写入冷却：防止写入太频繁
            // 注意：_lastWriteTime 的读写由 _writeLock（SemaphoreSlim）保证串行化，
            // WriteConfigToDeviceAsync 是唯一访问 _lastWriteTime 的方法，不会并发
            var timeSinceLastWrite = DateTime.Now - _lastWriteTime;
            if (timeSinceLastWrite.TotalMilliseconds < MinWriteIntervalMs)
            {
                int waitMs = MinWriteIntervalMs - (int)timeSinceLastWrite.TotalMilliseconds;
                Log("INFO", $"写入冷却：等待 {waitMs}ms");
                await Task.Delay(waitMs);
            }

            const int maxRetries = 3;
            byte[] data = _configSerializer.SerializeConfig(config);

            Log("INFO", $"开始写入配置，大小: {data.Length} 字节");
            OnOperationStatusChanged("正在保存配置到设备...");

            // 写入前预检设备状态
            bool pingOk = await PingDeviceAsync();
            if (!pingOk)
            {
                Log("WARNING", "设备预检失败，尝试重连...");
                OnOperationStatusChanged("设备响应异常，正在重新连接...");
                bool reconnected = await ReconnectDeviceAsync();
                if (!reconnected)
                {
                    Log("ERROR", "设备预检失败且重连失败");
                    OnOperationStatusChanged("设备未响应，请检查连接");
                    return false;
                }
            }

            // 解锁配置写入（固件默认锁定，需连续发送3次解锁命令）
            if (!await UnlockConfigAsync())
            {
                Log("ERROR", "配置解锁失败，无法写入");
                OnOperationStatusChanged("配置解锁失败，请重试");
                return false;
            }

            for (int attempt = 0; attempt < maxRetries; attempt++)
            {
                try
                {
                    // 分3块写入
                    byte[] block0 = new byte[BLOCK_SIZE];
                    byte[] block1 = new byte[BLOCK_SIZE];
                    byte[] block2 = new byte[BLOCK_SIZE];

                    Array.Copy(data, 0, block0, 0, Math.Min(BLOCK_SIZE, data.Length));
                    if (data.Length > BLOCK_SIZE)
                        Array.Copy(data, BLOCK_SIZE, block1, 0, Math.Min(BLOCK_SIZE, data.Length - BLOCK_SIZE));
                    if (data.Length > BLOCK_SIZE * 2)
                        Array.Copy(data, BLOCK_SIZE * 2, block2, 0, Math.Min(BLOCK_SIZE, data.Length - BLOCK_SIZE * 2));

                    bool ok0 = await SendFeatureReportWithTimeoutAsync(REPORT_ID_CONFIG_BLOCK0, block0);
                    bool ok1 = await SendFeatureReportWithTimeoutAsync(REPORT_ID_CONFIG_BLOCK1, block1);
                    bool ok2 = await SendFeatureReportWithTimeoutAsync(REPORT_ID_CONFIG_BLOCK2, block2);

                    // 发送v4扩展字段（偏移1326+，18字节，包含length+write_count+crc32）
                    byte[] extBlock = new byte[18];
                    if (data.Length >= OFFSET_JOY_INV_X + 18)
                    {
                        Array.Copy(data, OFFSET_JOY_INV_X, extBlock, 0, 18);
                    }
                    bool okExt = await SendFeatureReportWithTimeoutAsync(REPORT_ID_CONFIG_EXT, extBlock);

                    // 发送v6/v7扩展字段（CONFIG_EXT2，5个子块，覆盖偏移1346-1487）
                    var ext2Blocks = new (byte SubBlock, int Offset, int Length)[]
                    {
                        (0, 1346, 14),
                        (1, 1360, 32),
                        (2, 1392, 32),
                        (3, 1424, 32),
                        (4, 1456, 32),
                    };
                    bool okExt2 = true;
                    foreach (var (subBlock, offset, length) in ext2Blocks)
                    {
                        if (offset + length > data.Length) continue;
                        byte[] subData = new byte[length];
                        Array.Copy(data, offset, subData, 0, length);
                        bool ok = await SetConfigExt2Async(subBlock, subData);
                        if (!ok) okExt2 = false;
                    }

                    if (!ok0 || !ok1 || !ok2 || !okExt || !okExt2)
                    {
                        string failBlocks = $"{(!ok0 ? "0 " : "")}{(!ok1 ? "1 " : "")}{(!ok2 ? "2 " : "")}{(!okExt ? "EXT " : "")}{(!okExt2 ? "EXT2 " : "")}".Trim();
                        Log("WARNING", $"第 {attempt + 1} 次写入失败，块 {failBlocks} 写入失败");

                        // 写入块失败，重试前先重连
                        if (attempt < maxRetries - 1)
                        {
                            // 指数退避：100ms → 200ms → 400ms
                            int delayMs = 100 * (int)Math.Pow(2, attempt);
                            OnOperationStatusChanged($"写入失败，{delayMs}ms 后重试 ({attempt + 1}/{maxRetries})...");
                            Log("INFO", $"等待 {delayMs}ms 后重试...");
                            await Task.Delay(delayMs);

                            Log("INFO", "尝试重新连接设备...");
                            await ReconnectDeviceAsync();
                            continue;
                        }
                        Log("ERROR", "配置写入失败，已达最大重试次数");
                        OnOperationStatusChanged("配置保存失败，请检查设备连接");
                        return false;
                    }

                    // 等待一下，然后发送应用命令
                    await Task.Delay(10);
                    bool applied = await SendControlCommandAsync(CMD_APPLY_CONFIG);

                    if (applied)
                    {
                        lock (_stateLock)
                        {
                            _currentConfig = config;
                            _lastWriteTime = DateTime.Now;
                        }
                        Log("INFO", "配置写入成功，正在保存到Flash...");

                        // 【关键】写Flash会导致CPU挂起，USB句柄可能失效
                        // 主动等待Flash操作完成，然后重连恢复通信
                        await Task.Delay(100);
                        Log("INFO", "Flash写入完成，重新连接设备...");
                        OnOperationStatusChanged("正在恢复设备连接...");

                        bool reconnected = await ReconnectDeviceAsync();
                        if (reconnected)
                        {
                            Log("INFO", "配置写入完成，设备重连成功");
                            OnOperationStatusChanged("配置已保存");
                        }
                        else
                        {
                            Log("WARNING", "配置写入成功，但设备重连失败");
                            OnOperationStatusChanged("配置已保存，设备连接已断开");
                        }

                        return true;
                    }
                    else
                    {
                        Log("WARNING", $"第 {attempt + 1} 次应用命令失败");

                        // 应用命令失败，重试
                        if (attempt < maxRetries - 1)
                        {
                            int delayMs = 100 * (int)Math.Pow(2, attempt);
                            OnOperationStatusChanged($"写入失败，{delayMs}ms 后重试 ({attempt + 1}/{maxRetries})...");
                            Log("INFO", $"等待 {delayMs}ms 后重试...");
                            await Task.Delay(delayMs);

                            Log("INFO", "尝试重新连接设备...");
                            await ReconnectDeviceAsync();
                            continue;
                        }
                        Log("ERROR", "应用命令失败，已达最大重试次数");
                        OnOperationStatusChanged("配置保存失败，请检查设备连接");
                        return false;
                    }
                }
                catch (TimeoutException ex)
                {
                    // 超时异常 → 重试，不一定需要重连
                    Log("WARNING", $"第 {attempt + 1} 次写入超时: {ex.Message}");
                    if (attempt < maxRetries - 1)
                    {
                        int delayMs = 100 * (int)Math.Pow(2, attempt);
                        OnOperationStatusChanged($"写入超时，{delayMs}ms 后重试 ({attempt + 1}/{maxRetries})...");
                        await Task.Delay(delayMs);
                        continue;
                    }
                    Log("ERROR", "写入超时，已达最大重试次数");
                    OnOperationStatusChanged("配置保存失败：通信超时");
                    return false;
                }
                catch (IOException ex)
                {
                    // IO异常 → 句柄失效，重连后重试
                    Log("WARNING", $"第 {attempt + 1} 次IO异常: {ex.Message}");
                    if (attempt < maxRetries - 1)
                    {
                        int delayMs = 100 * (int)Math.Pow(2, attempt);
                        OnOperationStatusChanged($"通信中断，{delayMs}ms 后重连重试 ({attempt + 1}/{maxRetries})...");
                        Log("INFO", "设备通信中断，尝试重新连接...");
                        await Task.Delay(delayMs);
                        await ReconnectDeviceAsync();
                        continue;
                    }
                    Log("ERROR", "IO异常，已达最大重试次数");
                    OnOperationStatusChanged("配置保存失败：通信中断");
                    return false;
                }
                catch (Exception ex)
                {
                    // 其他异常 → 记录并重试
                    Log("ERROR", $"第 {attempt + 1} 次写入异常: {ex.GetType().Name} - {ex.Message}");
                    if (attempt < maxRetries - 1)
                    {
                        int delayMs = 100 * (int)Math.Pow(2, attempt);
                        OnOperationStatusChanged($"写入异常，{delayMs}ms 后重试 ({attempt + 1}/{maxRetries})...");
                        await Task.Delay(delayMs);
                        continue;
                    }
                    Log("ERROR", "写入异常，已达最大重试次数");
                    OnOperationStatusChanged("配置保存失败：未知错误");
                    return false;
                }
            }

            return false;
        }
        finally
        {
            _writeLock.Release();
        }
    }
    private Task<bool> ReconnectDeviceAsync() => _deviceConnectionService!.ReconnectDeviceAsync();

    /// <summary>
    /// 设备状态预检（Ping）
    /// 通过读取控制状态报告确认设备是否在线
    /// </summary>
    private Task<bool> PingDeviceAsync() => _deviceConnectionService!.PingDeviceAsync();

    /// <summary>
    /// 解锁配置写入（需连续发送3次，5秒内）
    /// 固件默认锁定配置，防止恶意程序篡改。写入配置前必须先解锁。
    /// </summary>
    private async Task<bool> UnlockConfigAsync()
    {
        // 检查解锁状态缓存：如果在25秒内已解锁，直接返回（固件端30秒自动锁定，留5秒余量）
        if (_configUnlocked && (DateTime.Now - _lastUnlockTime).TotalSeconds < UnlockCacheValidSeconds)
        {
            Log("DEBUG", $"配置仍在解锁有效期内（剩余{UnlockCacheValidSeconds - (int)(DateTime.Now - _lastUnlockTime).TotalSeconds}秒），跳过重复解锁");
            return true;
        }

        Log("INFO", "正在解锁配置写入...");
        for (int i = 0; i < 3; i++)
        {
            bool result = await SendControlCommandAsync(CMD_UNLOCK_CONFIG);
            if (!result)
            {
                Log("ERROR", $"解锁配置失败（第{i + 1}/3次）");
                _configUnlocked = false;
                return false;
            }
            if (i < 2) await Task.Delay(200);
        }

        // 解锁成功，更新缓存
        _configUnlocked = true;
        _lastUnlockTime = DateTime.Now;
        Log("INFO", "配置已解锁（30秒后自动锁定）");
        return true;
    }

    /// <summary>
    /// 判断命令是否会导致设备重启或进入DFU（无法发送ACK）
    /// 这些命令发送成功即返回，不等待ACK
    /// </summary>
    private static bool IsRebootCommand(byte command)
    {
        return command == CMD_RESET_CONFIG   // 恢复出厂配置后会重启
            || command == CMD_REBOOT          // 重启设备
            || command == CMD_ENTER_DFU;      // 进入DFU模式
    }

    /// <summary>
    /// 发送控制命令并等待 ACK 响应（带超时）
    /// 固件在收到控制命令后会通过 Input 报告（Report ID 24）返回执行结果。
    /// ACK 格式: [report_id=24, cmd, status(0=成功), error_code, param_low, param_high, reserved[2]]
    /// 后台 Input 监听任务持续读取 ACK 并触发对应的等待器。
    /// 超时未收到 ACK 视为失败（不向后兼容，要求固件必须发送ACK）。
    /// </summary>
    /// <param name="data">命令数据（第一个字节为命令码）</param>
    /// <param name="expectedCmd">期望的 ACK 命令码（用于匹配）</param>
    /// <returns>命令是否执行成功</returns>
    private async Task<bool> SendControlCommandWithAckAsync(byte[] data, byte expectedCmd)
    {
        try
        {
            // v3.1 协议：分配 Software ID（1~15循环，用于匹配请求响应）
            byte swId;
            lock (_swIdLock)
            {
                swId = _nextSwId;
                _nextSwId++;
                if (_nextSwId > 15) _nextSwId = 1;  // 0 保留给广播/通知
            }

            // v3.1 命令格式: [device_idx, cmd, sw_id, param_low, param_high, reserved[3]]
            // 有线版本 device_idx 固定为 0xFF（直连设备）
            byte[] cmdData = new byte[8];
            cmdData[0] = DEVICE_INDEX_DIRECT;  // device_idx
            cmdData[1] = expectedCmd;           // cmd
            cmdData[2] = swId;                  // sw_id
            // 复制参数（data[0]=cmd, data[1..]=param）
            if (data.Length >= 2) cmdData[3] = data[1];  // param_low
            if (data.Length >= 3) cmdData[4] = data[2];  // param_high

            // 1. 发送 Output Report 命令（中断传输，速度更快）
            bool sent = await _hidDriver.SendOutputReportAsync(REPORT_ID_CONTROL, cmdData);
            if (!sent)
            {
                RecordError(ErrorType.Communication, "SendControlCommandWithAckAsync",
                    $"命令 0x{expectedCmd:X2} 发送失败", "Output报告写入失败或设备断开");
                return false;
            }

            // 2. 短暂等待固件处理命令
            await Task.Delay(5);

            // 3. GET_REPORT(24) 读取响应，支持 BUSY 重试（最多5次，每次间隔10ms）
            //    固件端如果命令尚未处理完成，返回 status=CONFIG_HID_ERR_BUSY(8)，
            //    上位机应重试，避免读到旧响应或全零初始响应（竞态条件防护）
            const int BUSY_ERROR_CODE = 8;  // CONFIG_HID_ERR_BUSY
            const int MAX_BUSY_RETRIES = 5;
            byte[]? response = null;

            for (int retry = 0; retry <= MAX_BUSY_RETRIES; retry++)
            {
                response = await _hidDriver.GetFeatureReportAsync(REPORT_ID_ACK);
                if (response == null || response.Length < 6)
                {
                    RecordError(ErrorType.Protocol, "SendControlCommandWithAckAsync",
                        $"命令 0x{expectedCmd:X2} 读取响应失败", "响应为空或长度不足");
                    return false;
                }

                // 检查是否 BUSY（命令尚未处理完成）
                if (response[0] == BUSY_ERROR_CODE && retry < MAX_BUSY_RETRIES)
                {
                    await Task.Delay(10);
                    continue;
                }
                break;
            }

            // response 此时一定不为 null（循环内已检查），但添加显式检查满足编译器静态分析
            if (response == null)
            {
                RecordError(ErrorType.Protocol, "SendControlCommandWithAckAsync",
                    $"命令 0x{expectedCmd:X2} 读取响应失败", "重试后响应仍为空");
                return false;
            }

            // 4. 解析 v3.1 响应格式: [status, cmd, sw_id, device_idx, param_low, param_high, reserved[2]]
            byte status = response[0];
            byte respCmd = response[1];
            byte respSwId = response[2];

            // 验证 SW ID 匹配（防止读到上一条命令的残留响应）
            if (respSwId != swId)
            {
                RecordError(ErrorType.Protocol, "SendControlCommandWithAckAsync",
                    $"命令 0x{expectedCmd:X2} SW ID不匹配", $"期望={swId}, 实际={respSwId}");
                return false;
            }

            if (respCmd != expectedCmd)
            {
                RecordError(ErrorType.Protocol, "SendControlCommandWithAckAsync",
                    $"命令 0x{expectedCmd:X2} 响应命令不匹配", $"期望={expectedCmd}, 实际={respCmd}");
                return false;
            }

            bool success = status == 0;
            if (!success)
            {
                byte errorCode = status;
                ushort param = response.Length >= 6 ? (ushort)(response[4] | (response[5] << 8)) : (ushort)0;
                RecordError(ErrorType.Command, "SendControlCommandWithAckAsync",
                    $"命令 0x{expectedCmd:X2} 执行失败",
                    $"error_code=0x{errorCode:X2}, param=0x{param:X4}, sw_id={swId}");
            }
            return success;
        }
        catch (Exception ex)
        {
            RecordError(ErrorType.Internal, "SendControlCommandWithAckAsync",
                $"命令 0x{expectedCmd:X2} 异常", ex.Message);
            return false;
        }
    }

    private async Task<bool> SendControlCommandAsync(byte command)
    {
        try
        {
            byte[] data = new byte[1] { command };
            // 会导致设备重启/进入DFU的命令不等待响应（设备会立即重启）
            if (IsRebootCommand(command))
            {
                return await _hidDriver.SendOutputReportAsync(REPORT_ID_CONTROL, data);
            }
            return await SendControlCommandWithAckAsync(data, command);
        }
        catch
        {
            return false;
        }
    }

    private async Task<bool> SendControlCommandAsync(byte command, byte param)
    {
        try
        {
            byte[] data = new byte[2] { command, param };
            // 会导致设备重启/进入DFU的命令不等待ACK
            if (IsRebootCommand(command))
            {
                return await _hidDriver.SendOutputReportAsync(REPORT_ID_CONTROL, data);
            }
            return await SendControlCommandWithAckAsync(data, command);
        }
        catch
        {
            return false;
        }
    }

    private async Task<bool> SendControlCommandAsync(byte command, ushort param)
    {
        try
        {
            byte[] data = new byte[3] { command, (byte)(param & 0xFF), (byte)((param >> 8) & 0xFF) };
            // 会导致设备重启/进入DFU的命令不等待ACK
            if (IsRebootCommand(command))
            {
                return await _hidDriver.SendOutputReportAsync(REPORT_ID_CONTROL, data);
            }
            return await SendControlCommandWithAckAsync(data, command);
        }
        catch
        {
            return false;
        }
    }

    #region 能力协商（Capability Negotiation）

    /// <summary>
    /// 当前设备的能力列表（连接后填充）
    /// </summary>
    public IReadOnlyDictionary<CapabilityId, CapabilityInfo> Capabilities => VersionInfo?.Capabilities ?? new Dictionary<CapabilityId, CapabilityInfo>();

    /// <summary>
    /// 检查指定能力是否可用（考虑依赖关系）
    /// </summary>
    public bool IsCapabilityAvailable(CapabilityId capabilityId)
    {
        return VersionInfo?.IsCapabilityAvailable(capabilityId) ?? false;
    }

    /// <summary>
    /// 获取指定能力的精确版本号（能力版本精确化）
    /// 发送 CMD_GET_CAPABILITY_INFO 命令，固件返回该能力的版本号
    /// </summary>
    /// <param name="capabilityId">能力ID</param>
    /// <returns>能力版本（null=获取失败）</returns>
    public async Task<Version?> GetCapabilityVersionAsync(CapabilityId capabilityId)
    {
        try
        {
            byte[] data = new byte[2] { CMD_GET_CAPABILITY_INFO, (byte)capabilityId };
            var (success, param) = await SendControlCommandWithAckParamAsync(data, CMD_GET_CAPABILITY_INFO);
            if (success)
            {
                int major = (param >> 8) & 0xFF;
                int minor = param & 0xFF;
                return new Version(major, minor);
            }
            return null;
        }
        catch (Exception ex)
        {
            RecordError(ErrorType.Communication, "GetCapabilityVersionAsync",
                $"获取能力{capabilityId}版本失败", ex.Message);
            return null;
        }
    }

    /// <summary>
    /// 发送控制命令并返回 ACK 中的 param 字段（用于需要获取返回值的命令）
    /// </summary>
    /// <param name="data">命令数据</param>
    /// <param name="expectedCmd">期望的 ACK 命令码</param>
    /// <returns>(是否成功, ACK param)</returns>
    private async Task<(bool Success, ushort Param)> SendControlCommandWithAckParamAsync(byte[] data, byte expectedCmd)
    {
        var tcs = new TaskCompletionSource<byte[]>(TaskCreationOptions.RunContinuationsAsynchronously);
        lock (_ackLock)
        {
            if (_ackWaiters.TryGetValue(expectedCmd, out var oldWaiter))
                oldWaiter.TrySetCanceled();
            _ackWaiters[expectedCmd] = tcs;
        }

        try
        {
            bool sent = await SendFeatureReportWithTimeoutAsync(REPORT_ID_CONTROL, data);
            if (!sent)
                return (false, 0);

            using var timeoutCts = new CancellationTokenSource(ACK_TIMEOUT_MS);
            var completedTask = await Task.WhenAny(tcs.Task, Task.Delay(ACK_TIMEOUT_MS, timeoutCts.Token));
            if (completedTask == tcs.Task)
            {
                byte[] ack = await tcs.Task;
                if (ack.Length >= 3 && ack[0] == REPORT_ID_ACK && ack[1] == expectedCmd)
                {
                    bool success = ack[2] == 0;
                    ushort param = ack.Length >= 6 ? (ushort)(ack[4] | (ack[5] << 8)) : (ushort)0;
                    return (success, param);
                }
                return (false, 0);
            }
            return (false, 0);
        }
        finally
        {
            lock (_ackLock)
            {
                _ackWaiters.Remove(expectedCmd);
            }
        }
    }

    /// <summary>
    /// 从设备获取所有能力的精确版本并更新能力列表
    /// 在连接成功后调用，逐个查询每个能力的版本号
    /// </summary>
    private async Task RefreshCapabilityVersionsAsync()
    {
        if (VersionInfo == null || VersionInfo.Capabilities.Count == 0)
            return;

        int updated = 0;
        foreach (var capId in VersionInfo.Capabilities.Keys.ToList())
        {
            var version = await GetCapabilityVersionAsync(capId);
            if (version != null && VersionInfo.Capabilities.TryGetValue(capId, out var cap))
            {
                cap.DeviceVersion = version;
                // 重新评估支持状态
                if (version < cap.HostMinVersion)
                    cap.Support = CapabilitySupport.DeviceTooOld;
                else if (version > cap.HostMaxVersion)
                    cap.Support = CapabilitySupport.HostTooOld;
                else
                    cap.Support = CapabilitySupport.Full;
                updated++;
            }
        }

        // 重新验证依赖关系
        VersionComparer.ValidateAllDependencies(VersionInfo.Capabilities);

        Log("INFO", $"能力版本刷新完成：查询{updated}个能力的精确版本");
    }

    #endregion

    /// <summary>
    /// 带超时的 Feature 报告发送
    /// </summary>
    private async Task<bool> SendFeatureReportWithTimeoutAsync(byte reportId, byte[] data)
    {
        using var cts = new CancellationTokenSource(HID_TIMEOUT_MS);
        try
        {
            return await _hidDriver.SendFeatureReportAsync(reportId, data, cts.Token);
        }
        catch (OperationCanceledException)
        {
            Log("ERROR", $"发送 Feature 报告超时: Report ID=0x{reportId:X2}");
            RecordError(ErrorType.Communication, "SendFeatureReportWithTimeoutAsync",
                $"发送超时 ReportID=0x{reportId:X2}", $"timeout={HID_TIMEOUT_MS}ms");
            return false;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"发送 Feature 报告异常: Report ID=0x{reportId:X2}, {ex.GetType().Name}: {ex.Message}");
            RecordError(ErrorType.Communication, "SendFeatureReportWithTimeoutAsync",
                $"发送异常 ReportID=0x{reportId:X2}", $"{ex.GetType().Name}: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 带超时的 Feature 报告读取
    /// </summary>
    private async Task<byte[]?> GetFeatureReportWithTimeoutAsync(byte reportId)
    {
        using var cts = new CancellationTokenSource(HID_TIMEOUT_MS);
        try
        {
            return await _hidDriver.GetFeatureReportAsync(reportId, cts.Token);
        }
        catch (OperationCanceledException)
        {
            Log("ERROR", $"读取 Feature 报告超时: Report ID=0x{reportId:X2}");
            RecordError(ErrorType.Communication, "GetFeatureReportWithTimeoutAsync",
                $"读取超时 ReportID=0x{reportId:X2}", $"timeout={HID_TIMEOUT_MS}ms");
            return null;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取 Feature 报告异常: Report ID=0x{reportId:X2}, {ex.GetType().Name}: {ex.Message}");
            RecordError(ErrorType.Communication, "GetFeatureReportWithTimeoutAsync",
                $"读取异常 ReportID=0x{reportId:X2}", $"{ex.GetType().Name}: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 获取按键统计数据
    /// </summary>
    /// <returns>64个按键的统计数据数组，失败返回null</returns>
    public Task<uint[]?> GetKeyStatsAsync() => _keyStatsService!.GetKeyStatsAsync();

    /// <summary>
    /// 清零按键统计
    /// </summary>
    /// <returns>是否成功</returns>
    public Task<bool> ResetKeyStatsAsync() => _keyStatsService!.ResetKeyStatsAsync();

    #region 宏配置

    /// <summary>
    /// 读取一个宏的配置数据
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <returns>宏的原始字节数据，失败返回null</returns>
    public Task<byte[]?> GetMacroAsync(byte macroId) => _macroService!.GetMacroAsync(macroId);

    /// <summary>
    /// 写入一个宏的配置数据
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <param name="data">宏的原始字节数据</param>
    /// <returns>是否成功</returns>
    public Task<bool> SetMacroAsync(byte macroId, byte[] data) => _macroService!.SetMacroAsync(macroId, data);

    /// <summary>
    /// 播放指定宏
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <returns>是否成功发送命令</returns>
    public Task<bool> PlayMacroAsync(byte macroId) => _macroService!.PlayMacroAsync(macroId);

    /// <summary>
    /// 停止指定宏
    /// </summary>
    /// <param name="macroId">宏ID (0-7)，0xFF 表示停止所有宏</param>
    /// <returns>是否成功发送命令</returns>
    public Task<bool> StopMacroAsync(byte macroId) => _macroService!.StopMacroAsync(macroId);

    #endregion

    #region 错误日志

    /// <summary>
    /// 获取错误日志信息
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>错误日志信息，失败返回null</returns>
    public Task<ErrorLogInfo?> GetErrorLogInfoAsync(CancellationToken cancellationToken = default) => _errorLogService!.GetErrorLogInfoAsync(cancellationToken);

    /// <summary>
    /// 读取指定索引的错误日志
    /// </summary>
    /// <param name="index">日志索引 (0-15)</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>错误日志条目，失败返回null</returns>
    public Task<ErrorLogEntry?> ReadErrorLogEntryAsync(byte index, CancellationToken cancellationToken = default) => _errorLogService!.ReadErrorLogEntryAsync(index, cancellationToken);

    /// <summary>
    /// 获取所有错误日志
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>所有错误日志列表，失败返回null</returns>
    public Task<IReadOnlyList<ErrorLogEntry>?> GetAllErrorLogsAsync(CancellationToken cancellationToken = default) => _errorLogService!.GetAllErrorLogsAsync(cancellationToken);

    /// <summary>
    /// 清除所有错误日志
    /// </summary>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>是否成功</returns>
    public Task<bool> ClearErrorLogsAsync(CancellationToken cancellationToken = default) => _errorLogService!.ClearErrorLogsAsync(cancellationToken);

    #endregion

    #region 性能监控

    /// <summary>
    /// 获取系统性能统计
    /// </summary>
    public async Task<PerfSystemStat?> GetPerfSystemStatAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;

        try
        {
            byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_PERF_SYSTEM);
            if (data == null || data.Length < 12)
            {
                Log("ERROR", "读取系统性能统计失败");
                return null;
            }

            var stat = new PerfSystemStat
            {
                CpuUsage = data[0],
                LoopFreqHz = BitConverter.ToUInt16(data, 1),
                UptimeSeconds = BitConverter.ToUInt32(data, 3),
                TaskCount = data[7],
                CpuUsageAvg10s = data[8],
                CpuUsageAvg30s = data[9],
                LoopFreqAvg10s = BitConverter.ToUInt16(data, 10)
            };

            Log("INFO", $"读取系统性能统计成功: CPU {stat.CpuUsage}% (10s avg {stat.CpuUsageAvg10s}%), 频率 {stat.LoopFreqHz}Hz, 运行 {stat.UptimeSeconds}s");
            return stat;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取系统性能统计异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 获取指定任务的性能统计
    /// </summary>
    public async Task<PerfTaskStat?> GetPerfTaskStatAsync(byte index, CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;

        try
        {
            // 先设置读取索引
            byte[] setBuffer = new byte[62];
            setBuffer[0] = index;
            await _hidDriver.SendFeatureReportAsync(REPORT_ID_PERF_TASK, setBuffer);

            // 再读取数据
            byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_PERF_TASK);
            if (data == null || data.Length < 31)
            {
                Log("ERROR", "读取任务性能统计失败");
                return null;
            }

            byte valid = data[1];
            if (valid == 0)
            {
                Log("WARNING", $"任务 {index} 无效");
                return null;
            }

            var stat = new PerfTaskStat
            {
                Index = data[0],
                ExecutionCount = BitConverter.ToUInt32(data, 2),
                MinTimeUs = BitConverter.ToUInt32(data, 6),
                MaxTimeUs = BitConverter.ToUInt32(data, 10),
                AvgTimeUs = BitConverter.ToUInt32(data, 14),
                LastTimeUs = BitConverter.ToUInt32(data, 18),
                CpuPercent = data[22],
                OverrunCount = BitConverter.ToUInt32(data, 23),
                ThresholdUs = BitConverter.ToUInt32(data, 27),
                Name = System.Text.Encoding.UTF8.GetString(data, 31, Math.Min(31, data.Length - 31)).TrimEnd('\0')
            };

            Log("INFO", $"读取任务 {index} 性能统计成功: {stat.Name}, 执行 {stat.ExecutionCount} 次, 平均 {stat.AvgTimeUs}us");
            return stat;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取任务性能统计异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 获取实时按键状态（64位bitmap，bit=1表示按下）
    /// </summary>
    public async Task<ulong?> GetKeyStateAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;

        try
        {
            byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_KEY_STATE);
            if (data == null || data.Length < 8)
            {
                return null;
            }

            // 小端序：data[0]是低字节，对应键0-7
            ulong keys = 0;
            for (int i = 0; i < 8; i++)
            {
                keys |= ((ulong)data[i]) << (i * 8);
            }

            return keys;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取按键状态异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 获取实时摇杆状态
    /// </summary>
    public async Task<(sbyte X, sbyte Y, bool Button)?> GetJoystickStateAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;

        try
        {
            byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_JOYSTICK_STATE);
            if (data == null || data.Length < 3)
            {
                return null;
            }

            sbyte x = (sbyte)data[0];
            sbyte y = (sbyte)data[1];
            bool btn = data[2] != 0;

            return (x, y, btn);
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取摇杆状态异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 获取摇杆校准状态（Report ID 23，10字节）
    /// 与固件 config_hid.c 中 REPORT_ID_JOYSTICK_CALIB 的 GET_REPORT 处理一致。
    /// 固件返回格式:
    ///   byte[0] = 校准状态（0=空闲,1=校准中,2=完成）
    ///   byte[1-2] = center_x（小端，ADC值，0-4095）
    ///   byte[3-4] = center_y（小端，ADC值，0-4095）
    ///   byte[5] = calibrated（0=未校准,1=已校准）
    ///   byte[6-7] = 死区值（小端，ADC原始值）
    ///   byte[8-9] = 灵敏度（小端，定点数1.0=1000）
    /// </summary>
    public async Task<(byte State, ushort CenterX, ushort CenterY, bool Calibrated, ushort Deadzone, ushort Sensitivity)?> GetJoystickCalibrationAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;

        try
        {
            byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_JOYSTICK_CALIB, cancellationToken);
            if (data == null || data.Length < 10)
            {
                Log("WARNING", $"读取摇杆校准状态失败或数据不足: length={data?.Length ?? -1}");
                return null;
            }

            var result = (
                State: data[0],
                CenterX: BitConverter.ToUInt16(data, 1),
                CenterY: BitConverter.ToUInt16(data, 3),
                Calibrated: data[5] != 0,
                Deadzone: BitConverter.ToUInt16(data, 6),
                Sensitivity: BitConverter.ToUInt16(data, 8)
            );

            Log("INFO", $"读取摇杆校准状态: state={result.State}, center=({result.CenterX},{result.CenterY}), calibrated={result.Calibrated}, deadzone={result.Deadzone}, sensitivity={result.Sensitivity}");
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取摇杆校准状态异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 读取 CONFIG_EXT2 子块数据（Report ID 25，子块索引机制）
    /// 由于 HID GET_REPORT 不能携带参数，读取流程为：
    ///   1. 先 SET_REPORT(25) 发送子块索引（1字节）
    ///   2. 再 GET_REPORT(25) 读取当前子块数据
    /// 子块定义:
    ///   0 = v6字段 (偏移1346, 14字节): dpi_slot[4], current_dpi_slot, lod_level, angle_snapping, dpi_switch_key, reserved4[2]
    ///   1 = layer2_keymap[0..31] (偏移1360, 32字节)
    ///   2 = layer2_keymap[32..63] (偏移1392, 32字节)
    ///   3 = layer3_keymap[0..31] (偏移1424, 32字节)
    ///   4 = layer3_keymap[32..63] (偏移1456, 32字节)
    /// </summary>
    /// <param name="subBlock">子块索引 (0-4)</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>子块数据，失败返回null</returns>
    public async Task<byte[]?> GetConfigExt2Async(byte subBlock, CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;
        if (subBlock > 4)
        {
            Log("ERROR", $"GetConfigExt2Async: 无效子块索引 {subBlock}");
            return null;
        }

        try
        {
            // 步骤1: 设置子块索引
            byte[] indexBuf = new byte[] { subBlock };
            bool setOk = await _hidDriver.SendFeatureReportAsync(REPORT_ID_CONFIG_EXT2, indexBuf, cancellationToken);
            if (!setOk)
            {
                Log("ERROR", $"GetConfigExt2Async: 设置子块索引 {subBlock} 失败");
                return null;
            }

            // 步骤2: 读取当前子块数据
            byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_CONFIG_EXT2, cancellationToken);
            if (data == null)
            {
                Log("ERROR", $"GetConfigExt2Async: 读取子块 {subBlock} 数据失败");
                return null;
            }

            Log("INFO", $"GetConfigExt2Async: 子块 {subBlock}, 读取 {data.Length} 字节");
            return data;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"GetConfigExt2Async 异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 写入 CONFIG_EXT2 子块数据（Report ID 25）
    /// 发送格式: byte[0]=子块索引, byte[1..]=数据
    /// </summary>
    /// <param name="subBlock">子块索引 (0-4)</param>
    /// <param name="data">要写入的数据</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>是否成功</returns>
    public async Task<bool> SetConfigExt2Async(byte subBlock, byte[] data, CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return false;
        if (subBlock > 4 || data == null)
            return false;

        try
        {
            // 构造发送缓冲区: byte[0]=子块索引, byte[1..]=数据
            byte[] sendBuf = new byte[data.Length + 1];
            sendBuf[0] = subBlock;
            Array.Copy(data, 0, sendBuf, 1, data.Length);

            bool result = await _hidDriver.SendFeatureReportAsync(REPORT_ID_CONFIG_EXT2, sendBuf, cancellationToken);
            Log("INFO", $"SetConfigExt2Async: 子块 {subBlock}, 写入 {data.Length} 字节, 结果={result}");
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"SetConfigExt2Async 异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 重置性能统计
    /// </summary>
    public async Task<bool> ResetPerfStatsAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return false;

        try
        {
            bool result = await SendControlCommandAsync(CMD_RESET_PERF);
            if (result)
            {
                Log("INFO", "重置性能统计成功");
            }
            else
            {
                Log("ERROR", "重置性能统计失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"重置性能统计异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 设置性能监控开关
    /// </summary>
    public async Task<bool> SetPerfMonitorEnabledAsync(bool enabled, CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return false;

        try
        {
            byte param = enabled ? (byte)1 : (byte)0;
            bool result = await SendControlCommandAsync(CMD_SET_PERF_ENABLE, param);
            if (result)
            {
                Log("INFO", $"性能监控{(enabled ? "开启" : "关闭")}成功");
            }
            else
            {
                Log("ERROR", $"性能监控{(enabled ? "开启" : "关闭")}失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"设置性能监控异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 设置性能监控任务超时阈值
    /// </summary>
    /// <param name="taskIndex">任务索引（0-15）</param>
    /// <param name="thresholdUs">超时阈值（微秒，0=禁用告警，最大25500us=25.5ms）</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>true=成功，false=失败</returns>
    public async Task<bool> SetPerfThresholdAsync(int taskIndex, int thresholdUs, CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return false;

        // 参数验证
        if (taskIndex < 0 || taskIndex > 15)
        {
            Log("ERROR", $"设置性能监控阈值失败: 无效任务索引 {taskIndex}（范围0-15）");
            return false;
        }
        if (thresholdUs < 0 || thresholdUs > 25500)
        {
            Log("ERROR", $"设置性能监控阈值失败: 无效阈值 {thresholdUs}us（范围0-25500us）");
            return false;
        }

        try
        {
            // 转换为固件端期望的格式：
            // 高字节（bit 8-15）= 任务索引（0-15）
            // 低字节（bit 0-7）= 阈值/100us（0-255）
            byte thresholdRaw = (byte)(thresholdUs / 100);
            ushort param = (ushort)((taskIndex << 8) | thresholdRaw);

            bool result = await SendControlCommandAsync(CMD_SET_PERF_THRESHOLD, param);
            if (result)
            {
                if (thresholdUs == 0)
                {
                    Log("INFO", $"任务{taskIndex} 性能监控超时告警已禁用");
                }
                else
                {
                    Log("INFO", $"任务{taskIndex} 性能监控超时阈值设置为 {thresholdUs}us");
                }
            }
            else
            {
                Log("ERROR", $"设置任务{taskIndex} 性能监控阈值失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"设置性能监控阈值异常: {ex.Message}");
            return false;
        }
    }

    #endregion

    /// <summary>
    /// 设置 DPI
    /// </summary>
    public async Task<bool> SetDpiAsync(int dpiIndex)
    {
        if (_currentConfig == null)
            return false;

        // 根据索引获取 DPI 值
        if (dpiIndex < 0 || dpiIndex >= _currentConfig.DpiLevels.Length)
            return false;

        ushort newDpi = (ushort)_currentConfig.DpiLevels[dpiIndex];
        _currentConfig.Dpi = newDpi;
        _currentConfig.DpiIndex = dpiIndex;

        return await WriteConfigToDeviceAsync(_currentConfig);
    }

    /// <summary>设置任意DPI值（100-6400，自动对齐到25的倍数）</summary>
    public async Task<bool> SetDpiValueAsync(ushort dpi, CancellationToken cancellationToken = default)
    {
        if (_currentConfig == null)
            return false;

        // 限制范围并对齐到25的倍数
        if (dpi < 100) dpi = 100;
        if (dpi > 6400) dpi = 6400;
        dpi = (ushort)((dpi / 25) * 25);

        _currentConfig.Dpi = dpi;
        // 更新索引（如果匹配预设档位）
        for (int i = 0; i < _currentConfig.DpiLevels.Length; i++)
        {
            if (_currentConfig.DpiLevels[i] == dpi)
            {
                _currentConfig.DpiIndex = i;
                break;
            }
        }

        return await WriteConfigToDeviceAsync(_currentConfig);
    }

    /// <summary>
    /// 循环切换 DPI 档位（发送 CMD_SWITCH_DPI 命令，立即生效并保存）
    /// </summary>
    /// <param name="slot">目标档位（0-3），或 0xFF 表示循环切换到下一档</param>
    public async Task<bool> SwitchDpiSlotAsync(byte slot = 0xFF)
    {
        if (_currentDevice == null)
            return false;

        bool result = await SendControlCommandAsync(CMD_SWITCH_DPI, slot);
        if (result)
        {
            // 刷新配置（从设备重新读取，await 不能在 lock 内，先读取再原子赋值）
            var newConfig = await ReadConfigFromDeviceAsync();
            lock (_stateLock)
            {
                _currentConfig = newConfig;
            }
        }
        return result;
    }

    /// <summary>
    /// 切换按键层（发送 CMD_SWITCH_LAYER 命令，立即生效，不写Flash）
    /// </summary>
    /// <param name="layer">目标层号（0-3）</param>
    public async Task<bool> SwitchLayerAsync(byte layer)
    {
        if (_currentDevice == null)
            return false;
        if (layer > 3)
        {
            Log("ERROR", $"SwitchLayerAsync: 无效层号 {layer}（允许0-3）");
            return false;
        }

        bool result = await SendControlCommandAsync(CMD_SWITCH_LAYER, layer);
        if (result)
        {
            Log("INFO", $"切换到按键层 {layer} 成功");
        }
        else
        {
            Log("ERROR", $"切换到按键层 {layer} 失败");
        }
        return result;
    }

    /// <summary>
    /// 触发摇杆中心点校准
    /// </summary>
    public async Task<bool> CalibrateJoystickAsync(ushort sampleCount = 50)
    {
        if (_currentDevice == null)
            return false;

        bool result = await SendControlCommandAsync(CMD_JOYSTICK_CALIBRATE, sampleCount);
        if (result)
        {
            // 校准需要时间，延迟后刷新配置（await 不能在 lock 内，先读取再原子赋值）
            await Task.Delay(500);
            var newConfig = await ReadConfigFromDeviceAsync();
            lock (_stateLock)
            {
                _currentConfig = newConfig;
            }
        }
        return result;
    }

    /// <summary>
    /// 设置指针加速
    /// 注意：当前固件版本不支持指针加速设置，此功能预留待固件实现。
    /// 调用将返回 false 并记录警告，不会修改设备配置。
    /// </summary>
    public async Task<bool> SetAccelerationAsync(bool enabled, double threshold, double ratio)
    {
        if (_currentConfig == null)
            return false;

        // 固件当前版本不支持加速设置，device_config_t 中无 acceleration 相关字段
        Log("WARNING", $"SetAccelerationAsync: 固件不支持指针加速设置（enabled={enabled}, threshold={threshold}, ratio={ratio}），操作已忽略");
        OnOperationStatusChanged("⚠️ 当前固件不支持指针加速设置");
        return false;
    }

    /// <summary>
    /// 设置摇杆死区
    /// </summary>
    public async Task<bool> SetJoystickDeadzoneAsync(ushort deadzone)
    {
        if (_currentConfig == null)
            return false;

        _currentConfig.JoystickDeadzone = deadzone;
        return await WriteConfigToDeviceAsync(_currentConfig);
    }

    /// <summary>
    /// 实时设置摇杆死区（不写Flash，立即生效）
    /// </summary>
    public async Task<bool> SetJoystickDeadzoneRealtimeAsync(ushort deadzone)
    {
        if (!IsConnected)
            return false;

        try
        {
            bool result = await SendControlCommandAsync(CMD_SET_JOYSTICK_DZ_RT, deadzone);
            if (result && _currentConfig != null)
            {
                _currentConfig.JoystickDeadzone = deadzone;
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"实时设置摇杆死区异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 设置编码器方向反转
    /// </summary>
    public async Task<bool> SetEncoderReverseAsync(bool reverse)
    {
        if (_currentConfig == null)
            return false;

        _currentConfig.EncoderReverse = reverse;
        return await WriteConfigToDeviceAsync(_currentConfig);
    }

    /// <summary>
    /// 保存配置到设备 Flash
    /// </summary>
    public async Task<bool> SaveConfigAsync(DeviceConfig config)
    {
        // WriteConfigToDeviceAsync 已经包含了写配置块 + 应用命令 + 保存到Flash + 重连
        return await WriteConfigToDeviceAsync(config);
    }

    /// <summary>
    /// 恢复默认配置
    /// </summary>
    public async Task<bool> ResetConfigAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return false;

        try
        {
            bool result = await SendControlCommandAsync(CMD_RESET_CONFIG);
            if (result)
            {
                Log("INFO", "恢复默认配置成功");
                // 重新加载配置（await 不能在 lock 内，先读取再原子赋值）
                var newConfig = await ReadConfigFromDeviceAsync();
                lock (_stateLock)
                {
                    _currentConfig = newConfig;
                }
            }
            else
            {
                Log("ERROR", "恢复默认配置失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"恢复默认配置异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 导出配置到文件
    /// </summary>
    public async Task<bool> ExportConfigAsync(string filePath)
    {
        if (_currentConfig == null)
            return false;

        try
        {
            byte[] data = _configSerializer.SerializeConfig(_currentConfig);
            await File.WriteAllBytesAsync(filePath, data);
            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 从文件导入配置
    /// </summary>
    public async Task<bool> ImportConfigAsync(string filePath)
    {
        try
        {
            // 验证文件存在
            if (!File.Exists(filePath))
            {
                Log("ERROR", $"导入配置失败：文件不存在: {filePath}");
                return false;
            }

            // 验证扩展名
            string ext = Path.GetExtension(filePath).ToLowerInvariant();
            if (ext != ".bin" && ext != ".json" && ext != ".hidcfg")
            {
                Log("ERROR", $"导入配置失败：不支持的文件扩展名: {ext}");
                return false;
            }

            // 验证文件大小合理性（最大 1MB）
            var fileInfo = new FileInfo(filePath);
            if (fileInfo.Length > 1024 * 1024)
            {
                Log("ERROR", $"导入配置失败：文件过大: {fileInfo.Length} 字节");
                return false;
            }

            byte[] data = await File.ReadAllBytesAsync(filePath);
            if (data.Length < CONFIG_SIZE)
            {
                Log("ERROR", $"导入配置失败：文件大小不足: {data.Length} < {CONFIG_SIZE}");
                return false;
            }

            var config = _configSerializer.ParseConfig(data);
            if (config == null)
            {
                Log("ERROR", "导入配置失败：解析配置数据失败");
                return false;
            }
            lock (_stateLock)
            {
                _currentConfig = config;
            }

            // 写入到设备
            return await WriteConfigToDeviceAsync(config);
        }
        catch (Exception ex)
        {
            Log("ERROR", $"导入配置异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 释放资源
    /// </summary>
    #region 自动连接与心跳检测

    /// <summary>
    /// 启动自动连接和心跳检测
    /// </summary>
    public void StartAutoConnect()
    {
        lock (_autoConnectLock)
        {
            if (_heartbeatCts != null)
                return; // 已经启动了

            _heartbeatCts = new CancellationTokenSource();
            _ = HeartbeatLoopAsync(_heartbeatCts.Token);
            Log("INFO", "自动连接与心跳检测已启动");
        }
    }

    /// <summary>
    /// 停止自动连接和心跳检测
    /// </summary>
    public void StopAutoConnect()
    {
        lock (_autoConnectLock)
        {
            if (_heartbeatCts != null)
            {
                try
                {
                    _heartbeatCts.Cancel();
                }
                catch { }

                _heartbeatCts.Dispose();
                _heartbeatCts = null;
                Log("INFO", "自动连接与心跳检测已停止");
            }
        }
    }

    /// <summary>
    /// 通知设备已插入（由 UI 层监听到 WM_DEVICECHANGE 后调用）
    /// 会延迟 500ms 再尝试连接，等待 Windows 驱动初始化完成
    /// </summary>
    public void NotifyDevicePluggedIn()
    {
        _ = Task.Run(async () =>
        {
            try
            {
                // 延迟等待驱动初始化
                await Task.Delay(PlugInDelayMs);

                // 如果已经连接了，就不用再连了
                if (IsConnected)
                    return;

                await TryAutoConnectAsync();
            }
            catch (Exception ex)
            {
                Log("ERROR", $"设备插入后自动连接异常: {ex.Message}");
            }
        });
    }

    /// <summary>
    /// 心跳检测循环
    /// </summary>
    private async Task HeartbeatLoopAsync(CancellationToken token)
    {
        Log("INFO", "心跳检测循环已启动");

        // 启动时先尝试连接一次
        if (!IsConnected)
        {
            await TryAutoConnectAsync();
        }

        while (!token.IsCancellationRequested)
        {
            try
            {
                await Task.Delay(HeartbeatIntervalMs, token);

                if (IsConnected)
                {
                    // 已连接：检测设备是否还活着
                    bool alive = await TestDeviceAliveAsync();
                    if (!alive)
                    {
                        Log("WARNING", "心跳检测失败，设备可能已断开，尝试重连...");
                        OnOperationStatusChanged("设备连接已断开，正在重连...");

                        // 先断开
                        Disconnect();

                        // 尝试重连
                        await TryAutoConnectAsync();
                    }
                }
                else
                {
                    // 未连接：尝试连接
                    await TryAutoConnectAsync();
                }
            }
            catch (OperationCanceledException)
            {
                // 正常取消，忽略
                break;
            }
            catch (Exception ex)
            {
                Log("ERROR", $"心跳检测异常: {ex.Message}");
            }
        }

        Log("INFO", "心跳检测循环已停止");
    }

    /// <summary>
    /// 检测设备是否还活着（发送一个简单的读取命令）
    /// </summary>
    private async Task<bool> TestDeviceAliveAsync()
    {
        try
        {
            // Bootloader 模式下不支持设备信息报告，通过 USB 连接状态判断设备是否存活
            // 如果设备断开，Windows 会移除设备，后续 HID 操作会失败
            if (CurrentDeviceMode == DeviceMode.Bootloader)
            {
                return _hidDriver.IsConnected;
            }

            // 应用程序模式：读取设备信息报告，能读成功就说明设备还活着
            byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_DEVICE_INFO);
            return data != null && data.Length >= 1;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 尝试自动连接设备
    /// </summary>
    private async Task<bool> TryAutoConnectAsync()
    {
        // 用互斥锁防止并发重连
        if (!await _connectLock.WaitAsync(0))
            return false; // 正在连接中，直接返回

        try
        {
            if (IsConnected)
                return true; // 已经连接了

            // 重试机制：设备插入后驱动可能需要时间初始化，最多重试 5 次
            const int maxRetries = 5;
            for (int attempt = 0; attempt < maxRetries; attempt++)
            {
                if (IsConnected)
                    return true;

                // 搜索设备（同时扫描应用程序和 Bootloader 设备）
                var appDevices = await _hidDriver.FindDevicesAsync(DeviceVendorId, DeviceProductId);
                var bootloaderDevices = await _hidDriver.FindDevicesAsync((ushort)DfuUpdater.BOOTLOADER_VID, (ushort)DfuUpdater.BOOTLOADER_PID);
                var devices = appDevices.Concat(bootloaderDevices).ToList();

                // 调试日志：输出找到的设备信息
                if (devices.Count > 0)
                {
                    Log("INFO", $"找到 {devices.Count} 个设备（应用+Bootloader）:");
                    foreach (var d in devices)
                    {
                        Log("INFO", $"  - VID=0x{d.VendorId:X4}, PID=0x{d.ProductId:X4}, UsagePage=0x{d.UsagePage:X4}, Name={d.ProductName}");
                    }
                }

                // 过滤：优先选择 Vendor 配置接口（UsagePage == 0xFF00），如果没有则取第一个
                var targetDevice = devices.FirstOrDefault(d => d.UsagePage == 0xFF00);
                if (targetDevice == null && devices.Count > 0)
                {
                    Log("WARNING", "未找到 UsagePage=0xFF00 的配置接口，使用第一个设备");
                    targetDevice = devices[0];
                }

                if (targetDevice != null)
                {
                    // 连接设备
                    bool result = await ConnectAsync(targetDevice);
                    if (result)
                        return true;
                }

                // 没找到设备或连接失败，等待后重试（最后一次不等待）
                if (attempt < maxRetries - 1)
                {
                    Log("INFO", $"自动连接第 {attempt + 1}/{maxRetries} 次未找到设备，500ms 后重试");
                    await Task.Delay(500);
                }
            }

            Log("WARNING", $"自动连接失败：重试 {maxRetries} 次后仍未找到设备");
            return false;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"自动连接异常: {ex.Message}");
            return false;
        }
        finally
        {
            _connectLock.Release();
        }
    }

    #endregion

    #region 设备控制

    /// <summary>
    /// 重启设备
    /// </summary>
    public async Task<bool> RebootAsync()
    {
        if (!IsConnected)
            return false;

        try
        {
            Log("INFO", "正在重启设备...");
            // 固件要求连续发送3次（5秒内）才执行，防止恶意DoS
            bool result = false;
            for (int i = 0; i < 3; i++)
            {
                result = await SendControlCommandAsync(CMD_REBOOT);
                if (!result) break;
                if (i < 2) await Task.Delay(100);
            }

            if (result)
            {
                Log("INFO", "重启命令已发送（3次确认）");
                // 设备会断开连接，自动连接机制会处理重连
            }
            else
            {
                Log("ERROR", "重启命令发送失败");
            }

            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"重启异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 获取 Bootloader 信息（版本、应用区大小等）
    /// 仅在设备处于 Bootloader 模式时有效
    /// </summary>
    public async Task<DfuStatus?> GetBootloaderInfoAsync()
    {
        if (!IsConnected || CurrentDeviceMode != DeviceMode.Bootloader)
        {
            Log("WARNING", "GetBootloaderInfoAsync: 设备未连接或非 Bootloader 模式");
            return null;
        }

        try
        {
            var dfu = new DfuUpdater(_hidDriver, msg => Log("DEBUG", msg));
            var status = await dfu.GetStatusAsync();
            if (status != null)
            {
                Log("INFO", $"Bootloader v{status.VersionMajor}.{status.VersionMinor}, 应用区: 0x{status.AppStart:X8}, 最大: {status.AppMaxSize} 字节");
            }
            return status;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"获取 Bootloader 信息失败: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 从 Bootloader 模式启动应用程序
    /// 仅在设备处于 Bootloader 模式时有效
    /// </summary>
    public async Task<bool> BootToApplicationAsync()
    {
        if (!IsConnected || CurrentDeviceMode != DeviceMode.Bootloader)
        {
            Log("WARNING", "BootToApplicationAsync: 设备未连接或非 Bootloader 模式");
            return false;
        }

        try
        {
            Log("INFO", "从 Bootloader 启动应用程序...");
            var dfu = new DfuUpdater(_hidDriver, msg => Log("DEBUG", msg));
            bool result = await dfu.BootToApplicationAsync();
            if (result)
            {
                Log("INFO", "启动应用命令已发送，设备将重启进入应用程序");
                // 设备会重启，断开连接
                Disconnect();
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"启动应用程序失败: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 验证固件文件是否适合当前设备
    /// 检查 UF2 起始地址、文件大小、格式等
    /// </summary>
    public async Task<(bool Valid, string Message)> ValidateFirmwareFileAsync(string firmwarePath)
    {
        if (!File.Exists(firmwarePath))
        {
            return (false, "固件文件不存在");
        }

        try
        {
            string ext = Path.GetExtension(firmwarePath).ToLowerInvariant();

            if (ext == ".uf2")
            {
                // 解析 UF2 文件，检查起始地址
                using var fs = File.OpenRead(firmwarePath);
                byte[] block = new byte[512];

                uint? baseAddr = null;
                uint maxAddr = 0;
                uint totalPayload = 0;

                while (fs.Read(block, 0, 512) == 512)
                {
                    uint magic0 = BitConverter.ToUInt32(block, 0);
                    if (magic0 != 0x0A324655) continue;

                    uint targetAddr = BitConverter.ToUInt32(block, 12);
                    uint payloadSize = BitConverter.ToUInt32(block, 16);

                    if (baseAddr == null || targetAddr < baseAddr)
                        baseAddr = targetAddr;
                    if (targetAddr + payloadSize > maxAddr)
                        maxAddr = targetAddr + payloadSize;
                    totalPayload += payloadSize;
                }

                if (baseAddr == null)
                {
                    return (false, "无效的 UF2 文件（无有效数据块）");
                }

                // 检查起始地址
                if (baseAddr.Value == 0x10000000)
                {
                    return (false, "此 UF2 是 Bootloader 固件（起始地址 0x10000000），不能通过 DFU 升级应用程序。请使用 picotool 或 BOOTSEL 模式烧录 Bootloader。");
                }
                if (baseAddr.Value != 0x10009000)
                {
                    return (false, $"UF2 起始地址 0x{baseAddr.Value:X8} 不符合预期（应用程序应为 0x10009000）");
                }

                // 检查大小
                uint firmwareSize = maxAddr - baseAddr.Value;
                if (firmwareSize > 1948 * 1024)
                {
                    return (false, $"固件大小 {firmwareSize} 字节超过应用区最大 1948KB");
                }

                return (true, $"UF2 固件有效: 起始地址 0x{baseAddr.Value:X8}, 大小 {firmwareSize} 字节 ({firmwareSize / 1024}KB)");
            }
            else if (ext == ".bin")
            {
                FileInfo fi = new FileInfo(firmwarePath);
                if (fi.Length > 1948 * 1024)
                {
                    return (false, $"BIN 文件大小 {fi.Length} 字节超过应用区最大 1948KB");
                }
                if (fi.Length == 0)
                {
                    return (false, "BIN 文件为空");
                }
                return (true, $"BIN 固件有效: 大小 {fi.Length} 字节 ({fi.Length / 1024}KB)");
            }
            else
            {
                return (false, $"不支持的文件格式: {ext}（仅支持 .uf2 和 .bin）");
            }
        }
        catch (Exception ex)
        {
            return (false, $"验证固件文件失败: {ex.Message}");
        }
    }

    /// <summary>
    /// 进入 BOOTSEL 烧录模式
    /// 注意：第三次 CMD_ENTER_DFU 命令发送后设备会立即重启，
    /// HID 写入可能因设备断开而失败，这是正常现象，不影响设备进入 DFU 模式。
    /// </summary>
    public async Task<bool> EnterBootselAsync()
    {
        if (!IsConnected)
            return false;

        try
        {
            Log("INFO", "正在进入 BOOTSEL 模式...");
            // 固件要求连续发送3次（5秒内）才执行，防止恶意DoS
            // 前两次命令检查返回结果
            for (int i = 0; i < 2; i++)
            {
                bool result = await SendControlCommandAsync(CMD_ENTER_DFU);
                if (!result)
                {
                    Log("ERROR", $"BOOTSEL 命令第 {i + 1}/3 次发送失败");
                    return false;
                }
                await Task.Delay(100);
            }

            // 第三次命令：设备收到后会立即重启，HID 写入可能失败，这是正常的
            // 忽略异常，直接认为命令已发送
            try
            {
                await SendControlCommandAsync(CMD_ENTER_DFU);
            }
            catch
            {
                // 设备重启导致 HID 写入异常，忽略
            }

            Log("INFO", "BOOTSEL 命令已发送（3次确认），设备将进入烧录模式");
            // 设备会进入 BOOTSEL 模式，断开连接
            Disconnect();
            return true;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"进入 BOOTSEL 异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 软件触发进入官方 BOOTSEL UF2 模式（CMD_ENTER_BOOTSEL = 0x14）
    /// 相当于按住 BOOTSEL 键上电，设备会枚举为 RPI-RP2 U 盘
    /// 注意：固件要求连续发送3次（5秒内）才执行，防止恶意DoS和误触
    /// 第三次命令发送后设备会立即重启，HID 写入可能因设备断开而失败，这是正常现象。
    /// 与 EnterBootselAsync 的区别：EnterBootselAsync 进入自定义 Bootloader DFU 模式（用于一键升级），
    /// 本方法进入官方 BOOTSEL UF2 模式（用于手动拖拽 .uf2 文件）。
    /// </summary>
    public async Task<bool> EnterBootselModeAsync()
    {
        if (!IsConnected)
            return false;

        try
        {
            Log("INFO", "正在发送进入官方 BOOTSEL UF2 模式命令（需3次确认）...");

            // 固件要求连续发送3次（5秒内）才执行，防止恶意DoS
            // 前两次命令检查返回结果
            for (int i = 0; i < 2; i++)
            {
                bool result = await SendControlCommandAsync(CMD_ENTER_BOOTSEL);
                if (!result)
                {
                    Log("ERROR", $"BOOTSEL 命令第 {i + 1}/3 次发送失败");
                    return false;
                }
                await Task.Delay(100);
            }

            // 第三次命令：设备收到后会立即重启进入 BOOTSEL 模式，HID 写入可能失败，这是正常的
            // 忽略异常，直接认为命令已发送
            try
            {
                await SendControlCommandAsync(CMD_ENTER_BOOTSEL);
            }
            catch
            {
                // 设备重启导致 HID 写入异常，忽略
            }

            Log("INFO", "官方 BOOTSEL UF2 模式命令已发送（3次确认），设备将重启进入 BOOTSEL 模式");
            // 设备会进入 BOOTSEL 模式，断开连接
            Disconnect();
            return true;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"进入官方 BOOTSEL UF2 模式异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// DFU 固件升级事件
    /// </summary>
    public event EventHandler<string>? DfuStatusChanged;

    /// <summary>
    /// DFU 固件升级（完整流程）
    /// </summary>
    public async Task<bool> DfuUpgradeAsync(string firmwarePath, IProgress<int>? progress = null, CancellationToken cancellationToken = default)
    {
        if (!File.Exists(firmwarePath))
        {
            RecordError(ErrorType.Dfu, "DfuUpgradeAsync", "固件文件不存在", $"路径: {firmwarePath}");
            DfuStatusChanged?.Invoke(this, "错误：固件文件不存在");
            return false;
        }

        try
        {
            // 1. 解析固件文件
            DfuStatusChanged?.Invoke(this, "正在解析固件文件...");
            byte[] firmwareData;
            // TODO: 未来应从固件文件元数据中解析版本号（如 UF2 的自定义块或固件头部固定偏移）
            // 当前使用与固件端一致的版本号 v4.0.0 (0x0400)
            uint version = 0x0400;

            string ext = Path.GetExtension(firmwarePath).ToLowerInvariant();
            if (ext == ".uf2")
            {
                firmwareData = DfuUpdater.ExtractFirmwareFromUf2(firmwarePath);
            }
            else if (ext == ".bin")
            {
                firmwareData = await File.ReadAllBytesAsync(firmwarePath, cancellationToken);
            }
            else
            {
                RecordError(ErrorType.Dfu, "DfuUpgradeAsync", "不支持的文件格式", $"扩展名: {ext}");
                DfuStatusChanged?.Invoke(this, "错误：不支持的文件格式（仅支持 .uf2 和 .bin）");
                return false;
            }

            DfuStatusChanged?.Invoke(this, $"固件大小: {firmwareData.Length} 字节");
            progress?.Report(5);

            // 2. 判断当前设备模式，进入 DFU 模式
            //    - 如果已连接应用程序(PID=0x4004)，发送 CMD_ENTER_DFU 进入 Bootloader
            //    - 如果已连接 Bootloader(PID=0x4005)，直接开始升级
            bool alreadyInBootloader = _currentDevice != null && _currentDevice.ProductId == DfuUpdater.BOOTLOADER_PID;

            if (!alreadyInBootloader && IsConnected)
            {
                DfuStatusChanged?.Invoke(this, "正在进入 DFU 模式...");
                bool entered = await EnterBootselAsync();
                if (!entered)
                {
                    RecordError(ErrorType.Dfu, "DfuUpgradeAsync", "进入 DFU 模式失败", "CMD_ENTER_DFU 命令执行失败或设备无响应");
                    DfuStatusChanged?.Invoke(this, "错误：进入 DFU 模式失败");
                    return false;
                }
            }
            else if (alreadyInBootloader)
            {
                DfuStatusChanged?.Invoke(this, "设备已在 Bootloader 模式，直接开始升级");
                // 断开当前连接，稍后重新连接 Bootloader
                Disconnect();
            }
            else
            {
                DfuStatusChanged?.Invoke(this, "未连接设备，等待 Bootloader 设备...");
            }

            // 3. 等待 Bootloader 设备出现
            DfuStatusChanged?.Invoke(this, "等待 Bootloader 设备...");
            progress?.Report(10);

            string? bootloaderPath = null;
            for (int attempt = 0; attempt < 30; attempt++)
            {
                cancellationToken.ThrowIfCancellationRequested();
                var devices = await _hidDriver.FindDevicesAsync(DfuUpdater.BOOTLOADER_VID, DfuUpdater.BOOTLOADER_PID);
                if (devices.Count > 0)
                {
                    bootloaderPath = devices[0].DevicePath;
                    break;
                }
                await Task.Delay(500, cancellationToken);
            }

            if (bootloaderPath == null)
            {
                RecordError(ErrorType.Dfu, "DfuUpgradeAsync", "未找到 Bootloader 设备",
                    $"VID=0x{DfuUpdater.BOOTLOADER_VID:X4}, PID=0x{DfuUpdater.BOOTLOADER_PID:X4}, 等待15秒超时");
                DfuStatusChanged?.Invoke(this, "错误：未找到 Bootloader 设备");
                return false;
            }

            // 4. 连接到 Bootloader
            DfuStatusChanged?.Invoke(this, "正在连接 Bootloader...");
            bool connected = await _hidDriver.OpenAsync(bootloaderPath);
            if (!connected)
            {
                RecordError(ErrorType.Dfu, "DfuUpgradeAsync", "连接 Bootloader 失败", $"路径: {bootloaderPath}");
                DfuStatusChanged?.Invoke(this, "错误：连接 Bootloader 失败");
                return false;
            }

            // 5. 执行升级
            var dfuProgress = new Progress<int>(p =>
            {
                progress?.Report(10 + (int)(p * 0.85)); // 升级过程占 85%
            });

            var dfuUpdater = new DfuUpdater(_hidDriver, msg => DfuStatusChanged?.Invoke(this, msg));
            bool success = await dfuUpdater.UpgradeAsync(firmwareData, version, dfuProgress, cancellationToken);

            // 6. 断开 Bootloader 连接
            _hidDriver.Close();

            if (!success)
            {
                RecordError(ErrorType.Dfu, "DfuUpgradeAsync", "固件升级失败",
                    "DfuUpdater.UpgradeAsync 返回 false，可能是擦除/写入/校验失败");
                DfuStatusChanged?.Invoke(this, "错误：固件升级失败");
                return false;
            }

            // 7. 等待设备重启为应用模式
            DfuStatusChanged?.Invoke(this, "等待设备重启...");
            progress?.Report(98);

            for (int attempt = 0; attempt < 20; attempt++)
            {
                cancellationToken.ThrowIfCancellationRequested();
                var devices = await _hidDriver.FindDevicesAsync(DeviceVendorId, DeviceProductId);
                if (devices.Count > 0)
                {
                    // 重新连接
                    await ConnectAsync(devices[0]);
                    break;
                }
                await Task.Delay(500, cancellationToken);
            }

            progress?.Report(100);
            DfuStatusChanged?.Invoke(this, "固件升级完成！");
            return true;
        }
        catch (OperationCanceledException)
        {
            DfuStatusChanged?.Invoke(this, "升级已取消");
            return false;
        }
        catch (Exception ex)
        {
            DfuStatusChanged?.Invoke(this, $"升级异常: {ex.Message}");
            Log("ERROR", $"DFU 升级异常: {ex}");
            return false;
        }
    }

    #endregion

    public void Dispose()
    {
        if (!_disposed)
        {
            StopAutoConnect();
            Disconnect();
            _writeLock.Dispose();
            _connectLock.Dispose();
            _hidDriver.Dispose();
            _disposed = true;
        }
    }
}
