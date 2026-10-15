namespace HidConfigTool.Core.Utils;
using HidConfigTool.Core.Models;

/// <summary>
/// 能力ID（Capability IDs）
/// 与固件 config_hid.h 中的 capability_id_t 一一对应
/// 每个能力独立版本化，上位机根据能力位图判断功能兼容性
/// </summary>
public enum CapabilityId
{
    /// <summary>键盘基础：64键矩阵、标准键盘报告</summary>
    KeyboardBasic = 0,
    /// <summary>鼠标基础：PAW3395、标准鼠标报告</summary>
    MouseBasic = 1,
    /// <summary>摇杆：ADC摇杆、死区/灵敏度</summary>
    Joystick = 2,
    /// <summary>编码器：旋转编码器、滚轮</summary>
    Encoder = 3,
    /// <summary>按键映射：4层按键映射、Fn键</summary>
    Keymap = 4,
    /// <summary>宏引擎：8个宏、32个动作</summary>
    Macro = 5,
    /// <summary>配置存储：1510字节(v8)、CRC32、双备份</summary>
    ConfigStorage = 6,
    /// <summary>性能监控：CPU使用率、任务统计</summary>
    PerfMonitor = 7,
    /// <summary>错误日志：Flash持久化错误日志</summary>
    FaultLog = 8,
    /// <summary>按键统计：64键使用次数</summary>
    KeyStats = 9,
    /// <summary>DFU升级：USB DFU固件升级</summary>
    DfuUpgrade = 10,
    /// <summary>NKRO：NKRO位图键盘</summary>
    Nkro = 11,
    /// <summary>多媒体键：音量、播放控制</summary>
    Consumer = 12,
    /// <summary>游戏手柄：标准HID Gamepad</summary>
    Gamepad = 13,
    /// <summary>DPI档位：4档DPI切换、LOD</summary>
    DpiSlots = 14,
    /// <summary>摇杆校准：中心点校准、实时调节</summary>
    JoystickCalib = 15,
    /// <summary>实时 raw 诊断报告（KeyState/JoystickState），对应固件 CAP_DIAG_RAW_STATE</summary>
    DiagRawState = 16,
    /// <summary>调试串口 / 详细日志，对应固件 CAP_DIAG_DEBUG_UART</summary>
    DiagDebugUart = 17,
    // 18-23 预留
    /// <summary>工厂自检（FEATURE_FACTORY_TEST 且 manufacturing flag 未出厂锁定）</summary>
    MfgFactoryTest = 24,
    /// <summary>制造数据烧录 / 出厂锁定（序列号、校准、ship lock）</summary>
    MfgProvision = 25,
    // 26-31 预留
    /// <summary>能力总数（含 18-23 预留空洞）</summary>
    Count = 26
}

/// <summary>
/// 能力支持结果
/// </summary>
public enum CapabilitySupport
{
    /// <summary>完全支持：上位机和固件版本兼容</summary>
    Full,
    /// <summary>固件太旧：固件有此能力但版本过低</summary>
    DeviceTooOld,
    /// <summary>上位机太旧：固件有此能力但上位机不支持该版本</summary>
    HostTooOld,
    /// <summary>上位机不支持：固件有此能力但上位机不认识</summary>
    UnsupportedByHost,
    /// <summary>固件不支持：固件没有此能力</summary>
    UnsupportedByDevice
}

/// <summary>
/// 能力信息
/// </summary>
public class CapabilityInfo
{
    /// <summary>能力ID</summary>
    public CapabilityId Id { get; set; }
    /// <summary>能力名称</summary>
    public string Name { get; set; } = string.Empty;
    /// <summary>固件支持的能力版本</summary>
    public Version DeviceVersion { get; set; } = new(0, 0);
    /// <summary>上位机支持的最低版本</summary>
    public Version HostMinVersion { get; set; } = new(0, 0);
    /// <summary>上位机支持的最高版本</summary>
    public Version HostMaxVersion { get; set; } = new(0, 0);
    /// <summary>支持结果</summary>
    public CapabilitySupport Support { get; set; } = CapabilitySupport.UnsupportedByDevice;
    /// <summary>是否可用（Full 或 DeviceTooOld 时可用旧版模式）</summary>
    public bool IsAvailable => Support == CapabilitySupport.Full || Support == CapabilitySupport.DeviceTooOld;

