namespace HidConfigTool.Core.Models;

/// <summary>
/// 协议常量统一管理类（单一权威来源）
///
/// 所有版本号、魔数、协议常量在此定义一次，其他文件直接引用。
/// 修改常量时只需修改此文件，避免多处重复定义导致不一致。
///
/// 与固件端 version.h 保持同步，修改时需同时更新两端。
/// </summary>
public static class ProtocolConstants
{
    // ==================== 固件版本 ====================

    /// <summary>
    /// 固件版本号（USB bcdDevice 格式，BCD编码）
    /// 格式：0xMMNN，其中 MM=主版本.次版本，NN=修订版本
    /// 例如：0x0010 = v0.1.0，0x0100 = v1.0.0
    /// </summary>
    public const ushort FirmwareVersionBcd = 0x0010; // v0.1.0

    /// <summary>固件版本主版本号</summary>
    public const int FirmwareVersionMajor = 0;

    /// <summary>固件版本次版本号</summary>
    public const int FirmwareVersionMinor = 1;

    /// <summary>固件版本修订号</summary>
    public const int FirmwareVersionPatch = 0;

    /// <summary>固件版本字符串</summary>
    public const string FirmwareVersionString = "v0.1.0";

    // ==================== 协议版本 ====================

    /// <summary>
    /// HID 配置协议版本
    /// 主版本不同则不兼容，次版本不同则向下兼容
    /// v0.2：能力位扩展至 26（诊断/制造段）、Device Info 增加 device_flags、
    /// 命令三级分级与固件侧闸门、新增错误码 DIAG_LOCKED/MFG_LOCKED
    /// </summary>
    public const byte ProtocolVersionMajor = 0;

    /// <summary>协议次版本号</summary>
    public const byte ProtocolVersionMinor = 2;

    /// <summary>协议版本（合并值：高字节=主版本，低字节=次版本）</summary>
    public const ushort ProtocolVersion = (ProtocolVersionMajor << 8) | ProtocolVersionMinor;

    /// <summary>协议版本字符串</summary>
    public const string ProtocolVersionString = "v0.2";

    // ==================== 配置版本 ====================

    /// <summary>
    /// 配置结构版本号
    /// 配置结构体字段变更时递增，用于配置迁移和兼容性判断
    /// v8：在 crc32 前追加 perf_thresholds[8]（性能监控阈值持久化），总大小 1510
    /// </summary>
    public const ushort ConfigVersion = 0x0008; // v8

    /// <summary>配置魔数（用于识别有效配置，与固件保持一致）</summary>
    public const uint ConfigMagic = 0x5A5A5A5A;

    /// <summary>配置结构总大小（字节，含CRC32），与固件 device_config_t / CONFIG_TOTAL_SIZE 一致</summary>
    public const int ConfigSize = 1510;

    // ==================== CONFIG_EXT2 子块定义（v6/v7配置扩展） ====================

    /// <summary>CONFIG_EXT2 子块数量</summary>
    public const int ConfigExt2SubBlockCount = 5;

    /// <summary>CONFIG_EXT2 子块0：v6高级配置字段偏移（dpi_slot/lod/angle_snapping等）</summary>
    public const int ConfigExt2V6Offset = 1346;

    /// <summary>CONFIG_EXT2 子块0：v6高级配置字段长度（14字节）</summary>
    public const int ConfigExt2V6Length = 14;

    /// <summary>CONFIG_EXT2 子块1：Layer2 按键映射前半部分偏移（键0-31）</summary>
    public const int ConfigExt2Layer2Part1Offset = 1360;

    /// <summary>CONFIG_EXT2 子块2：Layer2 按键映射后半部分偏移（键32-63）</summary>
    public const int ConfigExt2Layer2Part2Offset = 1392;

    /// <summary>CONFIG_EXT2 子块3：Layer3 按键映射前半部分偏移（键0-31）</summary>
    public const int ConfigExt2Layer3Part1Offset = 1424;

