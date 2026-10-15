/*
 * DeviceConnectionTests.cs
 * 设备连接集成测试
 * 测试设备发现、连接、断开、重连等流程
 */

using HidConfigTool.Core.Services;
using HidConfigTool.IntegrationTests.Simulator;

namespace HidConfigTool.IntegrationTests.Tests;

/// <summary>
/// 设备连接集成测试
/// </summary>
public class DeviceConnectionTests : IDisposable
{
    private readonly SimulatedHidDevice _device;
    private readonly SimulatedHidDriver _driver;
    private readonly DeviceService _deviceService;

    public DeviceConnectionTests()
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

    #region 设备发现测试

    [Fact]
    public async Task FindDevicesAsync_WithMatchingVidPid_ShouldReturnDevice()
    {
        // Act
        var devices = await _deviceService.GetDevicesAsync();

        // Assert
        Assert.NotNull(devices);
        Assert.NotEmpty(devices);
        Assert.Equal(SimulatedHidDevice.DEFAULT_VENDOR_ID, devices[0].VendorId);
        Assert.Equal(SimulatedHidDevice.DEFAULT_PRODUCT_ID, devices[0].ProductId);
    }

    [Fact]
    public async Task FindDevicesAsync_WithNonMatchingVidPid_ShouldReturnEmpty()
    {
        // Act
        var devices = await _driver.FindDevicesAsync(0x1234, 0x5678);

        // Assert
        Assert.NotNull(devices);
        Assert.Empty(devices);
    }

    [Fact]
    public async Task FindDevicesAsync_WhenDeviceDisconnected_ShouldReturnEmpty()
    {
        // Arrange
        _device.Behavior.Mode = DeviceBehaviorMode.Disconnected;

        // Act
        var devices = await _deviceService.GetDevicesAsync();

        // Assert
        Assert.NotNull(devices);
        Assert.Empty(devices);
    }

    [Fact]
    public async Task FindDevicesAsync_ShouldReturnCorrectDeviceInfo()
    {
        // Act
        var devices = await _deviceService.GetDevicesAsync();

        // Assert
        Assert.NotEmpty(devices);
        var device = devices[0];
        Assert.Equal(_device.ProductName, device.ProductName);
        Assert.Equal(_device.ManufacturerName, device.ManufacturerName);
        Assert.Equal(_device.SerialNumber, device.SerialNumber);
        Assert.Equal(SimulatedHidDevice.DEFAULT_USAGE_PAGE, device.UsagePage);
    }

    #endregion

    #region 设备连接测试

    [Fact]
    public async Task ConnectAsync_WithValidDevice_ShouldSucceed()
    {
        // Arrange
        var devices = await _deviceService.GetDevicesAsync();
        Assert.NotEmpty(devices);

        // Act
        bool result = await _deviceService.ConnectAsync(devices[0]);

        // Assert
        Assert.True(result);
        Assert.True(_deviceService.IsConnected);
    }

    [Fact]
    public async Task ConnectAsync_WhenDeviceFails_ShouldFail()
    {
        // Arrange
        _device.Behavior.Mode = DeviceBehaviorMode.ErrorResponse;
        var devices = await _deviceService.GetDevicesAsync();
        Assert.NotEmpty(devices);

        // Act
        bool result = await _deviceService.ConnectAsync(devices[0]);

        // Assert
        Assert.False(result);
        Assert.False(_deviceService.IsConnected);
    }

    [Fact]
    public async Task DisconnectAsync_AfterConnect_ShouldSucceed()
    {
        // Arrange
        var devices = await _deviceService.GetDevicesAsync();
        await _deviceService.ConnectAsync(devices[0]);
        Assert.True(_deviceService.IsConnected);

        // Act
        _deviceService.Disconnect();

        // Assert
        Assert.False(_deviceService.IsConnected);
    }

    #endregion

    #region 设备信息读取测试

    [Fact]
    public async Task ConnectAsync_ShouldReadDeviceInfo()
    {
        // Arrange
        var devices = await _deviceService.GetDevicesAsync();

        // Act
        await _deviceService.ConnectAsync(devices[0]);

        // Assert
        Assert.NotNull(_deviceService.VersionInfo);
        Assert.NotNull(_deviceService.FirmwareVersion);
        Assert.Contains("v1.0.0", _deviceService.FirmwareVersion);
    }

    [Fact]
    public async Task ConnectAsync_ShouldReadConfig()
    {
        // Arrange
        var devices = await _deviceService.GetDevicesAsync();

        // Act
        await _deviceService.ConnectAsync(devices[0]);

        // Assert
        Assert.NotNull(_deviceService.CurrentConfig);
    }

    #endregion

    #region 重连测试

    [Fact]
    public async Task Reconnect_AfterDisconnect_ShouldSucceed()
    {
        // Arrange
        var devices = await _deviceService.GetDevicesAsync();
        await _deviceService.ConnectAsync(devices[0]);
        _deviceService.Disconnect();
        Assert.False(_deviceService.IsConnected);

        // Act
        bool result = await _deviceService.ConnectAsync(devices[0]);

        // Assert
        Assert.True(result);
        Assert.True(_deviceService.IsConnected);
    }

    [Fact]
    public async Task MultipleConnectDisconnect_ShouldNotLeakResources()
    {
        // Arrange
        var devices = await _deviceService.GetDevicesAsync();

        // Act & Assert
        for (int i = 0; i < 5; i++)
        {
            bool connected = await _deviceService.ConnectAsync(devices[0]);
            Assert.True(connected);
            _deviceService.Disconnect();
            Assert.False(_deviceService.IsConnected);
        }
    }

    #endregion

    #region 设备状态测试

    [Fact]
    public void IsConnected_BeforeConnect_ShouldBeFalse()
    {
        Assert.False(_deviceService.IsConnected);
    }

    [Fact]
    public void DeviceName_BeforeConnect_ShouldBeNull()
    {
        Assert.Null(_deviceService.DeviceName);
    }

    [Fact]
    public async Task DeviceName_AfterConnect_ShouldBeSet()
    {
        // Arrange
        var devices = await _deviceService.GetDevicesAsync();
        await _deviceService.ConnectAsync(devices[0]);

        // Assert
        Assert.NotNull(_deviceService.DeviceName);
    }

    #endregion
}
