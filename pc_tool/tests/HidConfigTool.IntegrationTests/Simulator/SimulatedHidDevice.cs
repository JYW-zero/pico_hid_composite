/*
 * SimulatedHidDevice.cs
 * 模拟 HID 设备核心
 * 实现设备状态管理、配置存储、命令处理、固件升级协议
 */

using System.Collections.Concurrent;
using System.Text;

namespace HidConfigTool.IntegrationTests.Simulator;

/// <summary>
/// 模拟 HID 设备
/// </summary>
public class SimulatedHidDevice
{
    #region 常量定义

    // Report ID 定义
    private const byte REPORT_ID_CONFIG_BLOCK0 = 0x05;
    private const byte REPORT_ID_DEVICE_INFO = 0x06;
    private const byte REPORT_ID_CONTROL = 0x07;
    private const byte REPORT_ID_CONFIG_BLOCK1 = 0x08;
    private const byte REPORT_ID_CONFIG_BLOCK2 = 0x09;
    private const byte REPORT_ID_KEY_STATS0 = 0x0A;
    private const byte REPORT_ID_KEY_STATS1 = 0x0B;
    private const byte REPORT_ID_KEY_STATS2 = 0x0C;
    private const byte REPORT_ID_KEY_STATS3 = 0x0D;
    private const byte REPORT_ID_MACRO_CONFIG = 0x0E;
    private const byte REPORT_ID_PERF_SYSTEM = 0x0F;
    private const byte REPORT_ID_PERF_TASK = 0x10;
    private const byte REPORT_ID_FAULT_INFO = 0x11;
    private const byte REPORT_ID_FAULT_LOG = 0x12;
    private const byte REPORT_ID_KEY_STATE = 0x13;
    private const byte REPORT_ID_JOYSTICK_STATE = 0x14;
    private const byte REPORT_ID_CONFIG_EXT = 0x15;
    private const byte REPORT_ID_JOYSTICK_CALIB = 0x17;
    private const byte REPORT_ID_CONFIG_EXT2 = 0x19;
    private const byte REPORT_ID_ACK = 0x18;          // 命令应答 Input 报告（与固件/ProtocolConstants.ReportIdAck 一致）

    // 控制命令
    private const byte CMD_SAVE_CONFIG = 0x01;
    private const byte CMD_RESET_CONFIG = 0x02;
    private const byte CMD_REBOOT = 0x03;
    private const byte CMD_ENTER_DFU = 0x04;
    private const byte CMD_APPLY_CONFIG = 0x05;
    private const byte CMD_RESET_STATS = 0x06;
    private const byte CMD_CLEAR_FAULT = 0x07;
    private const byte CMD_RESET_PERF = 0x08;
    private const byte CMD_MACRO_PLAY = 0x09;
    private const byte CMD_MACRO_STOP = 0x0A;
    private const byte CMD_SET_PERF_ENABLE = 0x0B;
    private const byte CMD_SET_JOYSTICK_DZ_RT = 0x0C;
    private const byte CMD_UNLOCK_CONFIG = 0x0D;
    private const byte CMD_JOYSTICK_CALIBRATE = 0x0E;
    private const byte CMD_SWITCH_DPI = 0x0F;

    // 配置大小（与固件 device_config_t / ProtocolConstants.ConfigSize 对齐：v8 = 1510）
    private const int CONFIG_SIZE = 1510;
    private const int BLOCK_SIZE = 62;
    private const int OFFSET_CRC32 = 1506; // crc32 位于末尾 4 字节（1510-4）

    // 设备信息
    public const ushort DEFAULT_VENDOR_ID = 0x1209; // pid.codes VID（免费，合法可用）
    public const ushort DEFAULT_PRODUCT_ID = 0x4004; // Pico HID Composite
    public const ushort DEFAULT_USAGE_PAGE = 0xFF00; // Vendor Defined
    public const ushort DEFAULT_USAGE_ID = 0x0001;

    #endregion

    #region 设备状态

    /// <summary>
    /// 设备行为配置
    /// </summary>
    public DeviceBehavior Behavior { get; } = new();

    /// <summary>
    /// 设备配置数据（1510字节，v8）
    /// </summary>
    private byte[] _configData = new byte[CONFIG_SIZE];

    /// <summary>
    /// 待上报给主机的 Input 报告队列（命令 ACK 等），由驱动 ReadInputReportAsync 取出
    /// </summary>
    private readonly ConcurrentQueue<byte[]> _inputReports = new();

    /// <summary>
    /// 最近一条控制命令的 v3.1 Feature 应答（GetFeatureReport(0x18) 轮询读取）
    /// 布局：[status, cmd, sw_id, device_idx, param_lo, param_hi, reserved, reserved]
    /// </summary>
    private byte[]? _lastAckFeature;

    /// <summary>
    /// 临时配置数据（用于 APPLY_CONFIG）
    /// </summary>
    private byte[]? _tempConfigData;

    /// <summary>
    /// 设备是否已连接
    /// </summary>
    public bool IsConnected { get; private set; } = true;

    /// <summary>
    /// 配置是否已解锁
    /// </summary>
    public bool IsConfigUnlocked { get; private set; }

    /// <summary>
    /// 解锁尝试次数
    /// </summary>
    private int _unlockAttempts;

    /// <summary>
    /// 最后一次解锁时间
    /// </summary>
    private DateTime _lastUnlockTime;

