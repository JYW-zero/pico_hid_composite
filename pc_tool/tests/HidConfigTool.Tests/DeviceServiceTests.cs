/*
 * DeviceServiceTests.cs
 * DeviceService 单元测试
 * 使用 Moq Mock IHidDriver 接口，测试设备连接、配置读写、异常处理
 */

using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Services;
using Moq;
using Xunit;

namespace HidConfigTool.Tests;

/// <summary>
/// DeviceService 单元测试
/// 通过 Mock IHidDriver 接口测试 DeviceService 的核心逻辑
/// </summary>
public class DeviceServiceTests : IDisposable
{
    private readonly Mock<IHidDriver> _mockDriver;
    private readonly DeviceService _service;

    public DeviceServiceTests()
    {
        _mockDriver = new Mock<IHidDriver>();
        _service = new DeviceService(_mockDriver.Object);
    }

    public void Dispose()
    {
        _service.Dispose();
    }

    #region 构造函数和初始状态测试

    /// <summary>
    /// 测试构造函数正确初始化状态
    /// </summary>
    [Fact]
    public void Constructor_ShouldInitializeWithDefaultState()
    {
        // Assert
        Assert.False(_service.IsConnected);
        Assert.Null(_service.CurrentConfig);
    }

    /// <summary>
    /// 测试构造函数传入 null 驱动应抛出异常
    /// </summary>
    [Fact]
    public void Constructor_NullDriver_ShouldThrowArgumentNullException()
    {
        // Act & Assert
        Assert.Throws<ArgumentNullException>(() => new DeviceService(null!));
    }

    #endregion

    #region 设备查找测试

    /// <summary>
    /// 测试查找设备成功
    /// </summary>
    [Fact]
    public async Task GetDevicesAsync_ShouldReturnDevicesFromDriver()
    {
        // Arrange
        var expectedDevices = new List<HidDeviceInfo>
        {
            new() { DevicePath = "\\\\?\\hid#vid_1209&pid_4004#1", VendorId = 0x1209, ProductId = 0x4004, ProductName = "Pico HID Composite", UsagePage = 0xFF00 }
        };
        // 使用回调根据 VID/PID 返回不同结果
        _mockDriver.Setup(d => d.FindDevicesAsync(It.IsAny<ushort>(), It.IsAny<ushort>()))
            .ReturnsAsync((ushort vid, ushort pid) =>
                (vid == DeviceService.DeviceVendorId && pid == DeviceService.DeviceProductId)
                    ? expectedDevices
                    : new List<HidDeviceInfo>());

        // Act
        var devices = await _service.GetDevicesAsync();

        // Assert
        Assert.NotNull(devices);
        Assert.Single(devices);
        Assert.Equal("Pico HID Composite", devices[0].ProductName);
    }

    /// <summary>
    /// 测试查找设备时驱动返回空列表
    /// </summary>
    [Fact]
    public async Task GetDevicesAsync_NoDevices_ShouldReturnEmptyList()
    {
        // Arrange
        _mockDriver.Setup(d => d.FindDevicesAsync(It.IsAny<ushort>(), It.IsAny<ushort>()))
            .ReturnsAsync(new List<HidDeviceInfo>());

        // Act
        var devices = await _service.GetDevicesAsync();

        // Assert
        Assert.NotNull(devices);
        Assert.Empty(devices);
    }

    /// <summary>
    /// 测试查找设备时驱动抛出异常应被捕获并返回空列表
    /// </summary>
    [Fact]
    public async Task GetDevicesAsync_DriverThrows_ShouldReturnEmptyList()
    {
        // Arrange
        _mockDriver.Setup(d => d.FindDevicesAsync(It.IsAny<ushort>(), It.IsAny<ushort>()))
            .ThrowsAsync(new IOException("USB error"));

        // Act
        var devices = await _service.GetDevicesAsync();

        // Assert
        Assert.NotNull(devices);
        Assert.Empty(devices);
    }

    #endregion

    #region 设备连接测试

    /// <summary>
    /// 测试连接设备失败（OpenAsync 返回 false）
    /// </summary>
    [Fact]
    public async Task ConnectAsync_OpenFails_ShouldReturnFalse()
    {
        // Arrange
        var deviceInfo = new HidDeviceInfo { DevicePath = "\\\\?\\hid#1" };
        _mockDriver.Setup(d => d.OpenAsync(It.IsAny<string>()))
            .ReturnsAsync(false);

        // Act
        bool result = await _service.ConnectAsync(deviceInfo);

        // Assert
        Assert.False(result);
        Assert.False(_service.IsConnected);
    }

    /// <summary>
    /// 测试连接设备时驱动抛出异常应返回 false
    /// </summary>
    [Fact]
    public async Task ConnectAsync_DriverThrows_ShouldReturnFalse()
    {
        // Arrange
        var deviceInfo = new HidDeviceInfo { DevicePath = "\\\\?\\hid#1" };
        _mockDriver.Setup(d => d.OpenAsync(It.IsAny<string>()))
            .ThrowsAsync(new UnauthorizedAccessException("Access denied"));

        // Act
        bool result = await _service.ConnectAsync(deviceInfo);

        // Assert
        Assert.False(result);
        Assert.False(_service.IsConnected);
    }

    #endregion

    #region 设备断开测试

    /// <summary>
    /// 测试未连接时断开设备不应抛出异常
    /// </summary>
    [Fact]
    public void Disconnect_NotConnected_ShouldNotThrow()
    {
        // Act & Assert
        _service.Disconnect(); // 未连接时断开不应抛出异常
        Assert.False(_service.IsConnected);
    }

    /// <summary>
    /// 测试多次断开不应抛出异常
    /// </summary>
    [Fact]
    public void Disconnect_MultipleCalls_ShouldNotThrow()
    {
        // Act & Assert
        _service.Disconnect();
        _service.Disconnect(); // 第二次调用不应抛出异常
    }

    #endregion

    #region 资源释放测试

    /// <summary>
    /// 测试 Dispose 应释放驱动资源
    /// </summary>
    [Fact]
    public void Dispose_ShouldDisposeDriver()
    {
        // Arrange
        var service = new DeviceService(_mockDriver.Object);

        // Act
        service.Dispose();

        // Assert
        _mockDriver.Verify(d => d.Dispose(), Times.Once);
    }

    /// <summary>
    /// 测试多次 Dispose 不应抛出异常
    /// </summary>
    [Fact]
    public void Dispose_MultipleCalls_ShouldNotThrow()
    {
        // Arrange
        var service = new DeviceService(_mockDriver.Object);

        // Act & Assert
        service.Dispose();
        service.Dispose(); // 第二次调用不应抛出异常
    }

    #endregion

    #region 错误历史测试

    /// <summary>
    /// 测试清除错误历史
    /// </summary>
    [Fact]
    public void ClearErrorHistory_ShouldNotThrow()
    {
        // Act & Assert
        _service.ClearErrorHistory(); // 不应抛出异常
    }

    #endregion

    #region 常量测试

    /// <summary>
    /// 测试设备 VID/PID 常量值
    /// </summary>
    [Fact]
    public void DeviceVendorId_ShouldBeExpectedValue()
    {
        // Assert
        Assert.Equal(0x1209, DeviceService.DeviceVendorId);
        Assert.Equal(0x4004, DeviceService.DeviceProductId);
    }

    #endregion
}
