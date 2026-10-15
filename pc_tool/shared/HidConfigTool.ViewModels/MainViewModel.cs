using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Services;
using HidConfigTool.Core.Utils;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 主窗口视图模型
/// </summary>
public partial class MainViewModel : PageViewModelBase
{
    /// <summary>
    /// 页面类型枚举
    /// </summary>
    public enum PageType
    {
        Dashboard,      // 首页仪表盘
        Device,
        KeyManagement,
        Mouse,
        Joystick,
        Encoder,
        Macro,
        ErrorLog,
        PerfMonitor,
        Diagnostics,
        Presets,
        Settings,
        FirmwareUpdate
    }

    [ObservableProperty]
    private PageType _currentPage = PageType.Dashboard;

    [ObservableProperty]
    private string _windowTitle = "HID 配置工具 v0.1";

    /// <summary>
    /// 兼容性检查结果
    /// </summary>
    [ObservableProperty]
    private CompatibilityResult _compatibility = CompatibilityResult.Unknown;

    /// <summary>
    /// 设备版本信息
    /// </summary>
    [ObservableProperty]
    private DeviceVersionInfo? _versionInfo;

    /// <summary>
    /// 版本检查提示消息
    /// </summary>
    [ObservableProperty]
    private string _versionMessage = string.Empty;

    /// <summary>
    /// 是否显示版本警告
    /// </summary>
    [ObservableProperty]
    private bool _showVersionWarning;

    #region 能力状态（UI 层动态调整）

    /// <summary>
    /// 检查指定能力是否可用
    /// </summary>
    public bool IsCapabilityAvailable(CapabilityId capabilityId) =>
        DeviceService.IsCapabilityAvailable(capabilityId);

    /// <summary>键盘功能是否可用</summary>
    public bool IsKeyboardAvailable => IsCapabilityAvailable(CapabilityId.KeyboardBasic);
    /// <summary>鼠标功能是否可用</summary>
    public bool IsMouseAvailable => IsCapabilityAvailable(CapabilityId.MouseBasic);
    /// <summary>摇杆功能是否可用</summary>
    public bool IsJoystickAvailable => IsCapabilityAvailable(CapabilityId.Joystick);
    /// <summary>编码器功能是否可用</summary>
    public bool IsEncoderAvailable => IsCapabilityAvailable(CapabilityId.Encoder);
    /// <summary>按键映射功能是否可用</summary>
    public bool IsKeymapAvailable => IsCapabilityAvailable(CapabilityId.Keymap);
    /// <summary>宏功能是否可用</summary>
    public bool IsMacroAvailable => IsCapabilityAvailable(CapabilityId.Macro);
    /// <summary>性能监控是否可用</summary>
    public bool IsPerfMonitorAvailable => IsCapabilityAvailable(CapabilityId.PerfMonitor);
    /// <summary>错误日志是否可用</summary>
    public bool IsFaultLogAvailable => IsCapabilityAvailable(CapabilityId.FaultLog);
    /// <summary>DFU升级是否可用</summary>
    public bool IsDfuUpgradeAvailable => IsCapabilityAvailable(CapabilityId.DfuUpgrade);
    /// <summary>NKRO是否可用</summary>
    public bool IsNkroAvailable => IsCapabilityAvailable(CapabilityId.Nkro);
    /// <summary>DPI档位是否可用</summary>
    public bool IsDpiSlotsAvailable => IsCapabilityAvailable(CapabilityId.DpiSlots);
    /// <summary>摇杆校准是否可用</summary>
    public bool IsJoystickCalibAvailable => IsCapabilityAvailable(CapabilityId.JoystickCalib);

    #region 本地角色（消费者 / 开发者模式，与设备能力位图正交）

    /// <summary>是否处于开发者模式（纯本地设置并持久化，见 LocalRoleService）</summary>
    [ObservableProperty]
    private bool _isDeveloperMode = LocalRoleService.IsDeveloper;

    /// <summary>错误日志菜单可见：开发者模式 且 设备具备 FaultLog 能力</summary>
    public bool IsErrorLogMenuVisible => IsDeveloperMode && IsFaultLogAvailable;

    /// <summary>性能监控菜单可见：开发者模式 且 设备具备 PerfMonitor 能力</summary>
    public bool IsPerfMonitorMenuVisible => IsDeveloperMode && IsPerfMonitorAvailable;

    /// <summary>设备诊断菜单可见：开发者模式 且 设备具备任一诊断能力</summary>
    public bool IsDiagnosticsMenuVisible => IsDeveloperMode &&
        (IsCapabilityAvailable(CapabilityId.KeyStats) ||
         IsCapabilityAvailable(CapabilityId.PerfMonitor) ||
         IsCapabilityAvailable(CapabilityId.FaultLog) ||
         IsCapabilityAvailable(CapabilityId.DiagRawState));

    /// <summary>本地角色变化时刷新诊断菜单显隐；关闭开发者模式则离开诊断页</summary>
    private void OnLocalRoleChanged()
    {
        _isDeveloperMode = LocalRoleService.IsDeveloper;
        OnPropertyChanged(nameof(IsDeveloperMode));
        OnPropertyChanged(nameof(IsErrorLogMenuVisible));
        OnPropertyChanged(nameof(IsPerfMonitorMenuVisible));
        OnPropertyChanged(nameof(IsDiagnosticsMenuVisible));

        if (!_isDeveloperMode &&
            (CurrentPage == PageType.ErrorLog ||
             CurrentPage == PageType.PerfMonitor ||
             CurrentPage == PageType.Diagnostics))
        {
            CurrentPage = PageType.Dashboard;
        }
    }