    /// <summary>
    /// 性能监控是否启用
    /// </summary>
    public bool PerfMonitorEnabled { get; private set; } = true;

    /// <summary>
    /// 当前 DPI 档位
    /// </summary>
    public int CurrentDpiSlot { get; private set; } = 1;

    /// <summary>
    /// 摇杆实时死区
    /// </summary>
    public ushort RealtimeDeadzone { get; private set; }

    /// <summary>
    /// 设备是否在 DFU 模式
    /// </summary>
    public bool IsInDfuMode { get; private set; }

    /// <summary>
    /// 固件升级数据（用于 DFU 回显测试）
    /// </summary>
    public List<byte> FirmwareUpgradeData { get; } = new();

    /// <summary>
    /// 操作日志
    /// </summary>
    public List<string> OperationLog { get; } = new();

    /// <summary>
    /// 按键统计数据（64个键）
    /// </summary>
    private uint[] _keyStats = new uint[64];

    /// <summary>
    /// 错误日志条目
    /// </summary>
    private List<FaultLogEntry> _faultLogs = new();

    /// <summary>
    /// 宏配置数据
    /// </summary>
    private byte[] _macroConfigData = new byte[1024];

    /// <summary>
    /// CONFIG_EXT2 当前子块索引（SET_REPORT设置，GET_REPORT返回对应子块数据）
    /// </summary>
    private byte _configExt2SubBlock = 0;

    /// <summary>
    /// 设备序列号
    /// </summary>
    public string SerialNumber { get; set; } = "SIM-000001";

    /// <summary>
    /// 产品名称
    /// </summary>
    public string ProductName { get; set; } = "Pico HID Composite Device (Simulated)";

    /// <summary>
    /// 制造商名称
    /// </summary>
    public string ManufacturerName { get; set; } = "Simulated Manufacturer";

    #endregion

    #region 构造函数

    public SimulatedHidDevice()
    {
        // 初始化默认配置
        InitializeDefaultConfig();

        // 添加一些初始错误日志
        AddFaultLog("INFO", "system", "Device initialized (simulated)");
    }

    #endregion

    #region 初始化

    /// <summary>
    /// 初始化默认配置
    /// </summary>
    private void InitializeDefaultConfig()
    {
        // 清零
        Array.Clear(_configData, 0, CONFIG_SIZE);

        // 设置 magic（与固件 CONFIG_MAGIC = 0x5A5A5A5A 一致，小端）
        _configData[0] = 0x5A;
        _configData[1] = 0x5A;
        _configData[2] = 0x5A;
        _configData[3] = 0x5A;

        // 设置版本（偏移4-5，配置结构版本 v8）
        BitConverter.TryWriteBytes(_configData.AsSpan(4, 2), (ushort)8);

        // 设置默认 DPI（假设偏移6-7是DPI）
        BitConverter.TryWriteBytes(_configData.AsSpan(6, 2), (ushort)800);

        // 设置默认摇杆死区（假设偏移8-9）
        BitConverter.TryWriteBytes(_configData.AsSpan(8, 2), (ushort)100);

        // 设置默认摇杆灵敏度（假设偏移12-15，float）
        BitConverter.TryWriteBytes(_configData.AsSpan(12, 4), 1.0f);

        // 计算 CRC32
        uint crc = CalculateCrc32(_configData.AsSpan(0, OFFSET_CRC32));
        BitConverter.TryWriteBytes(_configData.AsSpan(OFFSET_CRC32, 4), crc);
    }

    #endregion

    #region Feature Report 处理

    /// <summary>
    /// 处理 GetFeatureReport 请求
    /// </summary>
    public byte[]? HandleGetFeatureReport(byte reportId)
    {
        // 检查设备行为
        if (Behavior.ShouldFail())
        {
            return GenerateErrorResponse(reportId);
        }

        // 检查设备是否断开
        if (!IsConnected || Behavior.Mode == DeviceBehaviorMode.Disconnected)
        {
            return null;
        }

        LogOperation($"GET Feature Report: 0x{reportId:X2}");

        return reportId switch
        {
            REPORT_ID_DEVICE_INFO => GetDeviceInfo(),
            REPORT_ID_CONFIG_BLOCK0 => GetConfigBlock(0),
            REPORT_ID_CONFIG_BLOCK1 => GetConfigBlock(1),
            REPORT_ID_CONFIG_BLOCK2 => GetConfigBlock(2),
            REPORT_ID_CONFIG_EXT => GetConfigExt(),
            REPORT_ID_KEY_STATS0 => GetKeyStatsBlock(0),
            REPORT_ID_KEY_STATS1 => GetKeyStatsBlock(1),
            REPORT_ID_KEY_STATS2 => GetKeyStatsBlock(2),
            REPORT_ID_KEY_STATS3 => GetKeyStatsBlock(3),
            REPORT_ID_MACRO_CONFIG => GetMacroConfig(),
            REPORT_ID_PERF_SYSTEM => GetPerfSystem(),
            REPORT_ID_PERF_TASK => GetPerfTask(),
            REPORT_ID_FAULT_INFO => GetFaultInfo(),
            REPORT_ID_FAULT_LOG => GetFaultLog(),
            REPORT_ID_KEY_STATE => GetKeyState(),
            REPORT_ID_JOYSTICK_STATE => GetJoystickState(),
            REPORT_ID_JOYSTICK_CALIB => GetJoystickCalibration(),
            REPORT_ID_CONFIG_EXT2 => GetConfigExt2(),
            REPORT_ID_ACK => GetAckFeatureResponse(),
            _ => new byte[64] // 默认返回空数据
        };
    }

