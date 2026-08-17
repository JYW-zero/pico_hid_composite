using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;
using System.IO;

namespace HidConfigTool.Core.Services;

/// <summary>
/// HID 设备服务实现
/// 负责与设备通信，读写配置
/// 协议：基于 Feature 报告
///   Report ID 5 = 配置数据（146 字节）
///   Report ID 6 = 设备信息（32 字节）
///   Report ID 7 = 控制命令（1 字节）
/// </summary>
public class DeviceService : IDeviceService
{
    #region 协议常量

    // Report ID（分块协议）
    private const byte REPORT_ID_CONFIG_BLOCK0 = 0x05;  // 配置块 0（偏移 0-61）
    private const byte REPORT_ID_DEVICE_INFO = 0x06;    // 设备信息
    private const byte REPORT_ID_CONTROL = 0x07;        // 控制命令
    private const byte REPORT_ID_CONFIG_BLOCK1 = 0x08;  // 配置块 1（偏移 62-123）
    private const byte REPORT_ID_CONFIG_BLOCK2 = 0x09;  // 配置块 2（偏移 124-185）
    private const byte REPORT_ID_KEY_STATS0 = 0x0A;     // 按键统计块 0（键 0~15）
    private const byte REPORT_ID_KEY_STATS1 = 0x0B;     // 按键统计块 1（键 16~31）
    private const byte REPORT_ID_KEY_STATS2 = 0x0C;     // 按键统计块 2（键 32~47）
    private const byte REPORT_ID_KEY_STATS3 = 0x0D;     // 按键统计块 3（键 48~63）
    private const byte REPORT_ID_MACRO_CONFIG = 0x0E;   // 宏配置读写
    private const int MACRO_REPORT_SIZE = 62;           // 宏配置报告大小（不含Report ID）
    private const int MACRO_BLOCK_DATA_SIZE = 60;       // 每块数据大小（去掉2字节头）
    private const byte REPORT_ID_PERF_SYSTEM = 0x0F;    // 性能监控 - 系统状态
    private const byte REPORT_ID_PERF_TASK = 0x10;      // 性能监控 - 任务统计
    private const byte REPORT_ID_FAULT_INFO = 0x11;     // 错误日志 - 信息
    private const byte REPORT_ID_FAULT_LOG = 0x12;      // 错误日志 - 读取日志
    private const byte REPORT_ID_KEY_STATE = 0x13;      // 实时按键状态 (64位bitmap)
    private const byte REPORT_ID_JOYSTICK_STATE = 0x14; // 实时摇杆状态

    // 控制命令码
    private const byte CMD_SAVE_CONFIG = 0x01;
    private const byte CMD_RESET_CONFIG = 0x02;
    private const byte CMD_REBOOT = 0x03;
    private const byte CMD_ENTER_DFU = 0x04;
    private const byte CMD_APPLY_CONFIG = 0x05;         // 应用临时配置
    private const byte CMD_RESET_STATS = 0x06;          // 清零按键统计
    private const byte CMD_CLEAR_FAULT = 0x07;          // 清除错误日志
    private const byte CMD_RESET_PERF = 0x08;           // 重置性能统计
    private const byte CMD_MACRO_PLAY = 0x09;           // 播放宏（参数：宏ID）
    private const byte CMD_MACRO_STOP = 0x0A;           // 停止宏（参数：宏ID，0xFF=停止所有）
    private const byte CMD_SET_PERF_ENABLE = 0x0B;      // 设置性能监控开关（参数：1=开启，0=关闭）
    private const byte CMD_SET_JOYSTICK_DZ_RT = 0x0C;   // 实时设置摇杆死区（参数：2字节小端，不写Flash）
    private const byte CMD_UNLOCK_CONFIG = 0x0D;        // 解锁配置写入（需连续3次，5秒内）

    // 配置魔数
    private const uint CONFIG_MAGIC = 0x5A5A5A5A;

    // HID 操作超时（毫秒）
    private const int HID_TIMEOUT_MS = 5000;

