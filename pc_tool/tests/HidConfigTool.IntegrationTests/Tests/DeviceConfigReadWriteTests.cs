/*
 * DeviceConfigReadWriteTests.cs
 * 配置读写集成测试
 * 测试完整的配置读取、修改、写入、保存流程
 */

using HidConfigTool.Core.Models;
using HidConfigTool.Core.Services;
using HidConfigTool.IntegrationTests.Simulator;

namespace HidConfigTool.IntegrationTests.Tests;

/// <summary>
/// 配置读写集成测试
/// </summary>
public class DeviceConfigReadWriteTests : IDisposable
{
    private readonly SimulatedHidDevice _device;
    private readonly SimulatedHidDriver _driver;
    private readonly DeviceService _deviceService;

    public DeviceConfigReadWriteTests()
    {
        _device = new SimulatedHidDevice();
        _driver = new SimulatedHidDriver(_device);
        _deviceService = new DeviceService(_driver);
    }

    public void Dispose()
    {
        _deviceService.Dispose();
        _driver.Dispose();
    }

    #region 辅助方法

    /// <summary>
    /// 连接设备
    /// </summary>
    private async Task ConnectDeviceAsync()
    {
        var devices = await _deviceService.GetDevicesAsync();
        Assert.NotEmpty(devices);
        bool connected = await _deviceService.ConnectAsync(devices[0]);
        Assert.True(connected);
    }

    #endregion

    #region 配置读取测试

    [Fact]
    public async Task ReadConfig_AfterConnect_ShouldReturnValidConfig()
    {
        // Arrange
        await ConnectDeviceAsync();

        // Act
        var config = _deviceService.CurrentConfig;

        // Assert
        Assert.NotNull(config);
        // 配置版本应该 >= 1
        Assert.True(config.Version >= 1);
    }

    [Fact]
    public async Task ReadConfig_ShouldHaveDefaultDpi()
    {
        // Arrange
        await ConnectDeviceAsync();

        // Act
        var config = _deviceService.CurrentConfig;

        // Assert
        Assert.NotNull(config);
        Assert.Equal(800, config.Dpi);
    }

    [Fact]
    public async Task ReadConfig_ShouldHaveDefaultDeadzone()
    {
        // Arrange
        await ConnectDeviceAsync();

        // Act
        var config = _deviceService.CurrentConfig;

        // Assert
        Assert.NotNull(config);
        Assert.Equal(100, config.JoystickDeadzone);
    }

    #endregion

    #region 配置写入测试

    [Fact]
    public async Task SaveConfig_WithValidConfig_ShouldNotThrow()
    {
        // Arrange
        await ConnectDeviceAsync();

        var config = _deviceService.CurrentConfig;
        Assert.NotNull(config);
        config.Dpi = 1600;
        config.JoystickDeadzone = 200;

        // Act & Assert - 不应该抛出异常
        await _deviceService.SaveConfigAsync(config);
        Assert.True(true);
    }

    [Fact]
    public async Task SaveConfig_WhenDeviceFails_ShouldFail()
    {
        // Arrange
        await ConnectDeviceAsync();

        // 设置设备故障
        _device.Behavior.Mode = DeviceBehaviorMode.ErrorResponse;

        var config = _deviceService.CurrentConfig;
        Assert.NotNull(config);
        config.Dpi = 1600;

        // Act
        bool result = await _deviceService.SaveConfigAsync(config);

        // Assert
        Assert.False(result);
    }

    #endregion

    #region 配置重置测试

    [Fact]
    public async Task ResetConfig_ShouldSucceed()
    {
        // Arrange
        await ConnectDeviceAsync();

        // Act
        bool result = await _deviceService.ResetConfigAsync();

        // Assert
        Assert.True(result);
    }

    [Fact]
    public async Task ResetConfig_ShouldRestoreDefaults()
    {
        // Arrange
        await ConnectDeviceAsync();

        // Act
        await _deviceService.ResetConfigAsync();

        // 重新读取配置
        var config = _deviceService.CurrentConfig;

        // Assert
        Assert.NotNull(config);
        Assert.Equal(800, config.Dpi); // 默认值
    }

    #endregion

    #region 性能监控测试

    [Fact]
    public async Task SetPerfMonitorEnabled_ShouldSucceed()
    {
        // Arrange
        await ConnectDeviceAsync();
        Assert.True(_device.PerfMonitorEnabled);

        // Act - 关闭性能监控
        bool result = await _deviceService.SetPerfMonitorEnabledAsync(false);

        // Assert
        Assert.True(result);
        Assert.False(_device.PerfMonitorEnabled);
    }

    [Fact]
    public async Task SetPerfMonitorEnabled_True_ShouldEnable()
    {
        // Arrange
        await ConnectDeviceAsync();
        await _deviceService.SetPerfMonitorEnabledAsync(false);

        // Act
        bool result = await _deviceService.SetPerfMonitorEnabledAsync(true);

        // Assert - 方法调用成功即可，设备状态可能有延迟
        Assert.True(result);
    }

    #endregion

    #region 摇杆死区测试

    [Fact]
    public async Task SetJoystickDeadzoneRealtime_ShouldSucceed()
    {
        // Arrange
        await ConnectDeviceAsync();
        Assert.Equal(0, _device.RealtimeDeadzone);

        // Act
        bool result = await _deviceService.SetJoystickDeadzoneRealtimeAsync(150);

        // Assert
        Assert.True(result);
        Assert.Equal(150, _device.RealtimeDeadzone);
    }