    /// <summary>
    /// 处理 SendFeatureReport 请求
    /// </summary>
    public bool HandleSendFeatureReport(byte reportId, byte[] data)
    {
        // 检查设备行为
        if (Behavior.ShouldFail())
        {
            LogOperation($"SEND Feature Report FAILED (behavior): 0x{reportId:X2}");
            return false;
        }

        // 检查设备是否断开
        if (!IsConnected || Behavior.Mode == DeviceBehaviorMode.Disconnected)
        {
            return false;
        }

        LogOperation($"SEND Feature Report: 0x{reportId:X2}, length={data.Length}");

        switch (reportId)
        {
            case REPORT_ID_CONFIG_BLOCK0:
                SetConfigBlock(0, data);
                return true;
            case REPORT_ID_CONFIG_BLOCK1:
                SetConfigBlock(1, data);
                return true;
            case REPORT_ID_CONFIG_BLOCK2:
                SetConfigBlock(2, data);
                return true;
            case REPORT_ID_CONFIG_EXT:
                SetConfigExt(data);
                return true;
            case REPORT_ID_CONFIG_EXT2:
                SetConfigExt2(data);
                return true;
            case REPORT_ID_CONTROL:
            {
                bool ctrlOk = HandleControlCommand(data);
                byte ctrlCmd = data.Length > 0 ? data[0] : (byte)0;
                // 重启/进 DFU 时设备会立即复位、不回 ACK（与 DeviceService.IsRebootCommand 对应）
                if (ctrlCmd != CMD_REBOOT && ctrlCmd != CMD_ENTER_DFU)
                {
                    ushort ctrlParam = data.Length >= 3 ? BitConverter.ToUInt16(data, 1) : (ushort)0;
                    EnqueueAck(ctrlCmd, ctrlOk, ctrlParam);
                }
                return ctrlOk;
            }
            case REPORT_ID_MACRO_CONFIG:
                SetMacroConfig(data);
                return true;
            case REPORT_ID_PERF_TASK:
                // 设置性能监控任务索引
                return true;
            default:
                return true;
        }
    }

    #endregion

    #region 设备信息

    /// <summary>
    /// 获取设备信息
    /// </summary>
    private byte[] GetDeviceInfo()
    {
        byte[] data = new byte[64];

        // 固件版本 v1.0.0 (bcdDevice 格式 0x0100)
        BitConverter.TryWriteBytes(data.AsSpan(0, 2), (ushort)0x0100);

        // 配置版本 v8
        BitConverter.TryWriteBytes(data.AsSpan(2, 2), (ushort)8);

        // 协议版本 v1.0
        data[4] = 1; // 主版本
        data[5] = 0; // 次版本

        // 配置大小
        BitConverter.TryWriteBytes(data.AsSpan(6, 2), (ushort)CONFIG_SIZE);

        // 硬件版本
        data[8] = 1;
        data[9] = 0;

        // 设备特性标志
        data[10] = 0xFF; // 支持所有功能

        // 能力位图区（不含 Report ID：capability_count=data[11]，capability_bitmap=data[12..19]，
        // 小端 uint64）当前保持全 0：本模拟器尚未按协议 v0.2 布局建模位图（协议版本仍报 1.0，
        // 属已知遗留失真）。因此所有能力位——含 CAP_DFU_UPGRADE(bit10)，即自定义 DFU bootloader
        // 当前固件未编入——均为“未上报”。命令级集成测试不依赖能力显隐，故不受影响；
        // 待模拟器按 v0.2 真实化时，应在此填入 release 位图（bit10=0、BOOTSEL UF2 不占位）。

        return data;
    }

    #endregion

    #region 配置块读写

    /// <summary>
    /// 获取配置块
    /// </summary>
    private byte[] GetConfigBlock(int blockIndex)
    {
        byte[] data = new byte[64];
        int offset = blockIndex * BLOCK_SIZE;
        int length = Math.Min(BLOCK_SIZE, CONFIG_SIZE - offset);

        if (length > 0 && offset < CONFIG_SIZE)
        {
            Array.Copy(_configData, offset, data, 0, length);
        }

        return data;
    }

    /// <summary>
    /// 设置配置块
    /// </summary>
    private void SetConfigBlock(int blockIndex, byte[] data)
    {
        // 检查配置是否锁定
        if (!IsConfigUnlocked)
        {
            LogOperation("CONFIG BLOCK WRITE REJECTED: config locked");
            return;
        }

        int offset = blockIndex * BLOCK_SIZE;
        int length = Math.Min(data.Length, BLOCK_SIZE);

        if (offset + length <= CONFIG_SIZE)
        {
            // 初始化临时配置
            _tempConfigData ??= new byte[CONFIG_SIZE];
            Array.Copy(_configData, _tempConfigData, CONFIG_SIZE);

            // 复制数据
            Array.Copy(data, 0, _tempConfigData, offset, length);
            LogOperation($"CONFIG BLOCK {blockIndex} written to temp config");
        }
    }