    #endregion

    /// <summary>
    /// 当版本信息变化时，通知所有能力属性刷新
    /// </summary>
    partial void OnVersionInfoChanged(DeviceVersionInfo? value)
    {
        // 通知所有能力属性变化，UI 会重新绑定
        OnPropertyChanged(nameof(IsKeyboardAvailable));
        OnPropertyChanged(nameof(IsMouseAvailable));
        OnPropertyChanged(nameof(IsJoystickAvailable));
        OnPropertyChanged(nameof(IsEncoderAvailable));
        OnPropertyChanged(nameof(IsKeymapAvailable));
        OnPropertyChanged(nameof(IsMacroAvailable));
        OnPropertyChanged(nameof(IsPerfMonitorAvailable));
        OnPropertyChanged(nameof(IsFaultLogAvailable));
        OnPropertyChanged(nameof(IsDfuUpgradeAvailable));
        OnPropertyChanged(nameof(IsNkroAvailable));
        OnPropertyChanged(nameof(IsDpiSlotsAvailable));
        OnPropertyChanged(nameof(IsJoystickCalibAvailable));
        OnPropertyChanged(nameof(IsErrorLogMenuVisible));
        OnPropertyChanged(nameof(IsPerfMonitorMenuVisible));
        OnPropertyChanged(nameof(IsDiagnosticsMenuVisible));
    }

    #endregion

    public MainViewModel(IDeviceService deviceService)
        : base(deviceService)
    {
        StatusMessage = "就绪";
        Compatibility = DeviceService.Compatibility;
        VersionInfo = DeviceService.VersionInfo;

        // 订阅操作状态变化事件
        DeviceService.OperationStatusChanged += OnOperationStatusChanged;

        // 订阅版本检查完成事件
        DeviceService.VersionCheckCompleted += OnVersionCheckCompleted;

        // 订阅本地角色变化（开发者模式开关）
        LocalRoleService.Changed += OnLocalRoleChanged;
    }

    /// <summary>
    /// 设备已连接（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceConnectedAsync()
    {
        StatusMessage = "设备已连接";
        return Task.CompletedTask;
    }

    /// <summary>
    /// 设备已断开（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceDisconnectedAsync()
    {
        StatusMessage = "设备已断开";
        // 设备断开时重置版本信息
        Compatibility = CompatibilityResult.Unknown;
        VersionInfo = null;
        VersionMessage = string.Empty;
        ShowVersionWarning = false;
        return Task.CompletedTask;
    }

    /// <summary>
    /// 版本检查完成处理
    /// </summary>
    private void OnVersionCheckCompleted(object? sender, CompatibilityResult result)
    {
        Compatibility = result;
        VersionInfo = DeviceService.VersionInfo;
        VersionMessage = VersionComparer.GetCompatibilityMessage(result, VersionInfo);

        // 根据兼容性结果决定是否显示警告
        ShowVersionWarning = result == CompatibilityResult.ProtocolIncompatible
                           || result == CompatibilityResult.FirmwareOutdated;

        // 更新状态栏消息
        if (result == CompatibilityResult.ProtocolIncompatible)
        {
            StatusMessage = $"⚠️ {VersionMessage}";
        }
        else if (result == CompatibilityResult.FirmwareOutdated)
        {
            StatusMessage = $"💡 {VersionMessage}";
        }
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
        IsDeviceConnected = true;
        StatusMessage = "设备已连接";
    }

    /// <summary>
    /// 通知设备已断开
    /// </summary>
    public void NotifyDeviceDisconnected()
    {
        IsDeviceConnected = false;
        StatusMessage = "设备已断开";
    }

    [RelayCommand]
    private void NavigateToDashboard()
    {
        CurrentPage = PageType.Dashboard;
    }

    [RelayCommand]
    private void NavigateToDevice()
    {
        CurrentPage = PageType.Device;
    }

    [RelayCommand]
    private void NavigateToKeyManagement()
    {
        CurrentPage = PageType.KeyManagement;
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
        if (!IsErrorLogMenuVisible) return; // 角色/能力闸门：标准模式或设备无此能力时禁止进入
        CurrentPage = PageType.ErrorLog;
    }

    [RelayCommand]
    private void NavigateToPerfMonitor()
    {
        if (!IsPerfMonitorMenuVisible) return;
        CurrentPage = PageType.PerfMonitor;
    }

    [RelayCommand]
    private void NavigateToDiagnostics()
    {
        if (!IsDiagnosticsMenuVisible) return;
        CurrentPage = PageType.Diagnostics;
    }

    [RelayCommand]
    private void NavigateToPresets()
    {
        CurrentPage = PageType.Presets;
    }

    [RelayCommand]
    private void NavigateToSettings()
    {
        CurrentPage = PageType.Settings;
    }

    [RelayCommand]
    private void NavigateToFirmwareUpdate()
    {
        CurrentPage = PageType.FirmwareUpdate;
    }

    /// <summary>
    /// 释放资源，取消事件订阅
    /// </summary>
    /// <summary>
    /// 释放子类特有资源（重写基类虚方法）
    /// </summary>
    protected override void DisposeManagedResources()
    {
        DeviceService.OperationStatusChanged -= OnOperationStatusChanged;
        DeviceService.VersionCheckCompleted -= OnVersionCheckCompleted;
        LocalRoleService.Changed -= OnLocalRoleChanged;
    }
}


