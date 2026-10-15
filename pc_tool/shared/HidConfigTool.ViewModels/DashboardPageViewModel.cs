using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Utils;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 首页仪表盘 ViewModel
/// 参考 Logitech G HUB / Razer Synapse 的首页设计
/// 提供设备概览、快速设置、固件更新提示、系统状态等
/// </summary>
public partial class DashboardPageViewModel : PageViewModelBase
{
    private readonly IFirmwareUpdateService _firmwareUpdateService;

    /* ==================== 设备概览 ==================== */

    /// <summary>当前固件版本</summary>
    [ObservableProperty]
    private string? _firmwareVersion;

    /// <summary>当前 DPI</summary>
    [ObservableProperty]
    private int _currentDpi;

    /// <summary>当前按键层</summary>
    [ObservableProperty]
    private int _currentLayer;

    /// <summary>设备名称</summary>
    [ObservableProperty]
    private string _deviceName = "Pico HID Composite";

    /* ==================== 固件更新 ==================== */

    /// <summary>是否有可用更新</summary>
    [ObservableProperty]
    private bool _isUpdateAvailable;

    /// <summary>最新版本字符串</summary>
    [ObservableProperty]
    private string? _latestVersion;

    /// <summary>是否正在检查更新</summary>
    [ObservableProperty]
    private bool _isCheckingUpdate;

    /* ==================== 系统状态 ==================== */

    /// <summary>错误日志数量</summary>
    [ObservableProperty]
    private int _errorCount;

    /// <summary>性能监控是否启用</summary>
    [ObservableProperty]
    private bool _perfMonitorEnabled;

    /// <summary>CPU 使用率（模拟）</summary>
    [ObservableProperty]
    private int _cpuUsage;

    /* ==================== 快速操作命令 ==================== */

    public ICommand NavigateToDeviceCommand { get; }
    public ICommand NavigateToMouseCommand { get; }
    public ICommand NavigateToKeyboardCommand { get; }
    public ICommand NavigateToJoystickCommand { get; }
    public ICommand NavigateToMacroCommand { get; }
    public ICommand NavigateToFirmwareUpdateCommand { get; }
    public ICommand NavigateToErrorLogCommand { get; }
    public ICommand NavigateToPerfMonitorCommand { get; }
    public ICommand CheckForUpdateCommand { get; }

    /// <summary>
    /// 导航到指定页面的事件（由 MainWindow 订阅）
    /// </summary>
    public event EventHandler<MainViewModel.PageType>? NavigateRequested;

    public DashboardPageViewModel(IDeviceService deviceService, IFirmwareUpdateService firmwareUpdateService)
        : base(deviceService)
    {
        _firmwareUpdateService = firmwareUpdateService;

        NavigateToDeviceCommand = new RelayCommand(() => NavigateRequested?.Invoke(this, MainViewModel.PageType.Device));
        NavigateToMouseCommand = new RelayCommand(() => NavigateRequested?.Invoke(this, MainViewModel.PageType.Mouse));
        NavigateToKeyboardCommand = new RelayCommand(() => NavigateRequested?.Invoke(this, MainViewModel.PageType.KeyManagement));
        NavigateToJoystickCommand = new RelayCommand(() => NavigateRequested?.Invoke(this, MainViewModel.PageType.Joystick));
        NavigateToMacroCommand = new RelayCommand(() => NavigateRequested?.Invoke(this, MainViewModel.PageType.Macro));
        NavigateToFirmwareUpdateCommand = new RelayCommand(() => NavigateRequested?.Invoke(this, MainViewModel.PageType.FirmwareUpdate));
        NavigateToErrorLogCommand = new RelayCommand(() => NavigateRequested?.Invoke(this, MainViewModel.PageType.ErrorLog));
        NavigateToPerfMonitorCommand = new RelayCommand(() => NavigateRequested?.Invoke(this, MainViewModel.PageType.PerfMonitor));
        CheckForUpdateCommand = new RelayCommand(async () => await CheckForUpdateAsync());

        // 能力位图在版本检查完成后才就绪：bit10=0（未编入 DFU bootloader）时不提示在线更新，
        // 由 VersionCheckCompleted 兜底判定，避免连接早期能力未知导致误提示。
        DeviceService.VersionCheckCompleted += OnVersionCheckCompleted;

        FirmwareVersion = DeviceService.FirmwareVersion;

        // 页面加载时自动检查更新
        if (IsDeviceConnected)
        {
            _ = CheckForUpdateAsync();
        }
    }

    /// <summary>
    /// 设备已连接（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceConnectedAsync()
    {
        FirmwareVersion = DeviceService.FirmwareVersion;
        // 连接后自动检查更新（若能力位图尚未就绪，守卫会跳过，由 VersionCheckCompleted 补检）
        _ = CheckForUpdateAsync();
        return Task.CompletedTask;
    }

    /// <summary>
    /// 设备信息/能力位图解析完成后兜底：仅当设备编入自定义 DFU bootloader（bit10）
    /// 才检查并提示在线更新；否则清除“固件更新可用”，消费者走 BOOTSEL UF2 手动升级。
    /// </summary>
    private void OnVersionCheckCompleted(object? sender, CompatibilityResult result)
    {
        if (DeviceService.IsCapabilityAvailable(CapabilityId.DfuUpgrade))
        {
            _ = CheckForUpdateAsync();
        }
        else
        {
            IsUpdateAvailable = false;
            LatestVersion = null;
        }
    }

    protected override void DisposeManagedResources()
    {
        DeviceService.VersionCheckCompleted -= OnVersionCheckCompleted;
        base.DisposeManagedResources();
    }

    /// <summary>
    /// 检查固件更新
    /// </summary>
    public async Task CheckForUpdateAsync()
    {
        if (IsCheckingUpdate || !IsDeviceConnected)
            return;

        // 未编入自定义 DFU bootloader（能力位 bit10=0）时不做在线更新检查/提示，
        // 避免出现“有新版本”但升级页无在线升级入口的误导；手动升级走 BOOTSEL UF2。
        if (!DeviceService.IsCapabilityAvailable(CapabilityId.DfuUpgrade))
        {
            IsUpdateAvailable = false;
            LatestVersion = null;
            return;
        }

        try
        {
            IsCheckingUpdate = true;

            // 解析当前固件版本
            uint currentVersion = ParseVersion(FirmwareVersion);

            // 检查最新版本
            var updateInfo = await _firmwareUpdateService.CheckForUpdateAsync(currentVersion);

            if (updateInfo != null)
            {
                IsUpdateAvailable = true;
                LatestVersion = updateInfo.VersionString;
            }
            else
            {
                IsUpdateAvailable = false;
                LatestVersion = null;
            }
        }
        catch
        {
            // 检查更新失败，静默处理
            IsUpdateAvailable = false;
        }
        finally
        {
            IsCheckingUpdate = false;
        }
    }

    /// <summary>
    /// 解析版本字符串为 uint
    /// </summary>
    private static uint ParseVersion(string? versionString)
    {
        if (string.IsNullOrWhiteSpace(versionString))
            return 0;

        try
        {
            string cleaned = versionString.TrimStart('v', 'V');
            var parts = cleaned.Split('.');
            if (parts.Length < 2)
                return 0;

            if (!uint.TryParse(parts[0], out uint major))
                return 0;
            if (!uint.TryParse(parts[1], out uint minor))
                return 0;

            return (major << 8) | (minor & 0xFF);
        }
        catch
        {
            return 0;
        }
    }
}