    /// <summary>
    /// 获取配置扩展块
    /// </summary>
    private byte[] GetConfigExt()
    {
        byte[] data = new byte[64];
        // 扩展配置从偏移 186 开始
        int offset = 186;
        int length = Math.Min(62, CONFIG_SIZE - offset);

        if (length > 0 && offset < CONFIG_SIZE)
        {
            Array.Copy(_configData, offset, data, 0, length);
        }

        return data;
    }

    /// <summary>
    /// 设置配置扩展块
    /// </summary>
    private void SetConfigExt(byte[] data)
    {
        if (!IsConfigUnlocked)
        {
            LogOperation("CONFIG EXT WRITE REJECTED: config locked");
            return;
        }

        int offset = 186;
        int length = Math.Min(data.Length, 62);

        if (offset + length <= CONFIG_SIZE)
        {
            _tempConfigData ??= new byte[CONFIG_SIZE];
            Array.Copy(_configData, _tempConfigData, CONFIG_SIZE);
            Array.Copy(data, 0, _tempConfigData, offset, length);
            LogOperation("CONFIG EXT written to temp config");
        }
    }

    #endregion

    #region 控制命令处理

    /// <summary>
    /// 处理控制命令
    /// </summary>
    private bool HandleControlCommand(byte[] data)
    {
        if (data.Length < 1) return false;

        byte command = data[0];
        ushort parameter = data.Length >= 3 ? BitConverter.ToUInt16(data, 1) : (ushort)0;
        return DispatchCommand(command, parameter);
    }

    /// <summary>
    /// 控制命令分发（旧 feature 帧与 v3.1 output 帧共用同一套命令实现）。
    /// </summary>
    private bool DispatchCommand(byte command, ushort parameter)
    {
        LogOperation($"CONTROL CMD: 0x{command:X2}, param=0x{parameter:X4}");

        switch (command)
        {
            case CMD_UNLOCK_CONFIG:
                return HandleUnlockConfig();
            case CMD_SAVE_CONFIG:
                return HandleSaveConfig();
            case CMD_APPLY_CONFIG:
                return HandleApplyConfig();
            case CMD_RESET_CONFIG:
                return HandleResetConfig();
            case CMD_REBOOT:
                return HandleReboot();
            case CMD_ENTER_DFU:
                return HandleEnterDfu();
            case CMD_RESET_STATS:
                return HandleResetStats();
            case CMD_CLEAR_FAULT:
                return HandleClearFault();
            case CMD_RESET_PERF:
                return HandleResetPerf();
            case CMD_MACRO_PLAY:
                return HandleMacroPlay(parameter);
            case CMD_MACRO_STOP:
                return HandleMacroStop(parameter);
            case CMD_SET_PERF_ENABLE:
                return HandleSetPerfEnable(parameter);
            case CMD_SET_JOYSTICK_DZ_RT:
                return HandleSetJoystickDeadzone(parameter);
            case CMD_JOYSTICK_CALIBRATE:
                return HandleJoystickCalibrate();
            case CMD_SWITCH_DPI:
                return HandleSwitchDpi(parameter);
            default:
                LogOperation($"UNKNOWN CONTROL CMD: 0x{command:X2}");
                return true;
        }
    }

    /// <summary>
    /// 入队一条命令 ACK Input 报告：
    /// [0]=REPORT_ID_ACK,[1]=命令码,[2]=状态(0=OK),[3]=错误码,[4..5]=param(小端)
    /// </summary>
    private void EnqueueAck(byte command, bool ok, ushort param = 0)
    {
        var ack = new byte[64];
        ack[0] = REPORT_ID_ACK;
        ack[1] = command;
        ack[2] = (byte)(ok ? 0 : 9); // 0=OK；失败用 9=NOT_SUPPORTED 占位
        ack[3] = 0;
        ack[4] = (byte)(param & 0xFF);
        ack[5] = (byte)(param >> 8);
        _inputReports.Enqueue(ack);
    }

    /// <summary>
    /// 取出一条待上报的 Input 报告（供驱动轮询），无则返回 null
    /// </summary>
    public byte[]? TryDequeueInputReport()
        => _inputReports.TryDequeue(out byte[]? report) ? report : null;

    /// <summary>
    /// 处理主机下发的 Output 报告。
    /// 控制命令走 v3.1 帧（8 字节）：
    ///   请求 [device_idx, cmd, sw_id, param_lo, param_hi, reserved x3]
    ///   应答 GetFeatureReport(0x18) -> [status, cmd, sw_id, device_idx, param_lo, param_hi, 0, 0]
    /// 重启/DFU 为旧的 1 字节帧且设备立即复位、无应答。
    /// </summary>
    public bool HandleSendOutputReport(byte reportId, byte[] data)
    {
        // 故障注入 / 断开行为与 Feature 发送一致
        if (Behavior.ShouldFail())
        {
            LogOperation($"SEND Output Report FAILED (behavior): 0x{reportId:X2}");
            return false;
        }
        if (!IsConnected || Behavior.Mode == DeviceBehaviorMode.Disconnected)
        {
            return false;
        }

        if (reportId == REPORT_ID_CONTROL && data != null && data.Length >= 5)
        {
            byte deviceIdx = data[0];
            byte command = data[1];
            byte swId = data[2];

            LogOperation($"SEND Output Control(v3.1): cmd=0x{command:X2}, sw_id={swId}, length={data.Length}");
            ushort parameter = (ushort)(data[3] | (data[4] << 8));
            bool ok = DispatchCommand(command, parameter);

            // 复位类命令不产生应答（设备立即重启 / 进 DFU）
            if (command != CMD_REBOOT && command != CMD_ENTER_DFU)
            {
                var ack = new byte[8];
                ack[0] = (byte)(ok ? 0 : 9); // status：0=OK，失败用 9=NOT_SUPPORTED 占位
                ack[1] = command;
                ack[2] = swId;
                ack[3] = deviceIdx;
                ack[4] = data[3];            // param_lo
                ack[5] = data[4];            // param_hi
                _lastAckFeature = ack;
            }
            return ok;
        }

        // 其余 Output 报告（含旧 1 字节 reboot 帧）按 Feature 报告同样处理
        return data != null && HandleSendFeatureReport(reportId, data);
    }

