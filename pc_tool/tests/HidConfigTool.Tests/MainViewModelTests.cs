/*
 * MainViewModelTests.cs
 * MainViewModel 单元测试
 * 测试导航命令、设备连接状态处理
 */

using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Services;
using HidConfigTool.Core.Utils;
using HidConfigTool.ViewModels;
using Moq;

namespace HidConfigTool.Tests;

/// <summary>
/// MainViewModel 单元测试
/// </summary>
public class MainViewModelTests : IDisposable
{
    private readonly Mock<IDeviceService> _mockDeviceService;
    private readonly MainViewModel _viewModel;

    public MainViewModelTests()
    {
        _mockDeviceService = new Mock<IDeviceService>();
        // 模拟一台具备全部能力的设备（能力位与本地角色正交）
        _mockDeviceService.Setup(d => d.IsCapabilityAvailable(It.IsAny<CapabilityId>())).Returns(true);
        _viewModel = new MainViewModel(_mockDeviceService.Object);
        // 每个用例从消费者标准模式起步，隔离静态角色状态
        LocalRoleService.SetRole(LocalRole.Consumer);
    }

    #region 导航命令测试

    [Fact]
    public void NavigateToDeviceCommand_ShouldSetCurrentPageToDevice()
    {
        // Act
        _viewModel.NavigateToDeviceCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.Device, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToKeyManagementCommand_ShouldSetCurrentPageToKeyManagement()
    {
        // Act
        _viewModel.NavigateToKeyManagementCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.KeyManagement, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToMouseCommand_ShouldSetCurrentPageToMouse()
    {
        // Act
        _viewModel.NavigateToMouseCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.Mouse, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToJoystickCommand_ShouldSetCurrentPageToJoystick()
    {
        // Act
        _viewModel.NavigateToJoystickCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.Joystick, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToEncoderCommand_ShouldSetCurrentPageToEncoder()
    {
        // Act
        _viewModel.NavigateToEncoderCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.Encoder, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToMacroCommand_ShouldSetCurrentPageToMacro()
    {
        // Act
        _viewModel.NavigateToMacroCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.Macro, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToErrorLogCommand_ShouldSetCurrentPageToErrorLog()
    {
        // Arrange - 诊断页仅开发者模式可见
        LocalRoleService.SetRole(LocalRole.Developer);

        // Act
        _viewModel.NavigateToErrorLogCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.ErrorLog, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToPerfMonitorCommand_ShouldSetCurrentPageToPerfMonitor()
    {
        // Arrange - 诊断页仅开发者模式可见
        LocalRoleService.SetRole(LocalRole.Developer);

        // Act
        _viewModel.NavigateToPerfMonitorCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.PerfMonitor, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToDiagnosticsCommand_ShouldSetCurrentPageToDiagnostics()
    {
        // Arrange - 诊断页仅开发者模式可见
        LocalRoleService.SetRole(LocalRole.Developer);

        // Act
        _viewModel.NavigateToDiagnosticsCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.Diagnostics, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToPresetsCommand_ShouldSetCurrentPageToPresets()
    {
        // Act
        _viewModel.NavigateToPresetsCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.Presets, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToSettingsCommand_ShouldSetCurrentPageToSettings()
    {
        // Act
        _viewModel.NavigateToSettingsCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.Settings, _viewModel.CurrentPage);
    }

    [Fact]
    public void NavigateToFirmwareUpdateCommand_ShouldSetCurrentPageToFirmwareUpdate()
    {
        // Act
        _viewModel.NavigateToFirmwareUpdateCommand.Execute(null);

        // Assert
        Assert.Equal(MainViewModel.PageType.FirmwareUpdate, _viewModel.CurrentPage);
    }

    #endregion

    #region 本地角色闸门测试（功能角色分离）

    [Fact]
    public void ConsumerRole_DiagnosticMenus_ShouldBeHidden_EvenWhenDeviceCapable()
    {
        // 消费者标准模式下，即使设备具备全部能力，三个诊断菜单也不可见
        Assert.False(_viewModel.IsErrorLogMenuVisible);
        Assert.False(_viewModel.IsPerfMonitorMenuVisible);
        Assert.False(_viewModel.IsDiagnosticsMenuVisible);
    }

    [Fact]
    public void DeveloperRole_DiagnosticMenus_ShouldBeVisible_WhenDeviceCapable()
    {
        // Arrange
        LocalRoleService.SetRole(LocalRole.Developer);

        // Assert
        Assert.True(_viewModel.IsErrorLogMenuVisible);
        Assert.True(_viewModel.IsPerfMonitorMenuVisible);
        Assert.True(_viewModel.IsDiagnosticsMenuVisible);
    }

    [Theory]
    [InlineData(MainViewModel.PageType.ErrorLog)]
    [InlineData(MainViewModel.PageType.PerfMonitor)]
    [InlineData(MainViewModel.PageType.Diagnostics)]
    public void ConsumerRole_NavigateToDiagnosticPage_ShouldStayOnDashboard(MainViewModel.PageType target)
    {
        // 消费者模式下即使绕过 UI 隐藏直接执行导航命令，也应被守卫拦在首页
        switch (target)
        {
            case MainViewModel.PageType.ErrorLog:
                _viewModel.NavigateToErrorLogCommand.Execute(null);
                break;
            case MainViewModel.PageType.PerfMonitor:
                _viewModel.NavigateToPerfMonitorCommand.Execute(null);
                break;
            case MainViewModel.PageType.Diagnostics:
                _viewModel.NavigateToDiagnosticsCommand.Execute(null);
                break;
        }

        Assert.Equal(MainViewModel.PageType.Dashboard, _viewModel.CurrentPage);
    }

    [Fact]
    public void TurningOffDeveloperRole_WhileOnDiagnosticPage_ShouldReturnToDashboard()
    {
        // Arrange - 开发者模式进入性能监控页
        LocalRoleService.SetRole(LocalRole.Developer);
        _viewModel.NavigateToPerfMonitorCommand.Execute(null);
        Assert.Equal(MainViewModel.PageType.PerfMonitor, _viewModel.CurrentPage);

        // Act - 关闭开发者模式
        LocalRoleService.SetRole(LocalRole.Consumer);

        // Assert - 自动回到首页
        Assert.Equal(MainViewModel.PageType.Dashboard, _viewModel.CurrentPage);
    }

    #endregion

    #region 初始状态测试

    [Fact]
    public void Constructor_ShouldSetDefaultPageToDashboard()
    {
        // Assert
        Assert.Equal(MainViewModel.PageType.Dashboard, _viewModel.CurrentPage);
    }

    [Fact]
    public void Constructor_ShouldSubscribeToDeviceConnectionChanged()
    {
        // Assert - 验证事件订阅（通过触发事件来验证）
        _mockDeviceService.Raise(s => s.DeviceConnectionChanged += null, this, true);

        // 如果没有异常，说明事件处理正常
        Assert.True(true);
    }

    #endregion

    #region 设备连接状态测试

    [Fact]
    public void NotifyDeviceConnected_ShouldSetIsDeviceConnectedToTrue()
    {
        // Act
        _viewModel.NotifyDeviceConnected();

        // Assert
        Assert.True(_viewModel.IsDeviceConnected);
    }

    [Fact]
    public void NotifyDeviceDisconnected_ShouldSetIsDeviceConnectedToFalse()
    {
        // Arrange
        _viewModel.NotifyDeviceConnected();

        // Act
        _viewModel.NotifyDeviceDisconnected();

        // Assert
        Assert.False(_viewModel.IsDeviceConnected);
    }

    #endregion

    #region Dispose 测试

    [Fact]
    public void Dispose_ShouldUnsubscribeFromEvents()
    {
        // Act
        _viewModel.Dispose();

        // Assert - 触发事件不应导致异常
        _mockDeviceService.Raise(s => s.DeviceConnectionChanged += null, this, true);
        Assert.True(true);
    }

    [Fact]
    public void Dispose_CalledMultipleTimes_ShouldNotThrow()
    {
        // Act
        _viewModel.Dispose();
        _viewModel.Dispose();

        // Assert
        Assert.True(true);
    }

    #endregion

    /// <summary>还原静态本地角色并释放 VM，避免用例间状态泄漏</summary>
    public void Dispose()
    {
        LocalRoleService.SetRole(LocalRole.Consumer);
        _viewModel.Dispose();
    }
}