    public override string ToString() => $"{Name}: {Support} (固件v{DeviceVersion}, 上位机v{HostMinVersion}-{HostMaxVersion})";
}

/// <summary>
/// 版本信息
/// </summary>
public class DeviceVersionInfo
{
    /// <summary>固件版本（如 4.0.0）</summary>
    public Version FirmwareVersion { get; set; } = new(0, 0, 0);
    /// <summary>配置结构版本（如 7）</summary>
    public int ConfigVersion { get; set; }
    /// <summary>协议版本（如 2.0）</summary>
    public Version ProtocolVersion { get; set; } = new(0, 0);
    /// <summary>配置结构大小（字节）</summary>
    public int ConfigSize { get; set; }
    /// <summary>支持的按键层数</summary>
    public int LayerCount { get; set; }
    /// <summary>支持的宏数量</summary>
    public int MacroCount { get; set; }
    /// <summary>最大 DPI 档位索引</summary>
    public int MaxDpi { get; set; }
    /// <summary>支持的能力数量</summary>
    public int CapabilityCount { get; set; }
    /// <summary>能力位图（64位，每位代表一个能力是否存在）</summary>
    public ulong CapabilityBitmap { get; set; }
    /// <summary>设备标志位（v0.2 DEVICE_FLAG_*：bit0 已出厂锁定，bit1 本会话售后已解锁）；旧固件短报告为 0</summary>
    public byte DeviceFlags { get; set; }
    /// <summary>解析后的能力列表</summary>
    public Dictionary<CapabilityId, CapabilityInfo> Capabilities { get; set; } = new();
    /// <summary>是否为有效版本信息</summary>
    public bool IsValid => FirmwareVersion.Major > 0 || ProtocolVersion.Major > 0;

    /// <summary>检查指定能力是否可用</summary>
    public bool IsCapabilityAvailable(CapabilityId id) =>
        Capabilities.TryGetValue(id, out var cap) && cap.IsAvailable;

    public override string ToString()
    {
        int available = Capabilities.Values.Count(c => c.IsAvailable);
        return $"固件v{FirmwareVersion}, 协议v{ProtocolVersion}, 配置v{ConfigVersion}, 能力{available}/{CapabilityCount}";
    }
}

/// <summary>
/// 兼容性检查结果
/// </summary>
public enum CompatibilityResult
{
    /// <summary>完全兼容</summary>
    Compatible,
    /// <summary>协议兼容，但固件版本过旧（建议升级）</summary>
    FirmwareOutdated,
    /// <summary>协议不兼容（主版本不同，必须升级）</summary>
    ProtocolIncompatible,
    /// <summary>传输层不兼容（连基础通信都无法保证）</summary>
    TransportIncompatible,
    /// <summary>无法读取版本信息</summary>
    Unknown
}

/// <summary>
/// 版本比较工具类
/// 提供固件版本、协议版本的比较和兼容性检查，以及能力协商（Capability Negotiation）
/// 
/// 设计原则：
/// 1. 传输层协议版本必须匹配（主版本号）
/// 2. 应用功能按能力ID + 独立版本判断
/// 3. 全局固件版本只用于日志和升级推荐
/// 4. 上位机根据能力结果动态启用/禁用功能
/// </summary>
public static class VersionComparer
{
    /// <summary>上位机支持的最低固件版本</summary>
    public static readonly Version MinimumSupportedFirmwareVersion = new(3, 0, 0);
    /// <summary>上位机推荐的最低固件版本（统一从 ProtocolConstants 引用）</summary>
    public static readonly Version RecommendedFirmwareVersion = new(ProtocolConstants.FirmwareVersionMajor, ProtocolConstants.FirmwareVersionMinor, ProtocolConstants.FirmwareVersionPatch);
    /// <summary>上位机支持的协议版本（主版本必须匹配，统一从 ProtocolConstants 引用）
    /// v0.1: 初始版本，包含 Device Index、Software ID、标准化错误码</summary>
    public static readonly Version SupportedProtocolVersion = new(ProtocolConstants.ProtocolVersionMajor, ProtocolConstants.ProtocolVersionMinor);

