using HidConfigTool.Core.Models;
using System.Text.Json;

namespace HidConfigTool.Tests;

public class DeviceConfigTests
{
    [Fact]
    public void DeviceConfig_DefaultValues_AreCorrect()
    {
        var config = new DeviceConfig();

        Assert.Equal(1, config.Version);
        Assert.Equal(800, config.Dpi);
        Assert.Equal(1, config.DpiIndex);
        Assert.NotNull(config.DpiLevels);
        Assert.Equal(4, config.DpiLevels.Length);
        Assert.Equal(400, config.DpiLevels[0]);
        Assert.Equal(800, config.DpiLevels[1]);
        Assert.Equal(1600, config.DpiLevels[2]);
        Assert.Equal(3200, config.DpiLevels[3]);
        Assert.False(config.AccelerationEnabled);
        Assert.Equal(10, config.AccelerationThreshold);
        Assert.Equal(1.5, config.AccelerationRatio);
        Assert.Equal(100, config.JoystickDeadzone);
        Assert.False(config.EncoderReverse);
        Assert.NotNull(config.Keymap);
        Assert.Equal(64, config.Keymap.Length);
        Assert.NotNull(config.FnKeymap);
        Assert.Equal(64, config.FnKeymap.Length);
    }

    [Fact]
    public void DeviceConfig_CanSerializeAndDeserialize()
    {
        var config = new DeviceConfig
        {
            Dpi = 1600,
            DpiIndex = 2,
            AccelerationEnabled = true,
            EncoderReverse = true
        };

        var json = JsonSerializer.Serialize(config);
        var deserialized = JsonSerializer.Deserialize<DeviceConfig>(json);

        Assert.NotNull(deserialized);
        Assert.Equal(1600, deserialized.Dpi);
        Assert.Equal(2, deserialized.DpiIndex);
        Assert.True(deserialized.AccelerationEnabled);
        Assert.True(deserialized.EncoderReverse);
        Assert.Equal(64, deserialized.Keymap.Length);
        Assert.Equal(64, deserialized.FnKeymap.Length);
    }

    [Fact]
    public void DeviceConfig_Keymap_CanSetAndGet()
    {
        var config = new DeviceConfig();
        config.Keymap[0] = 0x04; // A
        config.Keymap[1] = 0x05; // B

        Assert.Equal(0x04, config.Keymap[0]);
        Assert.Equal(0x05, config.Keymap[1]);
    }
}
