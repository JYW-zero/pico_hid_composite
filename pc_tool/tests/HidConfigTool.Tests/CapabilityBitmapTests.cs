using HidConfigTool.Core.Utils;

namespace HidConfigTool.Tests;

/// <summary>
/// 能力位图（capability bitmap）协商测试，重点锁定 CAP_DFU_UPGRADE(bit10) 语义：
/// bit10 表示“自定义 in-app DFU bootloader 已随固件编入”，必须由固件位图明确上报，
/// 不得凭协议版本推断；bit10=0 时上位机隐藏一键/在线 DFU，仅保留 BOOTSEL UF2 手动升级。
/// 固件侧对应：firmware/src/protocol/capability.c（FEATURE_DFU_BOOTLOADER）与
/// command_gate.c（CMD_ENTER_DFU 要求 bit10）。
/// </summary>
public class CapabilityBitmapTests
{
    private const int DfuBit = (int)CapabilityId.DfuUpgrade; // 10

    [Fact]
    public void DfuUpgrade_Available_Only_When_Bit10_Set()
    {
        var withDfu = VersionComparer.EvaluateCapabilities(1UL << DfuBit, new Version(0, 2));
        var withoutDfu = VersionComparer.EvaluateCapabilities(0UL, new Version(0, 2));

        Assert.True(withDfu[CapabilityId.DfuUpgrade].IsAvailable);
        Assert.False(withoutDfu[CapabilityId.DfuUpgrade].IsAvailable);
    }

    [Fact]
    public void Bit10_Clear_Does_Not_Affect_Consumer_Base_Capabilities()
    {
        // 仅键盘基础位置位、bit10=0：键盘仍可用，DFU 不可用（BOOTSEL UF2 不占能力位，始终可用）
        var caps = VersionComparer.EvaluateCapabilities(
            1UL << (int)CapabilityId.KeyboardBasic, new Version(0, 2));

        Assert.True(caps[CapabilityId.KeyboardBasic].IsAvailable);
        Assert.False(caps[CapabilityId.DfuUpgrade].IsAvailable);
    }

    [Fact]
    public void Legacy_V2_ZeroBitmap_Does_Not_Infer_Dfu()
    {
        // 旧固件位图为 0、协议主版本>=2 时可推断消费者基础位，但 bit10 依赖 bootloader 实际编入，
        // 不得凭版本推断为可用，否则会向消费者展示必然失败的一键/在线升级。
        var caps = VersionComparer.EvaluateCapabilities(0UL, new Version(2, 0));

        Assert.True(caps[CapabilityId.KeyboardBasic].IsAvailable);
        Assert.False(caps[CapabilityId.DfuUpgrade].IsAvailable);
    }

    [Fact]
    public void ParseDeviceInfo_Bit10_Clear_Reports_No_Dfu()
    {
        // 含 Report ID(0x06) 的 32 字节 DeviceInfo：offset=1，capability_bitmap 在 data[13..20]
        byte[] data = new byte[32];
        data[0] = 0x06;
        // 协议版本 0.2：ToUInt16(data, offset+4)，高字节=主版本 -> data[5]=0, data[6]=2
        data[5] = 0;
        data[6] = 2;
        // 能力位图：仅消费者基础（键盘 bit0），bit10=0
        ulong bitmap = 1UL << (int)CapabilityId.KeyboardBasic;
        BitConverter.GetBytes(bitmap).CopyTo(data, 13);

        var info = VersionComparer.ParseDeviceInfo(data);

        Assert.True(info.IsCapabilityAvailable(CapabilityId.KeyboardBasic));
        Assert.False(info.IsCapabilityAvailable(CapabilityId.DfuUpgrade));
    }

    [Fact]
    public void ParseDeviceInfo_Bit10_Set_Reports_Dfu()
    {
        byte[] data = new byte[32];
        data[0] = 0x06;
        data[5] = 0;
        data[6] = 2;
        ulong bitmap = (1UL << (int)CapabilityId.KeyboardBasic) | (1UL << DfuBit);
        BitConverter.GetBytes(bitmap).CopyTo(data, 13);

        var info = VersionComparer.ParseDeviceInfo(data);

        Assert.True(info.IsCapabilityAvailable(CapabilityId.DfuUpgrade));
    }
}