    /// <summary>
    /// 返回最近一条控制命令的 v3.1 应答报告（report id 0x18）。
    /// 无待读应答时返回全零（上位机按 SW ID 不匹配处理）。
    /// </summary>
    private byte[] GetAckFeatureResponse()
    {
        return _lastAckFeature != null ? (byte[])_lastAckFeature.Clone() : new byte[8];
    }

    /// <summary>
    /// 处理解锁配置
    /// </summary>
    private bool HandleUnlockConfig()
    {
        DateTime now = DateTime.Now;

        // 检查是否在5秒窗口内
        if ((now - _lastUnlockTime).TotalSeconds > 5)
        {
            _unlockAttempts = 0;
        }

        _unlockAttempts++;
        _lastUnlockTime = now;

        LogOperation($"UNLOCK attempt {_unlockAttempts}/3");

        if (_unlockAttempts >= 3)
        {
            IsConfigUnlocked = true;
            _unlockAttempts = 0;
            LogOperation("CONFIG UNLOCKED");
            AddFaultLog("INFO", "security", "Config unlocked");
        }

        return true;
    }

    /// <summary>
    /// 处理保存配置
    /// </summary>
    private bool HandleSaveConfig()
    {
        if (!IsConfigUnlocked)
        {
            LogOperation("SAVE CONFIG REJECTED: config locked");
            return false;
        }

        if (_tempConfigData != null)
        {
            // 重新计算 CRC
            uint crc = CalculateCrc32(_tempConfigData.AsSpan(0, OFFSET_CRC32));
            BitConverter.TryWriteBytes(_tempConfigData.AsSpan(OFFSET_CRC32, 4), crc);

            // 应用到正式配置
            Array.Copy(_tempConfigData, _configData, CONFIG_SIZE);
            _tempConfigData = null;

            LogOperation("CONFIG SAVED to flash (simulated)");
            AddFaultLog("INFO", "config", "Config saved");
        }

        // 自动重新锁定
        IsConfigUnlocked = false;
        return true;
    }

    /// <summary>
    /// 处理应用配置（不保存到Flash）
    /// </summary>
    private bool HandleApplyConfig()
    {
        if (_tempConfigData != null)
        {
            Array.Copy(_tempConfigData, _configData, CONFIG_SIZE);
            _tempConfigData = null;
            LogOperation("CONFIG APPLIED (runtime only)");
        }
        return true;
    }

    /// <summary>
    /// 处理重置配置
    /// </summary>
    private bool HandleResetConfig()
    {
        InitializeDefaultConfig();
        _tempConfigData = null;
        LogOperation("CONFIG RESET to defaults");
        AddFaultLog("INFO", "config", "Config reset to defaults");
        return true;
    }

    /// <summary>
    /// 处理重启
    /// </summary>
    private bool HandleReboot()
    {
        LogOperation("DEVICE REBOOT (simulated - device will disconnect)");
        AddFaultLog("INFO", "system", "Device reboot requested");

        // 模拟设备断开
        IsConnected = false;
        Task.Run(async () =>
        {
            await Task.Delay(1000);
            IsConnected = true;
            LogOperation("DEVICE REBOOT COMPLETE (simulated)");
        });

        return true;
    }

    /// <summary>
    /// 处理进入 DFU 模式
    /// </summary>
    private bool HandleEnterDfu()
    {
        IsInDfuMode = true;
        FirmwareUpgradeData.Clear();
        LogOperation("ENTER DFU MODE (simulated)");
        AddFaultLog("INFO", "dfu", "Entered DFU mode");
        return true;
    }

    /// <summary>
    /// 处理重置按键统计
    /// </summary>
    private bool HandleResetStats()
    {
        Array.Clear(_keyStats, 0, _keyStats.Length);
        LogOperation("KEY STATS RESET");
        return true;
    }

    /// <summary>
    /// 处理清除错误日志
    /// </summary>
    private bool HandleClearFault()
    {
        _faultLogs.Clear();
        LogOperation("FAULT LOG CLEARED");
        return true;
    }

    /// <summary>
    /// 处理重置性能统计
    /// </summary>
    private bool HandleResetPerf()
    {
        LogOperation("PERF STATS RESET");
        return true;
    }

    /// <summary>
    /// 处理播放宏
    /// </summary>
    private bool HandleMacroPlay(ushort macroId)
    {
        LogOperation($"MACRO PLAY: id={macroId}");
        return true;
    }

