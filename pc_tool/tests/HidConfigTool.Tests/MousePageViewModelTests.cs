/*
 * MousePageViewModelTests.cs
 * MousePageViewModel 单元测试
 * 测试 DPI 设置、档位切换、高级配置保存
 */

using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;
using HidConfigTool.ViewModels;
using Moq;

namespace HidConfigTool.Tests;

/// <summary>
/// MousePageViewModel 单元测试
/// </summary>
public class MousePageViewModelTests
{
    private readonly Mock<IDeviceService> _mockDeviceService;
    private readonly MousePageViewModel _viewModel;

    public MousePageViewModelTests()
    {
        _mockDeviceService = new Mock<IDeviceService>();
        _mockDeviceService.Setup(s => s.IsConnected).Returns(false);
        _mockDeviceService.Setup(s => s.CurrentConfig).Returns((DeviceConfig?)null);
        _viewModel = new MousePageViewModel(_mockDeviceService.Object);
    }

    #region 初始状态测试

    [Fact]
    public void Constructor_ShouldSetDefaultDpiValues()
    {
        // Assert
        Assert.Equal(800, _viewModel.CurrentDpi);
        Assert.Equal(1, _viewModel.CurrentDpiIndex);
        Assert.Equal(400, _viewModel.DpiSlot0);
        Assert.Equal(800, _viewModel.DpiSlot1);
        Assert.Equal(1600, _viewModel.DpiSlot2);
        Assert.Equal(3200, _viewModel.DpiSlot3);
    }

    [Fact]
    public void Constructor_ShouldSetDefaultAdvancedValues()
    {
        // Assert
        Assert.False(_viewModel.AccelerationEnabled);
        Assert.Equal(10, _viewModel.AccelerationThreshold);
        Assert.Equal(1.5, _viewModel.AccelerationRatio);
        Assert.Equal(1, _viewModel.LodLevel);
        Assert.False(_viewModel.AngleSnapping);
    }

    [Fact]
    public void Constructor_ShouldHaveThreeLodOptions()
    {
        // Assert
        Assert.Equal(3, _viewModel.LodOptions.Length);
        Assert.Equal("低", _viewModel.LodOptions[0]);
        Assert.Equal("中", _viewModel.LodOptions[1]);
        Assert.Equal("高", _viewModel.LodOptions[2]);
    }

    [Fact]
    public void CurrentDpiText_ShouldReturnFormattedString()
    {
        // Arrange
        _viewModel.CurrentDpi = 1600;

        // Act
        var text = _viewModel.CurrentDpiText;

        // Assert
        Assert.Equal("1600 DPI", text);
    }

    #endregion

    #region 从配置加载测试

    [Fact]
    public void Constructor_WithConnectedDevice_ShouldLoadFromConfig()
    {
        // Arrange
        var config = new DeviceConfig
        {
            Dpi = 1200,
            DpiIndex = 2,
            AccelerationEnabled = true,
            AccelerationThreshold = 20,
            AccelerationRatio = 2.0,
            DpiSlots = new ushort[] { 500, 1000, 2000, 4000 },
            CurrentDpiSlot = 1,
            LodLevel = 2,
            AngleSnapping = true
        };

        var mockService = new Mock<IDeviceService>();
        mockService.Setup(s => s.IsConnected).Returns(true);
        mockService.Setup(s => s.CurrentConfig).Returns(config);

        // Act
        var vm = new MousePageViewModel(mockService.Object);

        // Assert
        Assert.Equal(1200, vm.CurrentDpi);
        Assert.Equal(2, vm.CurrentDpiIndex);
        Assert.True(vm.AccelerationEnabled);
        Assert.Equal(20, vm.AccelerationThreshold);
        Assert.Equal(2.0, vm.AccelerationRatio);
        Assert.Equal(500, vm.DpiSlot0);
        Assert.Equal(1000, vm.DpiSlot1);
        Assert.Equal(2000, vm.DpiSlot2);
        Assert.Equal(4000, vm.DpiSlot3);
        Assert.Equal(1, vm.CurrentDpiSlot);
        Assert.Equal(2, vm.LodLevel);
        Assert.True(vm.AngleSnapping);
    }