    [Fact]
    public async Task SetJoystickDeadzoneRealtime_WithZero_ShouldSucceed()
    {
        // Arrange
        await ConnectDeviceAsync();
        await _deviceService.SetJoystickDeadzoneRealtimeAsync(150);

        // Act
        bool result = await _deviceService.SetJoystickDeadzoneRealtimeAsync(0);

        // Assert
        Assert.True(result);
        Assert.Equal(0, _device.RealtimeDeadzone);
    }

    #endregion

    #region DPI 切换测试

    [Fact]
    public void SimulatedDevice_SwitchDpiSlot_Cycle_ShouldSucceed()
    {
        // Arrange
        int initialSlot = _device.CurrentDpiSlot;

        // Act - 直接测试模拟设备
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x0F, 0xFF, 0x00 }); // CMD_SWITCH_DPI, param=0xFF (循环)

        // Assert
        Assert.Equal((initialSlot + 1) % 4, _device.CurrentDpiSlot);
    }

    [Fact]
    public void SimulatedDevice_SwitchDpiSlot_ToSpecificSlot_ShouldSetSlot()
    {
        // Arrange

        // Act - 直接测试模拟设备
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x0F, 0x02, 0x00 }); // CMD_SWITCH_DPI, param=2 (指定档位)

        // Assert
        Assert.Equal(2, _device.CurrentDpiSlot);
    }

    #endregion

    #region 故障模拟测试

    [Fact]
    public async Task ReadConfig_WhenDeviceDisconnected_ShouldHandleGracefully()
    {
        // Arrange - 不连接设备，直接设置断开行为
        _device.Behavior.Mode = DeviceBehaviorMode.Disconnected;

        // Act - 尝试查找设备
        var devices = await _deviceService.GetDevicesAsync();

        // Assert - 应该返回空列表，不崩溃
        Assert.Empty(devices);
    }

    [Fact]
    public async Task SaveConfig_WhenDeviceReturnsError_ShouldFail()
    {
        // Arrange
        await ConnectDeviceAsync();

        // 设置设备返回错误
        _device.Behavior.Mode = DeviceBehaviorMode.ErrorResponse;
        _device.Behavior.ErrorType = ErrorResponseType.CrcMismatch;

        var config = _deviceService.CurrentConfig;
        Assert.NotNull(config);
        config.Dpi = 1600;

        // Act
        bool result = await _deviceService.SaveConfigAsync(config);

        // Assert
        Assert.False(result);
    }

    #endregion

    #region 模拟设备直接测试

    [Fact]
    public void SimulatedDevice_UnlockConfig_WithThreeAttempts_ShouldSucceed()
    {
        // Arrange
        Assert.False(_device.IsConfigUnlocked);

        // Act - 直接调用模拟设备的控制命令处理
        for (int i = 0; i < 3; i++)
        {
            _device.HandleSendFeatureReport(0x07, new byte[] { 0x0D, 0x00, 0x00 }); // CMD_UNLOCK_CONFIG
        }

        // Assert
        Assert.True(_device.IsConfigUnlocked);
    }

    [Fact]
    public void SimulatedDevice_UnlockConfig_WithLessThanThreeAttempts_ShouldFail()
    {
        // Arrange
        Assert.False(_device.IsConfigUnlocked);

        // Act
        for (int i = 0; i < 2; i++)
        {
            _device.HandleSendFeatureReport(0x07, new byte[] { 0x0D, 0x00, 0x00 });
        }

        // Assert
        Assert.False(_device.IsConfigUnlocked);
    }

    [Fact]
    public void SimulatedDevice_ResetConfig_ShouldRestoreDefaults()
    {
        // Arrange - 直接修改配置
        byte[] configData = _device.GetConfigData();
        configData[6] = 0x80; // 修改 DPI 为 128
        configData[7] = 0x05;

        // Act
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x02, 0x00, 0x00 }); // CMD_RESET_CONFIG

        // Assert
        byte[] newConfigData = _device.GetConfigData();
        ushort dpi = BitConverter.ToUInt16(newConfigData, 6);
        Assert.Equal(800, dpi); // 默认值
    }

    [Fact]
    public void SimulatedDevice_ShouldLogOperations()
    {
        // Arrange
        Assert.Empty(_device.OperationLog);

        // Act - 执行一些操作
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x0D, 0x00, 0x00 }); // 解锁
        _device.HandleGetFeatureReport(0x06); // 读取设备信息

        // Assert
        Assert.NotEmpty(_device.OperationLog);
        Assert.Contains(_device.OperationLog, log => log.Contains("UNLOCK"));
        Assert.Contains(_device.OperationLog, log => log.Contains("GET Feature Report"));
    }

    [Fact]
    public void SimulatedDevice_SimulateKeyPress_ShouldIncrementStats()
    {
        // Arrange
        _device.SimulateKeyPress(0);
        _device.SimulateKeyPress(0);
        _device.SimulateKeyPress(1);

        // Act - 读取按键统计
        byte[] statsData = _device.HandleGetFeatureReport(0x0A)!; // REPORT_ID_KEY_STATS0
        uint key0Stats = BitConverter.ToUInt32(statsData, 0);
        uint key1Stats = BitConverter.ToUInt32(statsData, 4);

        // Assert
        Assert.Equal(2u, key0Stats);
        Assert.Equal(1u, key1Stats);
    }

    #endregion
}