    /// <summary>
    /// 处理停止宏
    /// </summary>
    private bool HandleMacroStop(ushort macroId)
    {
        LogOperation($"MACRO STOP: id={macroId}");
        return true;
    }

    /// <summary>
    /// 处理设置性能监控开关
    /// </summary>
    private bool HandleSetPerfEnable(ushort enable)
    {
        PerfMonitorEnabled = enable != 0;
        LogOperation($"PERF MONITOR: {(PerfMonitorEnabled ? "ENABLED" : "DISABLED")}");
        return true;
    }

    /// <summary>
    /// 处理设置摇杆实时死区
    /// </summary>
    private bool HandleSetJoystickDeadzone(ushort deadzone)
    {
        RealtimeDeadzone = deadzone;
        LogOperation($"JOYSTICK REALTIME DEADZONE: {deadzone}");
        return true;
    }

    /// <summary>
    /// 处理摇杆校准
    /// </summary>
    private bool HandleJoystickCalibrate()
    {
        LogOperation("JOYSTICK CALIBRATION triggered");
        AddFaultLog("INFO", "joystick", "Calibration triggered");
        return true;
    }

    /// <summary>
    /// 处理切换 DPI
    /// </summary>
    private bool HandleSwitchDpi(ushort slot)
    {
        if (slot == 0xFF)
        {
            // 循环切换
            CurrentDpiSlot = (CurrentDpiSlot + 1) % 4;
        }
        else if (slot < 4)
        {
            CurrentDpiSlot = slot;
        }
        LogOperation($"DPI SWITCHED to slot {CurrentDpiSlot}");
        return true;
    }

    #endregion

    #region 其他 Report 处理

    /// <summary>
    /// 获取按键统计块
    /// </summary>
    private byte[] GetKeyStatsBlock(int blockIndex)
    {
        byte[] data = new byte[64];
        int startKey = blockIndex * 16;

        for (int i = 0; i < 16 && startKey + i < 64; i++)
        {
            BitConverter.TryWriteBytes(data.AsSpan(i * 4, 4), _keyStats[startKey + i]);
        }

        return data;
    }

    /// <summary>
    /// 获取宏配置
    /// </summary>
    private byte[] GetMacroConfig()
    {
        byte[] data = new byte[64];
        Array.Copy(_macroConfigData, 0, data, 0, Math.Min(62, _macroConfigData.Length));
        return data;
    }

    /// <summary>
    /// 设置宏配置
    /// </summary>
    private void SetMacroConfig(byte[] data)
    {
        if (!IsConfigUnlocked)
        {
            LogOperation("MACRO CONFIG WRITE REJECTED: config locked");
            return;
        }
        Array.Copy(data, 0, _macroConfigData, 0, Math.Min(data.Length, _macroConfigData.Length));
        LogOperation("MACRO CONFIG written");
    }

    /// <summary>
    /// 获取性能系统状态
    /// </summary>
    private byte[] GetPerfSystem()
    {
        byte[] data = new byte[64];
        data[0] = (byte)(PerfMonitorEnabled ? 1 : 0);
        // CPU 使用率（模拟）
        data[1] = 15; // 15%
        // 内存使用率（模拟）
        data[2] = 45; // 45%
        // 运行时间（秒）
        BitConverter.TryWriteBytes(data.AsSpan(4, 4), (uint)12345);
        return data;
    }

    /// <summary>
    /// 获取性能任务统计
    /// </summary>
    private byte[] GetPerfTask()
    {
        byte[] data = new byte[64];
        // 任务数量
        data[0] = 8;
        // 模拟任务数据
        for (int i = 0; i < 8; i++)
        {
            int offset = 1 + i * 7;
            if (offset + 7 <= data.Length)
            {
                data[offset] = (byte)i; // 任务ID
                BitConverter.TryWriteBytes(data.AsSpan(offset + 1, 4), (uint)(100 + i * 50)); // 执行时间(us)
                data[offset + 5] = (byte)(i * 10); // 超时次数
                data[offset + 6] = 0; // 保留
            }
        }
        return data;
    }

    /// <summary>
    /// 获取错误日志信息
    /// </summary>
    private byte[] GetFaultInfo()
    {
        byte[] data = new byte[64];
        // 总日志数
        BitConverter.TryWriteBytes(data.AsSpan(0, 2), (ushort)_faultLogs.Count);
        // 总故障数
        int faultCount = _faultLogs.Count(f => f.Level == "WARN" || f.Level == "ERROR" || f.Level == "FATAL");
        BitConverter.TryWriteBytes(data.AsSpan(2, 2), (ushort)faultCount);
        // 最新错误时间戳
        if (_faultLogs.Count > 0)
        {
            BitConverter.TryWriteBytes(data.AsSpan(4, 4), (uint)_faultLogs[^1].Timestamp);
        }
        return data;
    }

    /// <summary>
    /// 获取错误日志
    /// </summary>
    private byte[] GetFaultLog()
    {
        byte[] data = new byte[64];
        if (_faultLogs.Count > 0)
        {
            var entry = _faultLogs[0];
            data[0] = (byte)entry.Level.Length;
            Encoding.ASCII.GetBytes(entry.Level, 0, Math.Min(entry.Level.Length, 8), data, 1);
            data[9] = (byte)entry.Module.Length;
            Encoding.ASCII.GetBytes(entry.Module, 0, Math.Min(entry.Module.Length, 8), data, 10);
            Encoding.ASCII.GetBytes(entry.Message, 0, Math.Min(entry.Message.Length, 40), data, 19);
        }
        return data;
    }