    // 配置结构体大小
    private const int CONFIG_SIZE = 1338;  // v3: 与固件 device_config_t 一致（新增摇杆/编码器扩展字段）

    // 每个块的大小
    private const int BLOCK_SIZE = 62;

    // 配置字段偏移量
    private const int OFFSET_MAGIC = 0;
    private const int OFFSET_VERSION = 4;
    private const int OFFSET_DPI = 6;
    private const int OFFSET_JOYSTICK_DEADZONE = 8;
    private const int OFFSET_ENCODER_REVERSE = 10;
    private const int OFFSET_SEQ = 11;
    private const int OFFSET_KEYMAP = 14;
    private const int OFFSET_FN_KEYMAP = 78;
    // v3 新增字段（macro_data之后，crc32之前）
    private const int OFFSET_JOY_INV_X = 1326;
    private const int OFFSET_JOY_INV_Y = 1327;
    private const int OFFSET_ENC_STEPS = 1328;
    private const int OFFSET_ENC_SCROLL = 1329;
    private const int OFFSET_JOY_SENS = 1330;
    private const int OFFSET_CRC32 = 1334;  // v3: crc32 在扩展字段之后

    #endregion

    private readonly IHidDriver _hidDriver;
    private HidDeviceInfo? _currentDevice;
    private DeviceConfig? _currentConfig;
    private bool _disposed;

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
    private const int MinWriteIntervalMs = 200;

    /// <summary>
    /// 连接操作锁，防止并发重连
    /// </summary>
    private readonly SemaphoreSlim _connectLock = new(1, 1);

    /// <summary>
    /// 状态锁，保护 _currentDevice / _currentConfig / _lastWriteTime 等共享状态的多线程访问
    /// </summary>
    private readonly object _stateLock = new();

    /// <summary>
    /// 心跳检测取消令牌
    /// </summary>
    private CancellationTokenSource? _heartbeatCts;

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
    public DeviceConfig? CurrentConfig => _currentConfig;

    /// <summary>
    /// 设备 VID
    /// </summary>
    public const ushort DeviceVendorId = 0xCAFE;

    /// <summary>
    /// 设备 PID
    /// </summary>
    public const ushort DeviceProductId = 0x4004;

    public DeviceService(IHidDriver hidDriver)
    {
        _hidDriver = hidDriver;

        // 初始化日志文件路径
        string logDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "HIDConfigTool", "Logs");
        Directory.CreateDirectory(logDir);
        _logFilePath = Path.Combine(logDir, $"device_service_{DateTime.Now:yyyyMMdd}.log");

        // 清理旧日志
        CleanOldLogs(logDir);

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
            File.AppendAllText(_logFilePath, logLine + Environment.NewLine);
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

        var allDevices = await _hidDriver.FindDevicesAsync(DeviceVendorId, DeviceProductId);

        // 只返回配置接口的设备（UsagePage = 0xFF00 Vendor Defined）
        // 过滤掉键盘、鼠标、消费者控制、游戏手柄等其他集合
        var configDevices = allDevices.Where(d => d.UsagePage == 0xFF00).ToList();

        return configDevices;
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
            bool opened = await _hidDriver.OpenAsync(deviceInfo.DevicePath);
            if (!opened)
                return false;

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

            // 读取设备信息
            try
            {
                byte[]? deviceInfoData = await _hidDriver.GetFeatureReportAsync(REPORT_ID_DEVICE_INFO);
                if (deviceInfoData != null && deviceInfoData.Length >= 3)
                {
                    int major = deviceInfoData[0];
                    int minor = deviceInfoData[1];
                    int patch = deviceInfoData[2];
                    FirmwareVersion = $"v{major}.{minor}.{patch}";
                }
                else
                {
                    FirmwareVersion = "v1.0.0";
                }
            }
            catch
            {
                FirmwareVersion = "v1.0.0";
            }

            // 触发连接状态变化事件
            DeviceConnectionChanged?.Invoke(this, true);