    /// <summary>
    /// 上位机支持的能力注册表
    /// Key: 能力ID
    /// Value: (名称, 上位机支持的最低版本, 上位机支持的最高版本)
    /// 
    /// 这是能力协商的核心：上位机声明自己支持哪些能力、每个能力的版本范围。
    /// 固件报告能力位图后，上位机根据此表判断每个能力是否可用。
    /// </summary>
    private static readonly Dictionary<CapabilityId, (string Name, Version Min, Version Max)> HostCapabilityRegistry = new()
    {
        { CapabilityId.KeyboardBasic,    ("键盘基础",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.MouseBasic,       ("鼠标基础",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.Joystick,         ("摇杆",        new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.Encoder,          ("编码器",      new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.Keymap,           ("按键映射",    new Version(2, 0), new Version(2, 0)) },
        { CapabilityId.Macro,            ("宏引擎",      new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.ConfigStorage,    ("配置存储",    new Version(2, 0), new Version(2, 0)) },
        { CapabilityId.PerfMonitor,      ("性能监控",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.FaultLog,         ("错误日志",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.KeyStats,         ("按键统计",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.DfuUpgrade,       ("DFU升级",     new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.Nkro,             ("NKRO",        new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.Consumer,         ("多媒体键",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.Gamepad,          ("游戏手柄",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.DpiSlots,         ("DPI档位",     new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.JoystickCalib,    ("摇杆校准",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.DiagRawState,     ("实时诊断",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.DiagDebugUart,    ("调试串口",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.MfgFactoryTest,   ("工厂自检",    new Version(1, 0), new Version(1, 0)) },
        { CapabilityId.MfgProvision,     ("制造烧录",    new Version(1, 0), new Version(1, 0)) },
    };

    /// <summary>
    /// 能力依赖关系表（Capability Dependencies）
    /// Key: 能力ID
    /// Value: 该能力依赖的其他能力ID列表
    /// 
    /// 能力协商的进阶：不仅判断单个能力是否可用，还要验证其依赖的能力是否可用。
    /// 例如：宏引擎依赖配置存储，如果配置存储不可用，宏引擎也不能用。
    /// </summary>
    private static readonly Dictionary<CapabilityId, CapabilityId[]> CapabilityDependencies = new()
    {
        { CapabilityId.KeyboardBasic,    Array.Empty<CapabilityId>() },
        { CapabilityId.MouseBasic,       Array.Empty<CapabilityId>() },
        { CapabilityId.Joystick,         Array.Empty<CapabilityId>() },
        { CapabilityId.Encoder,          Array.Empty<CapabilityId>() },
        { CapabilityId.Keymap,           new[] { CapabilityId.ConfigStorage } },        /* 按键映射依赖配置存储 */
        { CapabilityId.Macro,            new[] { CapabilityId.ConfigStorage } },        /* 宏引擎依赖配置存储 */
        { CapabilityId.ConfigStorage,    Array.Empty<CapabilityId>() },
        { CapabilityId.PerfMonitor,      Array.Empty<CapabilityId>() },
        { CapabilityId.FaultLog,         Array.Empty<CapabilityId>() },
        { CapabilityId.KeyStats,         Array.Empty<CapabilityId>() },
        { CapabilityId.DfuUpgrade,       Array.Empty<CapabilityId>() },
        { CapabilityId.Nkro,             new[] { CapabilityId.KeyboardBasic } },        /* NKRO依赖键盘基础 */
        { CapabilityId.Consumer,         new[] { CapabilityId.KeyboardBasic } },        /* 多媒体键依赖键盘基础 */
        { CapabilityId.Gamepad,          Array.Empty<CapabilityId>() },
        { CapabilityId.DpiSlots,         new[] { CapabilityId.MouseBasic } },           /* DPI档位依赖鼠标基础 */
        { CapabilityId.JoystickCalib,    new[] { CapabilityId.Joystick } },             /* 摇杆校准依赖摇杆 */
        { CapabilityId.DiagRawState,     Array.Empty<CapabilityId>() },
        { CapabilityId.DiagDebugUart,    Array.Empty<CapabilityId>() },
        { CapabilityId.MfgFactoryTest,   Array.Empty<CapabilityId>() },
        { CapabilityId.MfgProvision,     Array.Empty<CapabilityId>() },
    };

    /// <summary>
    /// 验证能力依赖关系
    /// 检查指定能力的所有依赖是否都可用，如果有依赖不可用，则该能力也不可用。
    /// </summary>
    /// <param name="capabilityId">要验证的能力ID</param>
    /// <param name="capabilities">当前能力评估结果</param>
    /// <returns>验证后的能力支持结果（如果依赖不可用则返回 UnsupportedByDevice）</returns>
    public static CapabilitySupport ValidateCapabilityDependencies(CapabilityId capabilityId, Dictionary<CapabilityId, CapabilityInfo> capabilities)
    {
        if (!capabilities.TryGetValue(capabilityId, out var cap))
            return CapabilitySupport.UnsupportedByDevice;

        // 如果能力本身不可用，直接返回
        if (!cap.IsAvailable)
            return cap.Support;

        // 检查所有依赖
        if (CapabilityDependencies.TryGetValue(capabilityId, out var deps))
        {
            foreach (var dep in deps)
            {
                if (!capabilities.TryGetValue(dep, out var depCap) || !depCap.IsAvailable)
                {
                    // 依赖不可用，此能力也不可用
                    cap.Support = CapabilitySupport.UnsupportedByDevice;
                    return cap.Support;
                }
            }
        }

        return cap.Support;
    }

    /// <summary>
    /// 验证所有能力的依赖关系
    /// </summary>
    public static void ValidateAllDependencies(Dictionary<CapabilityId, CapabilityInfo> capabilities)
    {
        foreach (var id in capabilities.Keys.ToList())
        {
            ValidateCapabilityDependencies(id, capabilities);
        }
    }

    /// <summary>
    /// 从设备信息报告数据解析版本信息和能力位图
    /// 设备信息报告格式（Report ID 6，v0.2 为 32 字节，含 report_id）：
    /// byte[0] = report_id
    /// byte[1-2] = firmware_version (小端 uint16，如 0x0400 = 4.0.0)
    /// byte[3-4] = config_version (小端 uint16)
    /// byte[5-6] = protocol_version (高字节=主版本，低字节=次版本)
    /// byte[7-8] = config_size (小端 uint16)
    /// byte[9] = layer_count
    /// byte[10] = macro_count
    /// byte[11] = max_dpi
    /// byte[12] = capability_count
    /// byte[13-20] = capability_bitmap (小端 uint64)
    /// byte[21] = device_flags (v0.2 新增，旧固件无此字节，按短报告解析为 0)
    /// byte[22-31] = reserved
    /// </summary>
    /// <param name="data">设备信息报告数据（可能含 Report ID，自动检测）</param>
    /// <returns>解析后的版本信息</returns>
    public static DeviceVersionInfo ParseDeviceInfo(byte[] data)
    {
        var info = new DeviceVersionInfo();

        if (data == null || data.Length < 12)
            return info;

        try
        {
            // 自动检测是否包含 Report ID：
            // REPORT_ID_DEVICE_INFO = 0x06，如果 data[0]==0x06 且长度足够，说明含 Report ID，需偏移1字节
            int offset = 0;
            if (data.Length >= 13 && data[0] == 0x06)
            {
                offset = 1;
            }

            // 固件版本：bcdDevice 格式（如 0x0400 = 4.0.0），小端 uint16
            ushort firmwareBcd = BitConverter.ToUInt16(data, offset + 0);
            int fwMajor = (firmwareBcd >> 8) & 0xFF;
            int fwMinor = firmwareBcd & 0xFF;
            info.FirmwareVersion = new Version(fwMajor, fwMinor, 0);

            // 配置版本：小端 uint16
            info.ConfigVersion = BitConverter.ToUInt16(data, offset + 2);

            // 协议版本：小端 uint16，高字节=主版本，低字节=次版本
            // 例如 0x0200 = 协议 v2.0，小端存储为 [0x00, 0x02]
            ushort protoVer = BitConverter.ToUInt16(data, offset + 4);
            int protoMajor = (protoVer >> 8) & 0xFF;
            int protoMinor = protoVer & 0xFF;
            info.ProtocolVersion = new Version(protoMajor, protoMinor);

            // 配置大小：小端 uint16
            if (data.Length >= offset + 8)
                info.ConfigSize = BitConverter.ToUInt16(data, offset + 6);

            // 层数、宏数、最大 DPI
            if (data.Length >= offset + 12)
            {
                info.LayerCount = data[offset + 8];
                info.MacroCount = data[offset + 9];
                info.MaxDpi = data[offset + 10];
            }

            // 能力数量和能力位图（新增字段，旧固件可能没有）
            // 含 Report ID 时：capability_count 在 byte[12]，capability_bitmap 在 byte[13-20]
            // 不含 Report ID 时：capability_count 在 data[11]，capability_bitmap 在 data[12-19]
            if (data.Length >= offset + 20)
            {
                info.CapabilityCount = data[offset + 11];
                info.CapabilityBitmap = BitConverter.ToUInt64(data, offset + 12);
            }
            else
            {
                // 旧固件没有能力位图，根据协议版本推断默认能力
                info.CapabilityCount = 0;
                info.CapabilityBitmap = 0;
            }

            // v0.2 新增 device_flags（含 Report ID 时在 byte[21]，即 offset+20）；
            // 旧固件为短报告（无此字节），保持默认 0
            if (data.Length >= offset + 21)
            {
                info.DeviceFlags = data[offset + 20];
            }

            // 解析能力列表
            info.Capabilities = EvaluateCapabilities(info.CapabilityBitmap, info.ProtocolVersion);
        }
        catch
        {
            // 解析失败，返回默认值
        }

        return info;
    }

    /// <summary>
    /// 根据能力位图和上位机注册表，评估每个能力的支持结果
    /// </summary>
    /// <param name="deviceBitmap">固件能力位图</param>
    /// <param name="deviceProtocolVersion">固件协议版本（用于旧固件推断默认能力）</param>
    /// <returns>能力评估结果字典</returns>
    public static Dictionary<CapabilityId, CapabilityInfo> EvaluateCapabilities(ulong deviceBitmap, Version deviceProtocolVersion)
    {
        var result = new Dictionary<CapabilityId, CapabilityInfo>();

        // 如果固件没有能力位图（旧固件），根据协议版本推断默认能力
        ulong effectiveBitmap = deviceBitmap;
        if (deviceBitmap == 0 && deviceProtocolVersion.Major >= 2)
        {
            // 协议v2的旧固件默认支持消费者基础能力（bit0-15）；
            // 诊断位(16/17)与制造位(24/25)不得凭版本号推断默认开启，必须以位图为准。
            // CAP_DFU_UPGRADE(bit10) 依赖自定义 bootloader 实际编入，同样不得凭协议版本
            // 推断：未通过位图明确上报即视为不支持，上位机仅保留 BOOTSEL UF2 手动升级。
            effectiveBitmap = 0xFFFFUL & ~(1UL << (int)CapabilityId.DfuUpgrade);
        }

        foreach (var (id, (name, hostMin, hostMax)) in HostCapabilityRegistry)
        {
            var cap = new CapabilityInfo
            {
                Id = id,
                Name = name,
                HostMinVersion = hostMin,
                HostMaxVersion = hostMax,
            };

            bool deviceHas = (effectiveBitmap & (1UL << (int)id)) != 0;

            if (!deviceHas)
            {
                cap.Support = CapabilitySupport.UnsupportedByDevice;
                cap.DeviceVersion = new Version(0, 0);
            }
            else
            {
                // 固件有此能力，假设固件能力版本为1.0（当前能力位图只标记有无，不标记版本）
                // 未来可扩展能力描述符报告获取精确版本
                cap.DeviceVersion = new Version(1, 0);

                if (cap.DeviceVersion < hostMin)
                    cap.Support = CapabilitySupport.DeviceTooOld;
                else if (cap.DeviceVersion > hostMax)
                    cap.Support = CapabilitySupport.HostTooOld;
                else
                    cap.Support = CapabilitySupport.Full;
            }

            result[id] = cap;
        }

        // 验证能力依赖关系（如宏引擎依赖配置存储）
        ValidateAllDependencies(result);

        return result;
    }

    /// <summary>
    /// 检查设备版本兼容性
    /// 分层检查：
    /// 1. 传输层：协议主版本必须匹配
    /// 2. 固件版本：检查最低支持版本和推荐版本
    /// 3. 能力层：由 EvaluateCapabilities 单独判断，不影响整体兼容性
    /// </summary>
    /// <param name="deviceInfo">设备版本信息</param>
    /// <returns>兼容性检查结果</returns>
    public static CompatibilityResult CheckCompatibility(DeviceVersionInfo deviceInfo)
    {
        if (!deviceInfo.IsValid)
            return CompatibilityResult.Unknown;

        // 1. 传输层检查：协议主版本必须匹配
        if (deviceInfo.ProtocolVersion.Major != SupportedProtocolVersion.Major)
            return CompatibilityResult.TransportIncompatible;

        // 2. 固件版本检查：低于最低支持版本则不兼容
        if (deviceInfo.FirmwareVersion < MinimumSupportedFirmwareVersion)
            return CompatibilityResult.ProtocolIncompatible;

        // 3. 固件版本检查：低于推荐版本则警告但可用
        if (deviceInfo.FirmwareVersion < RecommendedFirmwareVersion)
            return CompatibilityResult.FirmwareOutdated;

        // 能力层检查不影响整体兼容性，由各功能模块单独判断
        return CompatibilityResult.Compatible;
    }

    /// <summary>
    /// 获取兼容性检查的用户可读提示
    /// </summary>
    public static string GetCompatibilityMessage(CompatibilityResult result, DeviceVersionInfo? deviceInfo = null)
    {
        return result switch
        {
            CompatibilityResult.Compatible =>
                "设备版本兼容，可以正常使用",

            CompatibilityResult.FirmwareOutdated =>
                $"设备固件版本过旧（{deviceInfo?.FirmwareVersion ?? new Version(0, 0, 0)}），" +
                $"建议升级到 v{RecommendedFirmwareVersion} 或更高版本以获得完整功能",

            CompatibilityResult.ProtocolIncompatible =>
                $"设备固件版本过低（{deviceInfo?.FirmwareVersion ?? new Version(0, 0, 0)}），" +
                $"最低支持 v{MinimumSupportedFirmwareVersion}，请升级固件",

            CompatibilityResult.TransportIncompatible =>
                $"设备协议版本不兼容（设备 v{deviceInfo?.ProtocolVersion ?? new Version(0, 0)}，" +
                $"上位机支持 v{SupportedProtocolVersion.Major}.x），" +
                $"请升级固件或使用兼容版本的上位机",

            CompatibilityResult.Unknown =>
                "无法读取设备版本信息，请检查设备连接",

            _ => "未知版本状态"
        };
    }

    /// <summary>
    /// 获取能力摘要信息（用于日志和UI显示）
    /// </summary>
    public static string GetCapabilitySummary(DeviceVersionInfo deviceInfo)
    {
        if (deviceInfo.Capabilities.Count == 0)
            return "设备未报告能力信息（旧固件）";

        int full = deviceInfo.Capabilities.Values.Count(c => c.Support == CapabilitySupport.Full);
        int deviceOld = deviceInfo.Capabilities.Values.Count(c => c.Support == CapabilitySupport.DeviceTooOld);
        int hostOld = deviceInfo.Capabilities.Values.Count(c => c.Support == CapabilitySupport.HostTooOld);
        int unsupported = deviceInfo.Capabilities.Values.Count(c => c.Support == CapabilitySupport.UnsupportedByDevice);

        return $"能力: 完全支持{full}, 固件过旧{deviceOld}, 上位机过旧{hostOld}, 不支持{unsupported}";
    }

    /// <summary>
    /// 判断是否允许配置读写操作
    /// </summary>
    public static bool IsConfigAccessAllowed(CompatibilityResult result)
    {
        return result switch
        {
            CompatibilityResult.Compatible => true,
            CompatibilityResult.FirmwareOutdated => true,  // 协议兼容，允许读写但提示升级
            CompatibilityResult.ProtocolIncompatible => false,
            CompatibilityResult.TransportIncompatible => false,
            CompatibilityResult.Unknown => false,
            _ => false
        };
    }

    /// <summary>比较两个版本号</summary>
    public static int CompareVersions(Version v1, Version v2) => v1.CompareTo(v2);

    /// <summary>解析版本字符串</summary>
    public static Version? ParseVersionString(string versionString)
    {
        if (string.IsNullOrWhiteSpace(versionString))
            return null;
        try
        {
            string cleaned = versionString.TrimStart('v', 'V');
            return Version.Parse(cleaned);
        }
        catch
        {
            return null;
        }
    }
}
