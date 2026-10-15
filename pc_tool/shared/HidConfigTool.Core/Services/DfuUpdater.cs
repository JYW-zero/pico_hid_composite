using System.IO;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Core.Services;

/// <summary>
/// DFU 固件升级器
/// 处理与 Bootloader 的通信协议
/// </summary>
public class DfuUpdater
{
    private readonly IHidDriver _hidDriver;
    private readonly Action<string> _log;

    // DFU Report ID（统一从 ProtocolConstants 引用，避免重复定义）
    private const byte REPORT_ID_DFU_STATUS = ProtocolConstants.ReportIdDfuStatus;
    private const byte REPORT_ID_DFU_ERASE = ProtocolConstants.ReportIdDfuErase;
    private const byte REPORT_ID_DFU_DATA = ProtocolConstants.ReportIdDfuData;
    private const byte REPORT_ID_DFU_VERIFY = ProtocolConstants.ReportIdDfuVerify;
    private const byte REPORT_ID_DFU_FINISH = ProtocolConstants.ReportIdDfuFinish;
    private const byte REPORT_ID_DFU_ABORT = ProtocolConstants.ReportIdDfuAbort;
    private const byte REPORT_ID_DFU_BOOT_APP = ProtocolConstants.ReportIdDfuBootApp;  // 启动应用程序

    // 每次写入的数据大小（58字节 + 4字节偏移 = 62字节报告）
    private const int DFU_DATA_CHUNK = 58;

    // Bootloader VID/PID（与应用程序使用相同 VID，不同 PID）
    public const int BOOTLOADER_VID = 0x1209;  // pid.codes VID
    public const int BOOTLOADER_PID = 0x4005;

    public DfuUpdater(IHidDriver hidDriver, Action<string> log)
    {
        _hidDriver = hidDriver;
        _log = log;
    }

    /// <summary>
    /// 获取 DFU 状态
    /// </summary>
    public async Task<DfuStatus?> GetStatusAsync(CancellationToken ct = default)
    {
        var data = await _hidDriver.GetFeatureReportAsync(REPORT_ID_DFU_STATUS, ct);
        if (data == null || data.Length < 28) return null;

        return new DfuStatus
        {
            State = data[0],
            Error = data[1],
            WriteOffset = BitConverter.ToUInt32(data, 4),
            ExpectedSize = BitConverter.ToUInt32(data, 8),
            ReceivedCrc = BitConverter.ToUInt32(data, 12),
            VersionMajor = data[16],
            VersionMinor = data[17],
            AppStart = BitConverter.ToUInt32(data, 20),
            AppMaxSize = BitConverter.ToUInt32(data, 24)
        };
    }

    /// <summary>
    /// 擦除应用程序区
    /// </summary>
    public async Task<bool> EraseAsync(uint firmwareSize, CancellationToken ct = default)
    {
        _log($"[DFU] 擦除应用区，大小: {firmwareSize} 字节");
        var data = new byte[4];
        BitConverter.GetBytes(firmwareSize).CopyTo(data, 0);
        return await _hidDriver.SendFeatureReportAsync(REPORT_ID_DFU_ERASE, data, ct);
    }

    /// <summary>
    /// 写入数据块
    /// </summary>
    public async Task<bool> WriteDataAsync(uint offset, byte[] data, CancellationToken ct = default)
    {
        var buffer = new byte[4 + data.Length];
        BitConverter.GetBytes(offset).CopyTo(buffer, 0);
        Array.Copy(data, 0, buffer, 4, data.Length);
        return await _hidDriver.SendFeatureReportAsync(REPORT_ID_DFU_DATA, buffer, ct);
    }

    /// <summary>
    /// 校验固件 CRC
    /// </summary>
    public async Task<bool> VerifyAsync(uint size, uint crc32, CancellationToken ct = default)
    {
        _log($"[DFU] 校验固件: size={size}, crc=0x{crc32:X8}");
        var data = new byte[8];
        BitConverter.GetBytes(size).CopyTo(data, 0);
        BitConverter.GetBytes(crc32).CopyTo(data, 4);
        return await _hidDriver.SendFeatureReportAsync(REPORT_ID_DFU_VERIFY, data, ct);
    }

    /// <summary>
    /// 完成升级，设备自动重启
    /// </summary>
    public async Task<bool> FinishAsync(uint version, CancellationToken ct = default)
    {
        _log($"[DFU] 完成升级，版本: 0x{version:X4}");
        var data = new byte[4];
        BitConverter.GetBytes(version).CopyTo(data, 0);
        return await _hidDriver.SendFeatureReportAsync(REPORT_ID_DFU_FINISH, data, ct);
    }

    /// <summary>
    /// 中止升级
    /// </summary>
    public async Task<bool> AbortAsync(CancellationToken ct = default)
    {
        return await _hidDriver.SendFeatureReportAsync(REPORT_ID_DFU_ABORT, new byte[1], ct);
    }

    /// <summary>
    /// 启动应用程序（从 Bootloader 模式跳转到应用程序）
    /// 此函数不会返回，设备会直接跳转到应用程序
    /// </summary>
    public async Task<bool> BootToApplicationAsync(CancellationToken ct = default)
    {
        _log("[DFU] 启动应用程序...");
        return await _hidDriver.SendFeatureReportAsync(REPORT_ID_DFU_BOOT_APP, new byte[1], ct);
    }

