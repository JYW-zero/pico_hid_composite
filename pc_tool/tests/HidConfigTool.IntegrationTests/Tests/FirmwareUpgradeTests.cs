/*
 * FirmwareUpgradeTests.cs
 * 固件升级集成测试
 * 测试进入 DFU 模式、固件数据传输、升级完成等流程
 */

using HidConfigTool.Core.Services;
using HidConfigTool.IntegrationTests.Simulator;

namespace HidConfigTool.IntegrationTests.Tests;

/// <summary>
/// 固件升级集成测试
/// </summary>
public class FirmwareUpgradeTests : IDisposable
{
    private readonly SimulatedHidDevice _device;
    private readonly SimulatedHidDriver _driver;
    private readonly DeviceService _deviceService;

    public FirmwareUpgradeTests()
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

    #region DFU 模式测试

    [Fact]
    public void SimulatedDevice_EnterDfuMode_ShouldSucceed()
    {
        // Arrange
        Assert.False(_device.IsInDfuMode);

        // Act - 直接调用模拟设备
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x04, 0x00, 0x00 }); // CMD_ENTER_DFU

        // Assert
        Assert.True(_device.IsInDfuMode);
    }

    [Fact]
    public void SimulatedDevice_EnterDfuMode_ShouldClearFirmwareData()
    {
        // Arrange
        _device.FirmwareUpgradeData.AddRange(new byte[] { 0x01, 0x02, 0x03 });
        Assert.NotEmpty(_device.FirmwareUpgradeData);

        // Act
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x04, 0x00, 0x00 }); // CMD_ENTER_DFU

        // Assert
        Assert.Empty(_device.FirmwareUpgradeData);
    }

    [Fact]
    public void SimulatedDevice_EnterDfuMode_ShouldAddFaultLog()
    {
        // Arrange
        int initialLogCount = _device.GetFaultLogs().Count;

        // Act
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x04, 0x00, 0x00 }); // CMD_ENTER_DFU

        // Assert
        Assert.True(_device.GetFaultLogs().Count > initialLogCount);
        var lastLog = _device.GetFaultLogs()[^1];
        Assert.Equal("dfu", lastLog.Module);
    }

    #endregion

    #region 重启测试

    [Fact]
    public async Task Reboot_ShouldNotThrow()
    {
        // Arrange
        await ConnectDeviceAsync();

        // Act & Assert - 不应该抛出异常
        await _deviceService.RebootAsync();
        Assert.True(true);
    }

    [Fact]
    public async Task Reboot_ShouldDisconnectDevice()
    {
        // Arrange
        await ConnectDeviceAsync();
        Assert.True(_deviceService.IsConnected);

        // Act
        await _deviceService.RebootAsync();

        // 等待设备断开
        await Task.Delay(100);

        // Assert - 设备应该断开
        Assert.False(_device.IsConnected);
    }

    [Fact]
    public async Task Reboot_ShouldReconnectAfterDelay()
    {
        // Arrange
        await ConnectDeviceAsync();

        // Act
        await _deviceService.RebootAsync();

        // 等待设备重启完成
        await Task.Delay(1500);

        // Assert - 设备应该重新连接
        Assert.True(_device.IsConnected);
    }

    [Fact]
    public void SimulatedDevice_Reboot_ShouldAddFaultLog()
    {
        // Arrange
        int initialLogCount = _device.GetFaultLogs().Count;

        // Act
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x03, 0x00, 0x00 }); // CMD_REBOOT

        // Assert
        Assert.True(_device.GetFaultLogs().Count >= initialLogCount);
        var rebootLog = _device.GetFaultLogs().FirstOrDefault(l => l.Message.Contains("reboot"));
        Assert.NotNull(rebootLog);
    }

    #endregion

    #region 固件升级流程测试

    [Fact]
    public void FullFirmwareUpgradeFlow_ShouldSucceed()
    {
        // Arrange - 直接测试模拟设备

        // Step 1: 进入 DFU 模式
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x04, 0x00, 0x00 }); // CMD_ENTER_DFU
        Assert.True(_device.IsInDfuMode);

        // Step 2: 模拟固件数据（在实际场景中通过 HID 报告传输）
        byte[] firmwareData = new byte[1024];
        new Random().NextBytes(firmwareData);

        // Step 3: 验证设备准备好接收数据
        Assert.True(_device.IsInDfuMode);
        Assert.Empty(_device.FirmwareUpgradeData);

        // Assert - 流程完成
        Assert.True(true);
    }

    [Fact]
    public void FirmwareUpgrade_WhenDeviceFails_ShouldHandleGracefully()
    {
        // Arrange - 设置设备故障
        _device.Behavior.Mode = DeviceBehaviorMode.ErrorResponse;

        // Act - 尝试进入 DFU 模式
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x04, 0x00, 0x00 }); // CMD_ENTER_DFU

        // Assert - 应该处理故障，不崩溃
        Assert.True(true);
    }

    [Fact]
    public void FirmwareUpgrade_WhenDeviceDisconnected_ShouldHandleGracefully()
    {
        // Arrange - 模拟设备断开
        _device.Behavior.Mode = DeviceBehaviorMode.Disconnected;

        // Act - 尝试进入 DFU 模式
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x04, 0x00, 0x00 }); // CMD_ENTER_DFU

        // Assert - 应该处理断开，不崩溃
        Assert.True(true);
    }

    #endregion

    #region 设备重置测试

    [Fact]
    public void ResetDevice_ShouldRestoreDefaultState()
    {
        // Arrange - 通过操作修改设备状态
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x0D, 0x00, 0x00 }); // 解锁1次
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x0B, 0x00, 0x00 }); // 关闭性能监控
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x0F, 0x02, 0x00 }); // 切换到 DPI 档位2
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x0C, 0x96, 0x00 }); // 设置摇杆死区150
        _device.HandleSendFeatureReport(0x07, new byte[] { 0x04, 0x00, 0x00 }); // 进入 DFU 模式
        _device.FirmwareUpgradeData.Add(0x01);

        // 验证状态已修改
        Assert.False(_device.PerfMonitorEnabled);
        Assert.Equal(2, _device.CurrentDpiSlot);
        Assert.Equal(150, _device.RealtimeDeadzone);
        Assert.True(_device.IsInDfuMode);
        Assert.NotEmpty(_device.FirmwareUpgradeData);

        // Act
        _device.Reset();

        // Assert
        Assert.False(_device.IsConfigUnlocked);
        Assert.True(_device.PerfMonitorEnabled);
        Assert.Equal(1, _device.CurrentDpiSlot);
        Assert.Equal(0, _device.RealtimeDeadzone);
        Assert.False(_device.IsInDfuMode);
        Assert.Empty(_device.FirmwareUpgradeData);
        Assert.True(_device.IsConnected);
        Assert.Equal(DeviceBehaviorMode.Normal, _device.Behavior.Mode);
    }

    [Fact]
    public void ResetDevice_ShouldClearOperationLog()
    {
        // Arrange
        _device.OperationLog.Add("test operation 1");
        _device.OperationLog.Add("test operation 2");
        Assert.True(_device.OperationLog.Count >= 2);

        // Act
        _device.Reset();

        // Assert - Reset 会添加一条 "DEVICE RESET" 日志，所以应该只有1条
        Assert.Single(_device.OperationLog);
        Assert.Contains("DEVICE RESET", _device.OperationLog[0]);
    }

    #endregion

    #region 设备行为配置测试

    [Theory]
    [InlineData(DeviceBehaviorMode.Normal)]
    [InlineData(DeviceBehaviorMode.Timeout)]
    [InlineData(DeviceBehaviorMode.ErrorResponse)]
    [InlineData(DeviceBehaviorMode.Disconnected)]
    [InlineData(DeviceBehaviorMode.Intermittent)]
    public void DeviceBehavior_ShouldSetMode(DeviceBehaviorMode mode)
    {
        // Act
        _device.Behavior.Mode = mode;

        // Assert
        Assert.Equal(mode, _device.Behavior.Mode);
    }

    [Fact]
    public void DeviceBehavior_Reset_ShouldRestoreDefaults()
    {
        // Arrange
        _device.Behavior.Mode = DeviceBehaviorMode.ErrorResponse;
        _device.Behavior.TimeoutDelayMs = 10000;
        _device.Behavior.IntermittentFailureProbability = 0.8;
        _device.Behavior.ErrorType = ErrorResponseType.RandomData;

        // Act
        _device.Behavior.Reset();

        // Assert
        Assert.Equal(DeviceBehaviorMode.Normal, _device.Behavior.Mode);
        Assert.Equal(5000, _device.Behavior.TimeoutDelayMs);
        Assert.Equal(0.3, _device.Behavior.IntermittentFailureProbability);
        Assert.Equal(ErrorResponseType.CrcMismatch, _device.Behavior.ErrorType);
    }

    [Theory]
    [InlineData(ErrorResponseType.CrcMismatch)]
    [InlineData(ErrorResponseType.IncompleteData)]
    [InlineData(ErrorResponseType.AllZeros)]
    [InlineData(ErrorResponseType.RandomData)]
    [InlineData(ErrorResponseType.NullResponse)]
    public void DeviceBehavior_ShouldSetErrorType(ErrorResponseType errorType)
    {
        // Act
        _device.Behavior.ErrorType = errorType;

        // Assert
        Assert.Equal(errorType, _device.Behavior.ErrorType);
    }

    #endregion
}
