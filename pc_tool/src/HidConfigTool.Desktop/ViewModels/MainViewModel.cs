using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.ViewModels;

public partial class MainViewModel : ObservableObject
{
    public enum PageType
    {
        Device, Keys, Mouse, Joystick, Encoder, Macro, Stats, Perf, ErrorLog, Settings
    }

    private readonly IDeviceService _deviceService;

    [ObservableProperty]
    private PageType _currentPage = PageType.Device;

    [ObservableProperty]
    private string _windowTitle = "HID 配置工具";

    [ObservableProperty]
    private string _statusMessage = "就绪";

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ConnectionText))]
    private bool _isDeviceConnected;

    public string ConnectionText => IsDeviceConnected ? "已连接" : "未连接";

    public DevicePageViewModel DevicePage { get; }
    public KeyPageViewModel KeyPage { get; }
    public MousePageViewModel MousePage { get; }
    public JoystickPageViewModel JoystickPage { get; }
    public EncoderPageViewModel EncoderPage { get; }
    public MacroPageViewModel MacroPage { get; }
    public StatsPageViewModel StatsPage { get; }
    public PerfPageViewModel PerfPage { get; }
    public ErrorLogPageViewModel ErrorLogPage { get; }
    public SettingsPageViewModel SettingsPage { get; }

    public bool IsDevicePage => CurrentPage == PageType.Device;
    public bool IsKeysPage => CurrentPage == PageType.Keys;
    public bool IsMousePage => CurrentPage == PageType.Mouse;
    public bool IsJoystickPage => CurrentPage == PageType.Joystick;
    public bool IsEncoderPage => CurrentPage == PageType.Encoder;
    public bool IsMacroPage => CurrentPage == PageType.Macro;
    public bool IsStatsPage => CurrentPage == PageType.Stats;
    public bool IsPerfPage => CurrentPage == PageType.Perf;
    public bool IsErrorLogPage => CurrentPage == PageType.ErrorLog;
    public bool IsSettingsPage => CurrentPage == PageType.Settings;

    partial void OnCurrentPageChanged(PageType value)
    {
        OnPropertyChanged(nameof(IsDevicePage));
        OnPropertyChanged(nameof(IsKeysPage));
        OnPropertyChanged(nameof(IsMousePage));
        OnPropertyChanged(nameof(IsJoystickPage));
        OnPropertyChanged(nameof(IsEncoderPage));
        OnPropertyChanged(nameof(IsMacroPage));
        OnPropertyChanged(nameof(IsStatsPage));
        OnPropertyChanged(nameof(IsPerfPage));
        OnPropertyChanged(nameof(IsErrorLogPage));
        OnPropertyChanged(nameof(IsSettingsPage));
    }

    public MainViewModel(
        IDeviceService deviceService,
        DevicePageViewModel devicePage,
        KeyPageViewModel keyPage,
        MousePageViewModel mousePage,
        JoystickPageViewModel joystickPage,
        EncoderPageViewModel encoderPage,
        MacroPageViewModel macroPage,
        StatsPageViewModel statsPage,
        PerfPageViewModel perfPage,
        ErrorLogPageViewModel errorLogPage,
        SettingsPageViewModel settingsPage)
    {
        _deviceService = deviceService;
        DevicePage = devicePage;
        KeyPage = keyPage;
        MousePage = mousePage;
        JoystickPage = joystickPage;
        EncoderPage = encoderPage;
        MacroPage = macroPage;
        StatsPage = statsPage;
        PerfPage = perfPage;
        ErrorLogPage = errorLogPage;
        SettingsPage = settingsPage;

        IsDeviceConnected = deviceService.IsConnected;
        deviceService.OperationStatusChanged += (_, status) => StatusMessage = status;
        deviceService.DeviceConnectionChanged += (_, connected) =>
        {
            IsDeviceConnected = connected;
            StatusMessage = connected ? "设备已连接" : "设备已断开";
        };
    }

    [RelayCommand] private void NavigateToDevice() => CurrentPage = PageType.Device;
    [RelayCommand] private void NavigateToKeys() => CurrentPage = PageType.Keys;
    [RelayCommand] private void NavigateToMouse() => CurrentPage = PageType.Mouse;
    [RelayCommand] private void NavigateToJoystick() => CurrentPage = PageType.Joystick;
    [RelayCommand] private void NavigateToEncoder() => CurrentPage = PageType.Encoder;
    [RelayCommand] private void NavigateToMacro() => CurrentPage = PageType.Macro;
    [RelayCommand] private void NavigateToStats() => CurrentPage = PageType.Stats;
    [RelayCommand] private void NavigateToPerf() => CurrentPage = PageType.Perf;
    [RelayCommand] private void NavigateToErrorLog() => CurrentPage = PageType.ErrorLog;
    [RelayCommand] private void NavigateToSettings() => CurrentPage = PageType.Settings;
}