    /// <summary>
    /// 获取实时按键状态
    /// </summary>
    private byte[] GetKeyState()
    {
        byte[] data = new byte[64];
        // 模拟：没有按键按下
        for (int i = 0; i < 8; i++)
        {
            data[i] = 0xFF; // 1=未按下，0=按下
        }
        return data;
    }

    /// <summary>
    /// 获取实时摇杆状态
    /// </summary>
    private byte[] GetJoystickState()
    {
        byte[] data = new byte[64];
        // X 轴（中心点 2048）
        BitConverter.TryWriteBytes(data.AsSpan(0, 2), (ushort)2048);
        // Y 轴（中心点 2048）
        BitConverter.TryWriteBytes(data.AsSpan(2, 2), (ushort)2048);
        // 按键状态（1=未按下）
        data[4] = 1;
        return data;
    }

    /// <summary>
    /// 获取摇杆校准状态（Report ID 23，10字节）
    /// 与固件 config_hid.c 中 REPORT_ID_JOYSTICK_CALIB 的 GET_REPORT 格式一致：
    /// byte[0] = 校准状态（0=空闲,1=校准中,2=完成）
    /// byte[1-2] = center_x（小端，ADC值，0-4095）
    /// byte[3-4] = center_y（小端，ADC值，0-4095）
    /// byte[5] = calibrated（0=未校准,1=已校准）
    /// byte[6-7] = 死区值（小端，ADC原始值）
    /// byte[8-9] = 灵敏度（小端，定点数1.0=1000）
    /// </summary>
    private byte[] GetJoystickCalibration()
    {
        byte[] data = new byte[64];
        // 校准状态：2=完成（模拟已校准）
        data[0] = 2;
        // center_x = 2048（默认中心点）
        BitConverter.TryWriteBytes(data.AsSpan(1, 2), (ushort)2048);
        // center_y = 2048（默认中心点）
        BitConverter.TryWriteBytes(data.AsSpan(3, 2), (ushort)2048);
        // calibrated = 1（已校准）
        data[5] = 1;
        // 死区值 = 100（默认值）
        BitConverter.TryWriteBytes(data.AsSpan(6, 2), (ushort)100);
        // 灵敏度 = 1000（1.0 定点数）
        BitConverter.TryWriteBytes(data.AsSpan(8, 2), (ushort)1000);
        return data;
    }

    /// <summary>
    /// 获取 CONFIG_EXT2 子块数据（Report ID 25）
    /// 子块定义:
    ///   0 = v6字段 (偏移1346, 14字节)
    ///   1 = layer2_keymap[0..31] (偏移1360, 32字节)
    ///   2 = layer2_keymap[32..63] (偏移1392, 32字节)
    ///   3 = layer3_keymap[0..31] (偏移1424, 32字节)
    ///   4 = layer3_keymap[32..63] (偏移1456, 32字节)
    /// </summary>
    private byte[] GetConfigExt2()
    {
        byte[] data = new byte[64];

        int offset = _configExt2SubBlock switch
        {
            0 => 1346, // v6字段
            1 => 1360, // layer2_keymap[0..31]
            2 => 1392, // layer2_keymap[32..63]
            3 => 1424, // layer3_keymap[0..31]
            4 => 1456, // layer3_keymap[32..63]
            _ => 1346
        };

        int dataLen = _configExt2SubBlock switch
        {
            0 => 14,
            1 or 2 or 3 or 4 => 32,
            _ => 14
        };

        int length = Math.Min(dataLen, CONFIG_SIZE - offset);
        if (length > 0 && offset < CONFIG_SIZE)
        {
            Array.Copy(_configData, offset, data, 0, length);
        }

        LogOperation($"GET CONFIG_EXT2: subBlock={_configExt2SubBlock}, offset={offset}, length={length}");
        return data;
    }

    /// <summary>
    /// 设置 CONFIG_EXT2 子块数据（Report ID 25）
    /// 发送格式: byte[0]=子块索引, byte[1..]=数据
    /// </summary>
    private void SetConfigExt2(byte[] data)
    {
        if (data.Length < 1) return;

        byte subBlock = data[0];
        if (subBlock > 4)
        {
            LogOperation($"SET CONFIG_EXT2 REJECTED: invalid subBlock {subBlock}");
            return;
        }

        // 设置当前子块索引
        _configExt2SubBlock = subBlock;

        // 如果有数据要写入（多于1字节）
        if (data.Length > 1)
        {
            // 检查配置是否锁定
            if (!IsConfigUnlocked)
            {
                LogOperation("CONFIG_EXT2 WRITE REJECTED: config locked");
                return;
            }

            int offset = subBlock switch
            {
                0 => 1346,
                1 => 1360,
                2 => 1392,
                3 => 1424,
                4 => 1456,
                _ => 1346
            };

            int dataLen = subBlock switch
            {
                0 => 14,
                1 or 2 or 3 or 4 => 32,
                _ => 14
            };

            int length = Math.Min(data.Length - 1, dataLen);
            length = Math.Min(length, CONFIG_SIZE - offset);

            if (length > 0 && offset < CONFIG_SIZE)
            {
                Array.Copy(data, 1, _configData, offset, length);
                LogOperation($"SET CONFIG_EXT2: subBlock={subBlock}, offset={offset}, length={length}");
            }
        }
        else
        {
            LogOperation($"SET CONFIG_EXT2: set subBlock index={subBlock}");
        }
    }

