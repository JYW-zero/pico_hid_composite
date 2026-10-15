/*
 * ConfigMigrationTests.cs
 * 配置版本迁移服务单元测试
 *
 * 测试 ConfigMigrationService 的所有迁移路径，确保：
 * 1. 最新版本（v8）配置不需要迁移
 * 2. 迁移过程中保留已有字段的值，并初始化新版本新增字段
 * 3. 边界情况处理正确（v0 当作 v1 起步、幂等、跨多版本）
 */

using HidConfigTool.Core.Models;
using HidConfigTool.Core.Services;

namespace HidConfigTool.Tests;

public class ConfigMigrationTests
{
    private readonly ConfigMigrationService _service = new();

    // ==================== 基础功能测试 ====================

    [Fact]
    public void GetCurrentVersion_Returns8()
    {
        Assert.Equal(8, _service.GetCurrentVersion());
    }

    [Fact]
    public void IsMigrationNeeded_NullConfig_ReturnsFalse()
    {
        Assert.False(_service.IsMigrationNeeded(null!));
    }

    [Fact]
    public void IsMigrationNeeded_CurrentV8Config_ReturnsFalse()
    {
        var config = new DeviceConfig { Version = 8 };
        Assert.False(_service.IsMigrationNeeded(config));
    }

    [Fact]
    public void IsMigrationNeeded_OlderV1Config_ReturnsTrue()
    {
        var config = new DeviceConfig { Version = 1 };
        Assert.True(_service.IsMigrationNeeded(config));
    }

    [Fact]
    public void IsMigrationNeeded_V0Config_ReturnsTrue()
    {
        var config = new DeviceConfig { Version = 0 };
        Assert.True(_service.IsMigrationNeeded(config));
    }

    [Fact]
    public void IsMigrationNeeded_NewerVersion_ReturnsFalse()
    {
        // 更新版本设备下发的配置，不在上位机做降级迁移
        var config = new DeviceConfig { Version = 99 };
        Assert.False(_service.IsMigrationNeeded(config));
    }

    // ==================== 迁移测试 ====================

    [Fact]
    public void Migrate_NullConfig_ReturnsNull()
    {
        var result = _service.Migrate(null!);
        Assert.Null(result);
    }

    [Fact]
    public void Migrate_V8Config_NoChange()
    {
        var config = new DeviceConfig
        {
            Version = 8,
            Dpi = 1600,
            JoystickDeadzone = 200
        };

        var result = _service.Migrate(config);

        Assert.Same(config, result);
        Assert.Equal(8, result.Version);
        Assert.Equal(1600, result.Dpi);
        Assert.Equal(200, result.JoystickDeadzone);
    }

    [Fact]
    public void Migrate_V1Config_UpgradesToV8_AndPreservesFields()
    {
        var config = new DeviceConfig
        {
            Version = 1,
            Dpi = 1600,
            JoystickDeadzone = 200
        };

        var result = _service.Migrate(config);

        Assert.Same(config, result);
        Assert.Equal(8, result.Version);
        Assert.Equal(1600, result.Dpi);
        Assert.Equal(200, result.JoystickDeadzone);
        // 各版本新增字段应被初始化（宏数据不在 DeviceConfig 模型内，走独立 HID 报告）
        Assert.NotNull(result.Layer2Keymap);
        Assert.NotNull(result.Layer3Keymap);
        Assert.NotNull(result.PerfThresholds);
        Assert.Equal(8, result.PerfThresholds.Length);
    }

    [Fact]
    public void Migrate_V0Config_UpgradesToV8()
    {
        var config = new DeviceConfig
        {
            Version = 0,
            Dpi = 800
        };

        var result = _service.Migrate(config);

        Assert.Same(config, result);
        Assert.Equal(8, result.Version);
        Assert.Equal(800, result.Dpi);
    }

    [Fact]
    public void Migrate_V7ToV8_InitializesPerfThresholdsToZero()
    {
        var config = new DeviceConfig
        {
            Version = 7,
            Dpi = 1200,
            Layer2Keymap = new byte[64],
            Layer3Keymap = new byte[64]
        };

        var result = _service.Migrate(config);

        Assert.Equal(8, result.Version);
        Assert.NotNull(result.PerfThresholds);
        Assert.Equal(8, result.PerfThresholds.Length);
        Assert.All(result.PerfThresholds, v => Assert.Equal((ushort)0, v));
        // 旧字段保留
        Assert.Equal(1200, result.Dpi);
    }

    [Fact]
    public void Migrate_MultipleCalls_Idempotent()
    {
        var config = new DeviceConfig { Version = 0, Dpi = 1600 };

        _service.Migrate(config);
        var versionAfterFirst = config.Version;
        _service.Migrate(config);

        Assert.Equal(8, versionAfterFirst);
        Assert.Equal(versionAfterFirst, config.Version);
        Assert.Equal(8, config.Version);
    }

    // ==================== 边界情况测试 ====================

    [Fact]
    public void Migrate_V3ToV8_PreservesExistingFields()
    {
        // 从已含 v3 字段的 v3 配置出发：后续 v4-v8 迁移不得覆盖这些已有值
        var config = new DeviceConfig
        {
            Version = 3,
            Dpi = 2400,
            JoystickDeadzone = 150,
            EncoderReverse = true,
            JoystickInvertX = true,
            JoystickInvertY = false,
            EncoderStepsPerTick = 3,
            EncoderScrollSpeed = 5,
            JoystickSensitivity = 1.5
        };

        var result = _service.Migrate(config);

        Assert.Equal(8, result.Version);
        Assert.Equal(2400, result.Dpi);
        Assert.Equal(150, result.JoystickDeadzone);
        Assert.True(result.EncoderReverse);
        Assert.True(result.JoystickInvertX);
        Assert.False(result.JoystickInvertY);
        Assert.Equal(3, result.EncoderStepsPerTick);
        Assert.Equal(5, result.EncoderScrollSpeed);
        Assert.Equal(1.5, result.JoystickSensitivity);
        // 新版本新增字段被初始化
        Assert.NotNull(result.Layer2Keymap);
        Assert.NotNull(result.Layer3Keymap);
        Assert.NotNull(result.PerfThresholds);
        Assert.Equal(8, result.PerfThresholds.Length);
    }
}