    /// <summary>CONFIG_EXT2 子块4：Layer3 按键映射后半部分偏移（键32-63）</summary>
    public const int ConfigExt2Layer3Part2Offset = 1456;

    /// <summary>CONFIG_EXT2 按键映射子块每部分长度（32字节，对应32个键）</summary>
    public const int ConfigExt2KeymapPartLength = 32;

    // ==================== 协议常量 ====================

    /// <summary>直连设备索引（有线版本固定为 0xFF）</summary>
    public const byte DeviceIndexDirect = 0xFF;

    /// <summary>广播设备索引</summary>
    public const byte DeviceIndexBroadcast = 0x00;

    /// <summary>软件ID广播（不需要响应匹配）</summary>
    public const byte SwIdBroadcast = 0x00;

    /// <summary>软件ID最小值</summary>
    public const byte SwIdMin = 0x01;

    /// <summary>软件ID最大值（15）</summary>
    public const byte SwIdMax = 0x0F;

    // ==================== Report ID ====================

    /// <summary>配置块0 Report ID（偏移 0-61）</summary>
    public const byte ReportIdConfigBlock0 = 0x05;

    /// <summary>设备信息 Report ID</summary>
    public const byte ReportIdDeviceInfo = 0x06;

    /// <summary>控制命令 Report ID（Output Report，发送命令）</summary>
    public const byte ReportIdControl = 0x07;

    /// <summary>配置块1 Report ID（偏移 62-123）</summary>
    public const byte ReportIdConfigBlock1 = 0x08;

    /// <summary>配置块2 Report ID（偏移 124-185）</summary>
    public const byte ReportIdConfigBlock2 = 0x09;

    /// <summary>按键统计块0 Report ID（键 0~15）</summary>
    public const byte ReportIdKeyStats0 = 0x0A;

    /// <summary>按键统计块1 Report ID（键 16~31）</summary>
    public const byte ReportIdKeyStats1 = 0x0B;

    /// <summary>按键统计块2 Report ID（键 32~47）</summary>
    public const byte ReportIdKeyStats2 = 0x0C;

    /// <summary>按键统计块3 Report ID（键 48~63）</summary>
    public const byte ReportIdKeyStats3 = 0x0D;

    /// <summary>宏配置读写 Report ID</summary>
    public const byte ReportIdMacroConfig = 0x0E;

    /// <summary>性能监控-系统状态 Report ID</summary>
    public const byte ReportIdPerfSystem = 0x0F;

    /// <summary>性能监控-任务统计 Report ID</summary>
    public const byte ReportIdPerfTask = 0x10;

    /// <summary>错误日志-信息 Report ID</summary>
    public const byte ReportIdFaultInfo = 0x11;

    /// <summary>错误日志-读取日志 Report ID</summary>
    public const byte ReportIdFaultLog = 0x12;

    /// <summary>实时按键状态 Report ID（64位bitmap）</summary>
    public const byte ReportIdKeyState = 0x13;

    /// <summary>实时摇杆状态 Report ID</summary>
    public const byte ReportIdJoystickState = 0x14;

    /// <summary>配置扩展字段 Report ID（v3/v4/v5配置扩展字段）</summary>
    public const byte ReportIdConfigExt = 0x15;

    /// <summary>NKRO 位图键盘 Report ID（6KRO/NKRO 切换）</summary>
    public const byte ReportIdNkro = 0x16;

    /// <summary>摇杆校准状态查询 Report ID</summary>
    public const byte ReportIdJoystickCalib = 0x17;

    /// <summary>ACK/NACK响应 Report ID（Input报告，固件主动发送）</summary>
    public const byte ReportIdAck = 0x18;

    /// <summary>配置扩展块2 Report ID（v6/v7配置扩展块2）</summary>
    public const byte ReportIdConfigExt2 = 0x19;

    // ==================== DFU Report ID ====================

    /// <summary>DFU 状态查询 Report ID</summary>
    public const byte ReportIdDfuStatus = 0x30;