    #endregion

    #region 错误响应生成

    /// <summary>
    /// 生成错误响应
    /// </summary>
    private byte[]? GenerateErrorResponse(byte reportId)
    {
        return Behavior.ErrorType switch
        {
            ErrorResponseType.CrcMismatch => GenerateCrcMismatchResponse(reportId),
            ErrorResponseType.IncompleteData => new byte[10], // 数据不完整
            ErrorResponseType.AllZeros => new byte[64], // 全零
            ErrorResponseType.RandomData => GenerateRandomData(),
            ErrorResponseType.NullResponse => null,
            _ => new byte[64]
        };
    }

    /// <summary>
    /// 生成 CRC 不匹配的响应
    /// </summary>
    private byte[] GenerateCrcMismatchResponse(byte reportId)
    {
        // 先获取正常响应，然后修改 CRC
        byte[]? normalResponse = reportId switch
        {
            REPORT_ID_CONFIG_BLOCK0 => GetConfigBlock(0),
            REPORT_ID_DEVICE_INFO => GetDeviceInfo(),
            _ => new byte[64]
        };

        if (normalResponse != null && normalResponse.Length >= 4)
        {
            // 破坏最后4字节（CRC）
            normalResponse[^1] = 0xFF;
            normalResponse[^2] = 0xFF;
            normalResponse[^3] = 0xFF;
            normalResponse[^4] = 0xFF;
        }

        return normalResponse ?? new byte[64];
    }

    /// <summary>
    /// 生成随机数据
    /// </summary>
    private byte[] GenerateRandomData()
    {
        byte[] data = new byte[64];
        new Random().NextBytes(data);
        return data;
    }

    #endregion

    #region CRC32 计算

    /// <summary>
    /// 计算 CRC32
    /// </summary>
    private static uint CalculateCrc32(ReadOnlySpan<byte> data)
    {
        uint crc = 0xFFFFFFFF;
        foreach (byte b in data)
        {
            crc ^= b;
            for (int i = 0; i < 8; i++)
            {
                crc = (crc & 1) != 0 ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
            }
        }
        return ~crc;
    }

    #endregion

    #region 辅助方法

    /// <summary>
    /// 添加错误日志条目
    /// </summary>
    public void AddFaultLog(string level, string module, string message)
    {
        _faultLogs.Add(new FaultLogEntry
        {
            Level = level,
            Module = module,
            Message = message,
            Timestamp = (uint)(DateTime.Now - DateTime.UnixEpoch).TotalSeconds
        });

        // 限制日志数量
        if (_faultLogs.Count > 100)
        {
            _faultLogs.RemoveAt(0);
        }
    }

    /// <summary>
    /// 记录操作日志
    /// </summary>
    private void LogOperation(string message)
    {
        string timestamp = DateTime.Now.ToString("HH:mm:ss.fff");
        OperationLog.Add($"[{timestamp}] {message}");

        // 限制日志数量
        if (OperationLog.Count > 500)
        {
            OperationLog.RemoveAt(0);
        }
    }

    /// <summary>
    /// 模拟按键按下（用于测试）
    /// </summary>
    public void SimulateKeyPress(int keyIndex)
    {
        if (keyIndex >= 0 && keyIndex < 64)
        {
            _keyStats[keyIndex]++;
            LogOperation($"KEY PRESS simulated: key={keyIndex}, count={_keyStats[keyIndex]}");
        }
    }

    /// <summary>
    /// 重置设备状态
    /// </summary>
    public void Reset()
    {
        InitializeDefaultConfig();
        _tempConfigData = null;
        IsConfigUnlocked = false;
        _unlockAttempts = 0;
        PerfMonitorEnabled = true;
        CurrentDpiSlot = 1;
        RealtimeDeadzone = 0;
        IsInDfuMode = false;
        FirmwareUpgradeData.Clear();
        Array.Clear(_keyStats, 0, _keyStats.Length);
        _faultLogs.Clear();
        Array.Clear(_macroConfigData, 0, _macroConfigData.Length);
        IsConnected = true;
        Behavior.Reset();
        OperationLog.Clear();
        AddFaultLog("INFO", "system", "Device reset (simulated)");
        LogOperation("DEVICE RESET");
    }

    /// <summary>
    /// 获取当前配置数据（用于测试验证）
    /// </summary>
    public byte[] GetConfigData()
    {
        byte[] copy = new byte[CONFIG_SIZE];
        Array.Copy(_configData, copy, CONFIG_SIZE);
        return copy;
    }

    /// <summary>
    /// 获取错误日志列表（用于测试验证）
    /// </summary>
    public IReadOnlyList<FaultLogEntry> GetFaultLogs() => _faultLogs.AsReadOnly();

    #endregion
}

/// <summary>
/// 错误日志条目
/// </summary>
public class FaultLogEntry
{
    public string Level { get; set; } = "INFO";
    public string Module { get; set; } = string.Empty;
    public string Message { get; set; } = string.Empty;
    public uint Timestamp { get; set; }
}