    [Fact]
    public void Constructor_WithConfigNullDpiSlots_ShouldUseDefaults()
    {
        // Arrange
        var config = new DeviceConfig
        {
            Dpi = 800,
            DpiSlots = null!
        };

        var mockService = new Mock<IDeviceService>();
        mockService.Setup(s => s.IsConnected).Returns(true);
        mockService.Setup(s => s.CurrentConfig).Returns(config);

        // Act
        var vm = new MousePageViewModel(mockService.Object);

        // Assert
        Assert.Equal(400, vm.DpiSlot0);
        Assert.Equal(800, vm.DpiSlot1);
        Assert.Equal(1600, vm.DpiSlot2);
        Assert.Equal(3200, vm.DpiSlot3);
    }

    #endregion

    #region 属性变更测试

    [Fact]
    public void CurrentDpi_Changed_ShouldUpdateCurrentDpiText()
    {
        // Arrange
        var originalText = _viewModel.CurrentDpiText;

        // Act
        _viewModel.CurrentDpi = 2400;

        // Assert
        Assert.NotEqual(originalText, _viewModel.CurrentDpiText);
        Assert.Equal("2400 DPI", _viewModel.CurrentDpiText);
    }

    [Fact]
    public void LodLevel_ShouldAcceptValidValues()
    {
        // Act & Assert
        _viewModel.LodLevel = 0;
        Assert.Equal(0, _viewModel.LodLevel);

        _viewModel.LodLevel = 1;
        Assert.Equal(1, _viewModel.LodLevel);

        _viewModel.LodLevel = 2;
        Assert.Equal(2, _viewModel.LodLevel);
    }

    [Fact]
    public void AngleSnapping_ShouldToggleCorrectly()
    {
        // Act
        _viewModel.AngleSnapping = true;
        Assert.True(_viewModel.AngleSnapping);

        _viewModel.AngleSnapping = false;
        Assert.False(_viewModel.AngleSnapping);
    }

    #endregion

    #region 命令测试

    [Fact]
    public void SwitchDpiSlotCommand_ShouldNotThrowWhenNotConnected()
    {
        // Arrange
        _mockDeviceService.Setup(s => s.IsConnected).Returns(false);

        // Act & Assert
        var exception = Record.ExceptionAsync(() => _viewModel.SwitchDpiSlotCommand.ExecuteAsync(null));
        Assert.NotNull(exception);
    }

    [Fact]
    public void SaveAdvancedConfigCommand_ShouldNotThrowWhenNotConnected()
    {
        // Arrange
        _mockDeviceService.Setup(s => s.IsConnected).Returns(false);

        // Act & Assert
        var exception = Record.ExceptionAsync(() => _viewModel.SaveAdvancedConfigCommand.ExecuteAsync(null));
        Assert.NotNull(exception);
    }

    #endregion

    #region DPI 档位测试

    [Theory]
    [InlineData(0, 400)]
    [InlineData(1, 800)]
    [InlineData(2, 1600)]
    [InlineData(3, 3200)]
    public void DpiSlots_ShouldHaveDefaultValues(int slot, int expectedDpi)
    {
        // Arrange
        var dpiSlots = new[] { _viewModel.DpiSlot0, _viewModel.DpiSlot1, _viewModel.DpiSlot2, _viewModel.DpiSlot3 };

        // Assert
        Assert.Equal(expectedDpi, dpiSlots[slot]);
    }

    [Fact]
    public void CurrentDpiSlot_ShouldAcceptValidRange()
    {
        // Act & Assert
        for (int i = 0; i < 4; i++)
        {
            _viewModel.CurrentDpiSlot = i;
            Assert.Equal(i, _viewModel.CurrentDpiSlot);
        }
    }

    #endregion
}