    /// <summary>DFU 擦除 Report ID</summary>
    public const byte ReportIdDfuErase = 0x31;

    /// <summary>DFU 数据传输 Report ID</summary>
    public const byte ReportIdDfuData = 0x32;

    /// <summary>DFU 校验 Report ID</summary>
    public const byte ReportIdDfuVerify = 0x33;

    /// <summary>DFU 完成 Report ID</summary>
    public const byte ReportIdDfuFinish = 0x34;

    /// <summary>DFU 中止 Report ID</summary>
    public const byte ReportIdDfuAbort = 0x35;

    /// <summary>DFU 启动应用程序 Report ID</summary>
    public const byte ReportIdDfuBootApp = 0x36;

    // ==================== 命令 ID ====================

    /// <summary>保存配置到 Flash</summary>
    public const byte CmdSaveConfig = 0x01;

    /// <summary>恢复出厂默认配置</summary>
    public const byte CmdResetConfig = 0x02;

    /// <summary>重启设备（需3次确认）</summary>
    public const byte CmdReboot = 0x03;

    /// <summary>进入 DFU 模式（需3次确认）</summary>
    public const byte CmdEnterDfu = 0x04;

    /// <summary>应用临时配置（写入 Flash 并生效）</summary>
    public const byte CmdApplyConfig = 0x05;

    /// <summary>清零按键统计</summary>
    public const byte CmdResetStats = 0x06;

    /// <summary>清除错误日志</summary>
    public const byte CmdClearFault = 0x07;

    /// <summary>重置性能统计</summary>
    public const byte CmdResetPerf = 0x08;

    /// <summary>播放宏（参数：宏ID）</summary>
    public const byte CmdMacroPlay = 0x09;

    /// <summary>停止宏（参数：宏ID，0xFF=停止所有）</summary>
    public const byte CmdMacroStop = 0x0A;

    /// <summary>设置性能监控开关（参数：1=开启，0=关闭）</summary>
    public const byte CmdSetPerfEnable = 0x0B;

    /// <summary>实时设置摇杆死区（参数：2字节小端，不写Flash）</summary>
    public const byte CmdSetJoystickDzRt = 0x0C;

    /// <summary>解锁配置写入（需连续3次，5秒内）</summary>
    public const byte CmdUnlockConfig = 0x0D;

    /// <summary>触发摇杆中心点校准（参数：采样次数）</summary>
    public const byte CmdJoystickCalibrate = 0x0E;

    /// <summary>循环切换 DPI 档位（参数：0xFF=循环，0-3=指定档位）</summary>
    public const byte CmdSwitchDpi = 0x0F;

    /// <summary>获取版本信息（返回固件/配置/协议版本）</summary>
    public const byte CmdGetVersion = 0x10;

    /// <summary>版本协商（参数：上位机协议版本，返回固件支持的版本）</summary>
    public const byte CmdNegotiateVersion = 0x11;

    /// <summary>切换按键层（参数：0-3 层号）</summary>
    public const byte CmdSwitchLayer = 0x12;

    /// <summary>获取能力信息（参数：能力ID索引，ACK返回能力版本）</summary>
    public const byte CmdGetCapabilityInfo = 0x13;

    /// <summary>进入官方 BOOTSEL UF2 模式（软件触发，需3次确认）</summary>
    public const byte CmdEnterBootsel = 0x14;

    /// <summary>
    /// 设置性能监控任务超时阈值
    /// 参数格式（16位小端）：
    ///   高字节（bit 8-15）：任务索引（0-15，对应 perf_monitor 中的任务）
    ///   低字节（bit 0-7）：阈值（0-255，单位100微秒，0=禁用告警）
    ///   最大阈值：255 × 100us = 25500us = 25.5ms
    /// 响应参数：高字节=任务索引，低字节=实际设置的阈值/100
    /// </summary>
    public const byte CmdSetPerfThreshold = 0x15;
}