    /// <summary>
    /// 执行完整升级流程
    /// </summary>
    public async Task<bool> UpgradeAsync(byte[] firmwareData, uint version, IProgress<int>? progress, CancellationToken ct)
    {
        try
        {
            // 1. 获取状态
            var status = await GetStatusAsync(ct);
            if (status == null)
            {
                _log("[DFU] 错误: 无法获取 Bootloader 状态");
                return false;
            }
            _log($"[DFU] Bootloader v{status.VersionMajor}.{status.VersionMinor}, 应用区最大: {status.AppMaxSize} 字节");

            if (firmwareData.Length > status.AppMaxSize)
            {
                _log($"[DFU] 错误: 固件大小 {firmwareData.Length} 超过应用区最大 {status.AppMaxSize}");
                return false;
            }

            // 2. 擦除
            if (!await EraseAsync((uint)firmwareData.Length, ct))
            {
                _log("[DFU] 错误: 擦除失败");
                return false;
            }
            await Task.Delay(100, ct);

            // 3. 计算 CRC32
            uint crc = Crc32.Calculate(firmwareData);
            _log($"[DFU] 固件 CRC32: 0x{crc:X8}");

            // 4. 分块写入
            int totalChunks = (firmwareData.Length + DFU_DATA_CHUNK - 1) / DFU_DATA_CHUNK;
            _log($"[DFU] 开始写入，共 {totalChunks} 块");

            for (int i = 0; i < totalChunks; i++)
            {
                ct.ThrowIfCancellationRequested();

                int offset = i * DFU_DATA_CHUNK;
                int chunkSize = Math.Min(DFU_DATA_CHUNK, firmwareData.Length - offset);
                var chunk = new byte[chunkSize];
                Array.Copy(firmwareData, offset, chunk, 0, chunkSize);

                if (!await WriteDataAsync((uint)offset, chunk, ct))
                {
                    _log($"[DFU] 错误: 写入块 {i + 1}/{totalChunks} 失败");
                    await AbortAsync(ct);
                    return false;
                }

                progress?.Report((i + 1) * 80 / totalChunks); // 写入占 80%
            }

            _log("[DFU] 写入完成，等待 Flash 写入完成...");
            await Task.Delay(200, ct);

            // 5. 校验
            progress?.Report(85);
            if (!await VerifyAsync((uint)firmwareData.Length, crc, ct))
            {
                _log("[DFU] 错误: 校验命令发送失败");
                return false;
            }
            await Task.Delay(100, ct);

            // 验证状态
            var verifyStatus = await GetStatusAsync(ct);
            if (verifyStatus != null && verifyStatus.Error != 0)
            {
                _log($"[DFU] 错误: 校验失败，错误码: {verifyStatus.Error}");
                return false;
            }

            // 6. 完成
            progress?.Report(95);
            if (!await FinishAsync(version, ct))
            {
                _log("[DFU] 错误: 完成命令发送失败");
                return false;
            }

            progress?.Report(100);
            _log("[DFU] 升级完成，设备正在重启...");
            return true;
        }
        catch (OperationCanceledException)
        {
            _log("[DFU] 升级已取消");
            await AbortAsync(default);
            return false;
        }
        catch (Exception ex)
        {
            _log($"[DFU] 异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 从 UF2 文件提取固件数据（BIN 格式）
    /// </summary>
    public static byte[] ExtractFirmwareFromUf2(string filePath)
    {
        using var fs = File.OpenRead(filePath);
        using var ms = new MemoryStream();
        byte[] block = new byte[512];

        uint? baseAddr = null;
        uint maxAddr = 0;

        // 第一遍：确定地址范围
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
        }

        if (baseAddr == null)
            throw new InvalidDataException("无效的 UF2 文件");

        // 第二遍：提取数据
        byte[] firmware = new byte[maxAddr - baseAddr.Value];
        fs.Position = 0;

        while (fs.Read(block, 0, 512) == 512)
        {
            uint magic0 = BitConverter.ToUInt32(block, 0);
            if (magic0 != 0x0A324655) continue;

            uint targetAddr = BitConverter.ToUInt32(block, 12);
            uint payloadSize = BitConverter.ToUInt32(block, 16);
            int offset = (int)(targetAddr - baseAddr.Value);

            Array.Copy(block, 32, firmware, offset, (int)payloadSize);
        }

        return firmware;
    }
}

/// <summary>
/// DFU 状态
/// </summary>
public class DfuStatus
{
    public byte State { get; set; }
    public byte Error { get; set; }
    public uint WriteOffset { get; set; }
    public uint ExpectedSize { get; set; }
    public uint ReceivedCrc { get; set; }
    public byte VersionMajor { get; set; }
    public byte VersionMinor { get; set; }
    public uint AppStart { get; set; }
    public uint AppMaxSize { get; set; }
}

/// <summary>
/// CRC32 计算（与固件端一致）
/// </summary>
public static class Crc32
{
    private static readonly uint[] Table = new uint[256];

    static Crc32()
    {
        for (uint i = 0; i < 256; i++)
        {
            uint crc = i;
            for (int j = 0; j < 8; j++)
            {
                crc = (crc & 1) != 0 ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
            }
            Table[i] = crc;
        }
    }

    public static uint Calculate(byte[] data)
    {
        uint crc = 0xFFFFFFFF;
        foreach (byte b in data)
        {
            crc = (crc >> 8) ^ Table[(crc ^ b) & 0xFF];
        }
        return ~crc;
    }
}