            return true;
        }
        catch
        {
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
            }
        }

        if (hadDevice)
        {
            try
            {
                _hidDriver.Close();
            }
            catch { }
            FirmwareVersion = null;

            // 触发连接状态变化事件
            DeviceConnectionChanged?.Invoke(this, false);
        }
    }

    /// <summary>
    /// 从设备读取配置（分块读取）
    /// </summary>
    private async Task<DeviceConfig?> ReadConfigFromDeviceAsync()
    {
        try
        {
            // 分 3 块读取
            byte[]? block0 = await _hidDriver.GetFeatureReportAsync(REPORT_ID_CONFIG_BLOCK0);
            byte[]? block1 = await _hidDriver.GetFeatureReportAsync(REPORT_ID_CONFIG_BLOCK1);
            byte[]? block2 = await _hidDriver.GetFeatureReportAsync(REPORT_ID_CONFIG_BLOCK2);

            if (block0 == null || block1 == null || block2 == null)
                return null;

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

            return ParseConfig(fullData);
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// 解析配置数据
    /// </summary>
    private DeviceConfig ParseConfig(byte[] data)
    {
        var config = new DeviceConfig();

        // 验证魔数
        if (data.Length < OFFSET_MAGIC + 4)
            return config;

        uint magic = BitConverter.ToUInt32(data, OFFSET_MAGIC);
        if (magic != CONFIG_MAGIC)
        {
            return config; // 魔数不对，返回默认配置
        }

        // 版本
        if (data.Length >= OFFSET_VERSION + 2)
            config.Version = BitConverter.ToUInt16(data, OFFSET_VERSION);

        // DPI
        if (data.Length >= OFFSET_DPI + 2)
        {
            config.Dpi = BitConverter.ToUInt16(data, OFFSET_DPI);
            // 根据 DPI 值计算索引
            for (int i = 0; i < config.DpiLevels.Length; i++)
            {
                if (config.DpiLevels[i] == config.Dpi)
                {
                    config.DpiIndex = i;
                    break;
                }
            }
        }

        // 摇杆死区
        if (data.Length >= OFFSET_JOYSTICK_DEADZONE + 2)
            config.JoystickDeadzone = BitConverter.ToUInt16(data, OFFSET_JOYSTICK_DEADZONE);

        // 编码器方向
        if (data.Length >= OFFSET_ENCODER_REVERSE + 1)
            config.EncoderReverse = data[OFFSET_ENCODER_REVERSE] != 0;

        // 按键映射
        config.Keymap = new byte[64];
        if (data.Length >= OFFSET_KEYMAP + 64)
        {
            Array.Copy(data, OFFSET_KEYMAP, config.Keymap, 0, 64);
        }

        // Fn 层按键映射
        config.FnKeymap = new byte[64];
        if (data.Length >= OFFSET_FN_KEYMAP + 64)
        {
            Array.Copy(data, OFFSET_FN_KEYMAP, config.FnKeymap, 0, 64);
        }

        // v3 新增字段（版本>=3时有效）
        if (config.Version >= 3 && data.Length >= OFFSET_CRC32)
        {
            config.JoystickInvertX = data[OFFSET_JOY_INV_X] != 0;
            config.JoystickInvertY = data[OFFSET_JOY_INV_Y] != 0;
            config.EncoderStepsPerTick = data[OFFSET_ENC_STEPS];
            config.EncoderScrollSpeed = data[OFFSET_ENC_SCROLL];
            if (data.Length >= OFFSET_JOY_SENS + 2)
            {
                config.JoystickSensitivity = BitConverter.ToUInt16(data, OFFSET_JOY_SENS) / 1000.0;
            }
        }

        return config;
    }

    /// <summary>
    /// 序列化配置
    /// </summary>
    private byte[] SerializeConfig(DeviceConfig config)
    {
        byte[] data = new byte[CONFIG_SIZE];

        // 魔数
        BitConverter.GetBytes(CONFIG_MAGIC).CopyTo(data, OFFSET_MAGIC);

        // 版本
        BitConverter.GetBytes((ushort)config.Version).CopyTo(data, OFFSET_VERSION);

        // DPI
        BitConverter.GetBytes(config.Dpi).CopyTo(data, OFFSET_DPI);

        // 摇杆死区
        BitConverter.GetBytes(config.JoystickDeadzone).CopyTo(data, OFFSET_JOYSTICK_DEADZONE);

        // 编码器方向
        data[OFFSET_ENCODER_REVERSE] = (byte)(config.EncoderReverse ? 1 : 0);

        // 序列号（暂时用 0）
        BitConverter.GetBytes((ushort)0).CopyTo(data, OFFSET_SEQ);

        // 按键映射
        if (config.Keymap != null)
        {
            int len = Math.Min(64, config.Keymap.Length);
            Array.Copy(config.Keymap, 0, data, OFFSET_KEYMAP, len);
        }

        // Fn 层按键映射
        if (config.FnKeymap != null)
        {
            int len = Math.Min(64, config.FnKeymap.Length);
            Array.Copy(config.FnKeymap, 0, data, OFFSET_FN_KEYMAP, len);
        }

        // v3 新增字段
        data[OFFSET_JOY_INV_X] = (byte)(config.JoystickInvertX ? 1 : 0);
        data[OFFSET_JOY_INV_Y] = (byte)(config.JoystickInvertY ? 1 : 0);
        data[OFFSET_ENC_STEPS] = (byte)Math.Clamp(config.EncoderStepsPerTick, 0, 255);
        data[OFFSET_ENC_SCROLL] = (byte)Math.Clamp(config.EncoderScrollSpeed, 0, 255);
        ushort sensRaw = (ushort)Math.Clamp((int)(config.JoystickSensitivity * 1000), 0, 65535);
        BitConverter.GetBytes(sensRaw).CopyTo(data, OFFSET_JOY_SENS);

        // CRC32（与固件端 crc32_calc 算法一致：标准 IEEE 802.3 CRC32）
        uint crc = Crc32Calculate(data, 0, OFFSET_CRC32);
        BitConverter.GetBytes(crc).CopyTo(data, OFFSET_CRC32);

        return data;
    }

    /// <summary>
    /// 计算 CRC32（标准 IEEE 802.3，多项式 0xEDB88320 反转）
    /// 与固件端 crc32_calc 算法一致
    /// </summary>
    private static uint Crc32Calculate(byte[] data, int offset, int length)
    {
        uint crc = 0xFFFFFFFF;
        for (int i = offset; i < offset + length && i < data.Length; i++)
        {
            crc ^= data[i];
            for (int j = 0; j < 8; j++)
            {
                if ((crc & 1) != 0)
                    crc = (crc >> 1) ^ 0xEDB88320u;
                else
                    crc >>= 1;
            }
        }
        return ~crc;
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
            byte[] data = SerializeConfig(config);

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

                    bool ok0 = await _hidDriver.SendFeatureReportAsync(REPORT_ID_CONFIG_BLOCK0, block0);
                    bool ok1 = await _hidDriver.SendFeatureReportAsync(REPORT_ID_CONFIG_BLOCK1, block1);
                    bool ok2 = await _hidDriver.SendFeatureReportAsync(REPORT_ID_CONFIG_BLOCK2, block2);

                    if (!ok0 || !ok1 || !ok2)
                    {
                        string failBlocks = $"{(!ok0 ? "0 " : "")}{(!ok1 ? "1 " : "")}{(!ok2 ? "2 " : "")}".Trim();
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
    private async Task<bool> ReconnectDeviceAsync()
    {
        try
        {
            // 在锁内读取设备路径，防止与 Disconnect() 竞态
            string devicePath;
            lock (_stateLock)
            {
                devicePath = _currentDevice?.DevicePath ?? "";
            }
            if (string.IsNullOrEmpty(devicePath))
            {
                Log("WARNING", "重连失败：没有设备路径");
                return false;
            }

            // 关闭旧句柄
            try
            {
                _hidDriver.Close();
            }
            catch (Exception ex)
            {
                Log("WARNING", $"关闭旧句柄时异常: {ex.Message}");
            }

            // 等待设备稳定
            await Task.Delay(50);

            // 重新打开
            bool reopened = await _hidDriver.OpenAsync(devicePath);
            if (!reopened)
            {
                // 重新打开失败，标记为断开连接
                Log("ERROR", "重连失败：无法打开设备");
                lock (_stateLock)
                {
                    _currentDevice = null;
                    _currentConfig = null;
                }
                FirmwareVersion = null;
                DeviceConnectionChanged?.Invoke(this, false);
                return false;
            }

            Log("INFO", "设备重连成功");
            return true;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"重连异常: {ex.GetType().Name} - {ex.Message}");
            lock (_stateLock)
            {
                _currentDevice = null;
                _currentConfig = null;
            }
            FirmwareVersion = null;
            DeviceConnectionChanged?.Invoke(this, false);
            return false;
        }
    }

    /// <summary>
    /// 设备状态预检（Ping）
    /// 通过读取控制状态报告确认设备是否在线
    /// </summary>
    private async Task<bool> PingDeviceAsync()
    {
        if (!IsConnected)
            return false;

        try
        {
            byte[]? response = await _hidDriver.GetFeatureReportAsync(REPORT_ID_CONTROL);
            return response != null && response.Length >= 1;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 解锁配置写入（需连续发送3次，5秒内）
    /// 固件默认锁定配置，防止恶意程序篡改。写入配置前必须先解锁。
    /// </summary>
    private async Task<bool> UnlockConfigAsync()
    {
        Log("INFO", "正在解锁配置写入...");
        for (int i = 0; i < 3; i++)
        {
            bool result = await SendControlCommandAsync(CMD_UNLOCK_CONFIG);
            if (!result)
            {
                Log("ERROR", $"解锁配置失败（第{i + 1}/3次）");
                return false;
            }
            if (i < 2) await Task.Delay(100);
        }
        Log("INFO", "配置已解锁（30秒后自动锁定）");
        return true;
    }

    private async Task<bool> SendControlCommandAsync(byte command)
    {
        try
        {
            byte[] data = new byte[1] { command };
            return await SendFeatureReportWithTimeoutAsync(REPORT_ID_CONTROL, data);
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
            return await SendFeatureReportWithTimeoutAsync(REPORT_ID_CONTROL, data);
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
            return await SendFeatureReportWithTimeoutAsync(REPORT_ID_CONTROL, data);
        }
        catch
        {
            return false;
        }
    }

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
            return null;
        }
    }

    /// <summary>
    /// 获取按键统计数据
    /// </summary>
    public async Task<uint[]?> GetKeyStatsAsync()
    {
        if (!IsConnected)
            return null;

        try
        {
            uint[] stats = new uint[64];
            byte[] reportIds = { REPORT_ID_KEY_STATS0, REPORT_ID_KEY_STATS1, REPORT_ID_KEY_STATS2, REPORT_ID_KEY_STATS3 };

            for (int block = 0; block < 4; block++)
            {
                byte[]? data = await _hidDriver.GetFeatureReportAsync(reportIds[block]);
                if (data == null || data.Length < 32)
                {
                    Log("WARNING", $"读取按键统计块 {block} 失败");
                    return null;
                }

                // 解析16个uint16_t（小端）
                for (int i = 0; i < 16; i++)
                {
                    int idx = block * 16 + i;
                    if (idx >= 64) break;
                    ushort val = (ushort)(data[i * 2] | (data[i * 2 + 1] << 8));
                    stats[idx] = val;
                }
            }

            return stats;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取按键统计失败: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 清零按键统计
    /// </summary>
    public async Task<bool> ResetKeyStatsAsync()
    {
        if (!IsConnected)
            return false;

        try
        {
            return await SendControlCommandAsync(CMD_RESET_STATS);
        }
        catch
        {
            return false;
        }
    }

    #region 宏配置

    /// <summary>
    /// 读取一个宏的配置数据
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <returns>宏的原始字节数据，失败返回null</returns>
    public async Task<byte[]?> GetMacroAsync(byte macroId)
    {
        if (!IsConnected || macroId >= 8)
            return null;

        try
        {
            await _writeLock.WaitAsync();
            try
            {
                byte[] macroData = new byte[148]; // 宏数据总大小148字节
                int bytesRead = 0;

                // 分3块读取
                for (byte block = 0; block < 3; block++)
                {
                    // 1. 先设置读取索引（SET_REPORT）
                    // block最高位设为1，表示这是读取命令，只设置索引，不写入数据
                    // 注意：HID报告必须是完整长度，否则Windows会拒绝发送
                    byte[] indexData = new byte[MACRO_REPORT_SIZE];
                    indexData[0] = macroId;
                    indexData[1] = (byte)(0x80 | block);  // 最高位=1表示读命令
                    await _hidDriver.SendFeatureReportAsync(REPORT_ID_MACRO_CONFIG, indexData);

                    // 短暂延迟，确保设备处理
                    await Task.Delay(5);

                    // 2. 再读取数据（GET_REPORT）
                    byte[]? blockData = await _hidDriver.GetFeatureReportAsync(REPORT_ID_MACRO_CONFIG);
                    if (blockData == null || blockData.Length < 2)
                    {
                        Log("ERROR", $"读取宏 {macroId} 块 {block} 失败");
                        return null;
                    }

                    // 3. 复制数据（前2字节是宏ID和块号，后面是数据）
                    int offset = block * 60;
                    int copyLen = Math.Min(60, blockData.Length - 2);
                    if (offset + copyLen > macroData.Length)
                        copyLen = macroData.Length - offset;

                    Array.Copy(blockData, 2, macroData, offset, copyLen);
                    bytesRead += copyLen;
                }

                Log("INFO", $"读取宏 {macroId} 成功，共 {bytesRead} 字节");
                return macroData;
            }
            finally
            {
                _writeLock.Release();
            }
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取宏 {macroId} 异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 写入一个宏的配置数据
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <param name="data">宏的原始字节数据</param>
    /// <returns>是否成功</returns>
    public async Task<bool> SetMacroAsync(byte macroId, byte[] data)
    {
        if (!IsConnected || macroId >= 8 || data == null || data.Length == 0)
            return false;

        try
        {
            await _writeLock.WaitAsync();
            try
            {
                // 分3块写入
                for (byte block = 0; block < 3; block++)
                {
                    int offset = block * 60;
                    if (offset >= data.Length)
                        break;

                    int copyLen = Math.Min(MACRO_BLOCK_DATA_SIZE, data.Length - offset);

                    // 构造数据：[宏ID, 块号, ...数据...]
                    // 注意：HID报告必须是完整长度，否则Windows会拒绝发送
                    byte[] blockData = new byte[MACRO_REPORT_SIZE];
                    blockData[0] = macroId;
                    blockData[1] = block;
                    Array.Copy(data, offset, blockData, 2, copyLen);

                    // 写入
                    bool ok = await _hidDriver.SendFeatureReportAsync(REPORT_ID_MACRO_CONFIG, blockData);
                    if (!ok)
                    {
                        Log("ERROR", $"写入宏 {macroId} 块 {block} 失败");
                        return false;
                    }

                    // 短暂延迟
                    await Task.Delay(5);
                }

                Log("INFO", $"写入宏 {macroId} 成功，共 {data.Length} 字节");
                return true;
            }
            finally
            {
                _writeLock.Release();
            }
        }
        catch (Exception ex)
        {
            Log("ERROR", $"写入宏 {macroId} 异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 播放指定宏
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <returns>是否成功发送命令</returns>
    public async Task<bool> PlayMacroAsync(byte macroId)
    {
        if (!IsConnected || macroId >= 8)
            return false;

        try
        {
            bool result = await SendControlCommandAsync(CMD_MACRO_PLAY, macroId);
            if (result)
            {
                Log("INFO", $"播放宏 {macroId}");
            }
            else
            {
                Log("ERROR", $"播放宏 {macroId} 失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"播放宏 {macroId} 异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 停止指定宏
    /// </summary>
    /// <param name="macroId">宏ID (0-7)，0xFF 表示停止所有宏</param>
    /// <returns>是否成功发送命令</returns>
    public async Task<bool> StopMacroAsync(byte macroId)
    {
        if (!IsConnected)
            return false;

        try
        {
            bool result = await SendControlCommandAsync(CMD_MACRO_STOP, macroId);
            if (result)
            {
                Log("INFO", macroId == 0xFF ? "停止所有宏" : $"停止宏 {macroId}");
            }
            else
            {
                Log("ERROR", $"停止宏 {macroId} 失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"停止宏 {macroId} 异常: {ex.Message}");
            return false;
        }
    }

    #endregion

    #region 错误日志

    /// <summary>
    /// 获取错误日志信息
    /// </summary>
    public async Task<ErrorLogInfo?> GetErrorLogInfoAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;

        try
        {
            await _writeLock.WaitAsync(cancellationToken);
            try
            {
                byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_FAULT_INFO);
                if (data == null || data.Length < 8)
                {
                    Log("ERROR", "读取错误日志信息失败");
                    return null;
                }

                var info = new ErrorLogInfo
                {
                    LogCount = BitConverter.ToUInt32(data, 0),
                    TotalFaultCount = BitConverter.ToUInt32(data, 4)
                };

                Log("INFO", $"读取错误日志信息成功: {info.LogCount} 条日志, 总故障 {info.TotalFaultCount} 次");
                return info;
            }
            finally
            {
                _writeLock.Release();
            }
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取错误日志信息异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 读取指定索引的错误日志
    /// </summary>
    public async Task<ErrorLogEntry?> ReadErrorLogEntryAsync(byte index, CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;

        try
        {
            await _writeLock.WaitAsync(cancellationToken);
            try
            {
                // 1. 先设置读取索引（SET_REPORT）
                // 注意：HID报告必须是完整长度，否则Windows会拒绝发送
                byte[] indexData = new byte[62];
                indexData[0] = index;
                bool ok = await _hidDriver.SendFeatureReportAsync(REPORT_ID_FAULT_LOG, indexData);
                if (!ok)
                {
                    Log("ERROR", $"设置错误日志读取索引 {index} 失败");
                    return null;
                }

                // 短暂延迟
                await Task.Delay(5, cancellationToken);

                // 2. 再读取数据（GET_REPORT）
                byte[]? data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_FAULT_LOG);
                if (data == null || data.Length < 8)
                {
                    Log("ERROR", $"读取错误日志 {index} 失败");
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
                    int moduleLen = Math.Min((int)entry.ModuleLength, 31);
                    if (moduleLen > 0 && data.Length >= 8 + moduleLen)
                    {
                        entry.Module = System.Text.Encoding.UTF8.GetString(data, 8, moduleLen);
                    }

                    // 读取消息（最多22字节）
                    int msgOffset = 40;
                    int maxMsgLen = Math.Min(21, data.Length - msgOffset);
                    if (maxMsgLen > 0)
                    {
                        // 找到字符串结束位置
                        int msgLen = 0;
                        for (int i = 0; i < maxMsgLen; i++)
                        {
                            if (data[msgOffset + i] == 0)
                                break;
                            msgLen++;
                        }
                        if (msgLen > 0)
                        {
                            entry.Message = System.Text.Encoding.UTF8.GetString(data, msgOffset, msgLen);
                        }
                    }
                }

                Log("INFO", $"读取错误日志 {index} 成功: {entry.LevelName} - {entry.Module} - {entry.Message}");
                return entry;
            }
            finally
            {
                _writeLock.Release();
            }
        }
        catch (Exception ex)
        {
            Log("ERROR", $"读取错误日志 {index} 异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 获取所有错误日志
    /// </summary>
    public async Task<IReadOnlyList<ErrorLogEntry>?> GetAllErrorLogsAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return null;

        try
        {
            // 先获取日志数量
            var info = await GetErrorLogInfoAsync(cancellationToken);
            if (info == null)
                return null;

            var logs = new List<ErrorLogEntry>();
            int count = (int)Math.Min(info.LogCount, 16);  // 最多16条

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

            Log("INFO", $"获取所有错误日志成功，共 {logs.Count} 条");
            return logs.AsReadOnly();
        }
        catch (Exception ex)
        {
            Log("ERROR", $"获取所有错误日志异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 清除所有错误日志
    /// </summary>
    public async Task<bool> ClearErrorLogsAsync(CancellationToken cancellationToken = default)
    {
        if (!IsConnected)
            return false;

        try
        {
            bool result = await SendControlCommandAsync(CMD_CLEAR_FAULT);
            if (result)
            {
                Log("INFO", "清除错误日志成功");
            }
            else
            {
                Log("ERROR", "清除错误日志失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"清除错误日志异常: {ex.Message}");
            return false;
        }
    }

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
    /// 设置指针加速
    /// </summary>
    public async Task<bool> SetAccelerationAsync(bool enabled, double threshold, double ratio)
    {
        if (_currentConfig == null)
            return false;

        _currentConfig.AccelerationEnabled = enabled;
        _currentConfig.AccelerationThreshold = threshold;
        _currentConfig.AccelerationRatio = ratio;

        // 注意：固件当前版本不支持加速设置，这里只更新本地配置
        return true;
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
                // 重新加载配置
                _currentConfig = await ReadConfigFromDeviceAsync();
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
            byte[] data = SerializeConfig(_currentConfig);
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

            var config = ParseConfig(data);
            if (config == null)
            {
                Log("ERROR", "导入配置失败：解析配置数据失败");
                return false;
            }
            _currentConfig = config;

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
        if (_heartbeatCts != null)
            return; // 已经启动了

        _heartbeatCts = new CancellationTokenSource();
        _ = HeartbeatLoopAsync(_heartbeatCts.Token);
        Log("INFO", "自动连接与心跳检测已启动");
    }

    /// <summary>
    /// 停止自动连接和心跳检测
    /// </summary>
    public void StopAutoConnect()
    {
        _heartbeatCts?.Cancel();
        _heartbeatCts?.Dispose();
        _heartbeatCts = null;
        Log("INFO", "自动连接与心跳检测已停止");
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
            // 读取设备信息报告，能读成功就说明设备还活着
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

                // 搜索设备
                var devices = await _hidDriver.FindDevicesAsync(DeviceVendorId, DeviceProductId);

                // 过滤：只保留 Vendor 配置接口（UsagePage == 0xFF00）
                var targetDevice = devices.FirstOrDefault(d => d.UsagePage == 0xFF00);

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
    /// 进入 BOOTSEL 烧录模式
    /// </summary>
    public async Task<bool> EnterBootselAsync()
    {
        if (!IsConnected)
            return false;

        try
        {
            Log("INFO", "正在进入 BOOTSEL 模式...");
            // 固件要求连续发送3次（5秒内）才执行，防止恶意DoS
            bool result = false;
            for (int i = 0; i < 3; i++)
            {
                result = await SendControlCommandAsync(CMD_ENTER_DFU);
                if (!result) break;
                if (i < 2) await Task.Delay(100);
            }

            if (result)
            {
                Log("INFO", "BOOTSEL 命令已发送（3次确认），设备将进入烧录模式");
                // 设备会进入 BOOTSEL 模式，断开连接
                Disconnect();
            }
            else
            {
                Log("ERROR", "BOOTSEL 命令发送失败");
            }

            return result;
        }
        catch (Exception ex)
        {
            Log("ERROR", $"进入 BOOTSEL 异常: {ex.Message}");
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
            _disposed = true;
        }
    }
}
