using System.IO;
using System.Text.RegularExpressions;
using HidConfigTool.Core.Models;
using HidConfigTool.Core.Services;

namespace HidConfigTool.Tests;

/// <summary>
/// 上位机配置 ABI 与固件 device_config_t 的跨端一致性锁定测试（v8 布局）。
/// 固件侧权威定义：firmware/include/board/config.h 的 _Static_assert（锁定 sizeof/关键偏移）
/// 与 firmware/include/version.h 的 CONFIG_VERSION。
/// 上位机侧权威：ProtocolConstants / ConfigOffsets。
/// 任一侧改动二进制布局而另一侧未跟随时，这些测试必须失败，防止再次出现
/// “上位机按 1492 布局、固件按 1510 布局”这类静默错位。
/// </summary>
public class ConfigAbiConsistencyTests
{
    private const int ExpectedVersion = 8;
    private const int ExpectedSize = 1510;
    private const int ExpectedPerfOffset = 1488;
    private const int ExpectedCrcOffset = 1506;

    [Fact]
    public void Constants_MatchFirmwareV8Layout()
    {
        Assert.Equal(ExpectedVersion, ProtocolConstants.ConfigVersion);
        Assert.Equal(ExpectedSize, ProtocolConstants.ConfigSize);
        Assert.Equal(ExpectedSize, ConfigOffsets.TotalSize);

        Assert.Equal(ExpectedPerfOffset, ConfigOffsets.PerfThresholds);
        Assert.Equal(1504, ConfigOffsets.Reserved5);
        Assert.Equal(ExpectedCrcOffset, ConfigOffsets.Crc32);

        // 末尾字段连续、无空洞：perf_thresholds[8](16) + reserved5(2) + crc32(4)
        Assert.Equal(ConfigOffsets.PerfThresholds + 16, ConfigOffsets.Reserved5);
        Assert.Equal(ConfigOffsets.Reserved5 + 2, ConfigOffsets.Crc32);
        Assert.Equal(ConfigOffsets.Crc32 + 4, ConfigOffsets.TotalSize);
    }

    [Fact]
    public void DefaultDeviceConfig_IsCurrentVersion()
    {
        Assert.Equal(ProtocolConstants.ConfigVersion, new DeviceConfig().Version);
        Assert.Equal(8, new DeviceConfig().PerfThresholds.Length);
    }

    [Fact]
    public void Serialize_ProducesExpectedSize_WithCrcAtExpectedOffset()
    {
        var serializer = new ConfigSerializerService();
        byte[] data = serializer.SerializeConfig(new DeviceConfig { Version = ExpectedVersion });

        Assert.Equal(ExpectedSize, data.Length);

        uint storedCrc = BitConverter.ToUInt32(data, ConfigOffsets.Crc32);
        uint calculatedCrc = ConfigSerializerService.Crc32Calculate(data, 0, ConfigOffsets.Crc32);
        Assert.Equal(calculatedCrc, storedCrc);
    }

    [Fact]
    public void PerfThresholds_RoundTripThroughSerializeAndParse()
    {
        var serializer = new ConfigSerializerService();
        var config = new DeviceConfig { Version = ExpectedVersion };
        config.PerfThresholds[0] = 1234;
        config.PerfThresholds[7] = 5678;

        byte[] data = serializer.SerializeConfig(config);
        var parsed = serializer.ParseConfig(data);

        Assert.Equal(ExpectedVersion, parsed.Version);
        Assert.Equal((ushort)1234, parsed.PerfThresholds[0]);
        Assert.Equal((ushort)0, parsed.PerfThresholds[3]);
        Assert.Equal((ushort)5678, parsed.PerfThresholds[7]);
    }

    [Fact]
    public void FirmwareHeader_LocksSameLayout_WhenSourceTreePresent()
    {
        string? configHeader = TryFindRepoFile(Path.Combine("firmware", "include", "board", "config.h"));
        if (configHeader == null)
            return; // 脱离源码树（如单独分发的测试包）时跳过强校验，不因此失败

        string text = File.ReadAllText(configHeader);
        Assert.Contains("sizeof(device_config_t) == 1510", text);
        Assert.Contains("perf_thresholds) == 1488", text);
        Assert.Contains("crc32) == 1506", text);

        // CONFIG_VERSION 定义在 version.h（固件写作 0x0008U 这种十六进制形式）
        string? versionHeader = TryFindRepoFile(Path.Combine("firmware", "include", "version.h"));
        if (versionHeader != null)
        {
            string versionText = File.ReadAllText(versionHeader);
            Match m = Regex.Match(versionText,
                @"#\s*define\s+CONFIG_VERSION\s+(?:\(\s*)?(0x[0-9A-Fa-f]+|\d+)", RegexOptions.Multiline);
            if (m.Success)
            {
                string token = m.Groups[1].Value;
                int v = token.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
                    ? Convert.ToInt32(token, 16)
                    : int.Parse(token);
                Assert.Equal(ExpectedVersion, v);
            }
        }
    }

    private static string? TryFindRepoFile(string relativePath)
    {
        DirectoryInfo? dir = new(AppContext.BaseDirectory);
        for (int i = 0; i < 10 && dir != null; i++)
        {
            string candidate = Path.Combine(dir.FullName, relativePath);
            if (File.Exists(candidate))
                return candidate;
            dir = dir.Parent;
        }
        return null;
    }
}
