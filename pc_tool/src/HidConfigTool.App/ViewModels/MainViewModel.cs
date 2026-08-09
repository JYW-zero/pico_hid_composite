using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 主窗口视图模型
/// </summary>
public partial class MainViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;

    /// <summary>
    /// 页面类型枚举
    /// </summary>
    public enum PageType
    {
        Device,
        Keys,
        Mouse,
        Joystick,
        Encoder,
        Macro,
        ErrorLog,
        PerfMonitor,
        Settings
    }

    [ObservableProperty]
    private PageType _currentPage = PageType.Device;

    [ObservableProperty]
    private string _windowTitle = "HID 配置工具 v0.1";

    /// <summary>
    /// 状态栏消息
    /// </summary>
    [ObservableProperty]
    private string _statusMessage = "就绪";

    /// <summary>
    /// 设备是否已连接
    /// </summary>
    public bool IsDeviceConnected => _deviceService.IsConnected;

    public MainViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 订阅操作状态变化事件
        _deviceService.OperationStatusChanged += OnOperationStatusChanged;

        // 订阅设备连接状态变化事件
        _deviceService.DeviceConnectionChanged += OnDeviceConnectionChanged;
    }

    private void OnDeviceConnectionChanged(object? sender, bool isConnected)
    {
        OnPropertyChanged(nameof(IsDeviceConnected));
        StatusMessage = isConnected ? "设备已连接" : "设备已断开";
    }

    /// <summary>
    /// 操作状态变化处理
    /// </summary>
    private void OnOperationStatusChanged(object? sender, string status)
    {
        StatusMessage = status;
    }

    /// <summary>
    /// 通知设备已连接
    /// </summary>
    public void NotifyDeviceConnected()
    {
        OnPropertyChanged(nameof(IsDeviceConnected));
        StatusMessage = "设备已连接";
    }

    /// <summary>
    /// 通知设备已断开
    /// </summary>
    public void NotifyDeviceDisconnected()
    {
        OnPropertyChanged(nameof(IsDeviceConnected));
        StatusMessage = "设备已断开";
    }

    [RelayCommand]
    private void NavigateToDevice()
    {
        CurrentPage = PageType.Device;
    }

    [RelayCommand]
    private void NavigateToKeys()
    {
        CurrentPage = PageType.Keys;
    }

    [RelayCommand]
    private void NavigateToMouse()
    {
        CurrentPage = PageType.Mouse;
    }

    [RelayCommand]
    private void NavigateToJoystick()
    {
        CurrentPage = PageType.Joystick;
    }

    [RelayCommand]
    private void NavigateToEncoder()
    {
        CurrentPage = PageType.Encoder;
    }

    [RelayCommand]
    private void NavigateToMacro()
    {
        CurrentPage = PageType.Macro;
    }

    [RelayCommand]
    private void NavigateToErrorLog()
    {
        CurrentPage = PageType.ErrorLog;
    }

    [RelayCommand]
    private void NavigateToPerfMonitor()
    {
        CurrentPage = PageType.PerfMonitor;
    }

    [RelayCommand]
    private void NavigateToSettings()
    {
        CurrentPage = PageType.Settings;
    }
}


