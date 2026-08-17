using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Animation;
using System.Windows.Interop;
using System.Runtime.InteropServices;
using HidConfigTool.ViewModels;
using HidConfigTool.App.Views;
using Microsoft.Extensions.DependencyInjection;
using HidConfigTool.App.Services;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App;

/// <summary>
/// 主窗口
/// </summary>
public partial class MainWindow : Window
{
    private readonly MainViewModel _viewModel;
    private readonly IServiceProvider _serviceProvider;
    private readonly IDeviceService _deviceService;
    private HwndSource? _hwndSource;
    private IntPtr _deviceNotificationHandle = IntPtr.Zero;

    // Windows 消息常量
    private const int WM_DEVICECHANGE = 0x0219;
    private const int DBT_DEVICEARRIVAL = 0x8000;       // 设备到达
    private const int DBT_DEVICEREMOVECOMPLETE = 0x8004; // 设备移除
    private const int DBT_DEVTYP_DEVICEINTERFACE = 0x00000005;
    private const uint DEVICE_NOTIFY_WINDOW_HANDLE = 0x00000000;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
    private struct DEV_BROADCAST_DEVICEINTERFACE
    {
        public int dbcc_size;
        public int dbcc_devicetype;
        public int dbcc_reserved;
        public Guid dbcc_classguid;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string dbcc_name;
    }

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern IntPtr RegisterDeviceNotification(IntPtr hRecipient, ref DEV_BROADCAST_DEVICEINTERFACE NotificationFilter, uint Flags);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnregisterDeviceNotification(IntPtr Handle);

    [DllImport("hid.dll", SetLastError = true)]
    private static extern void HidD_GetHidGuid(out Guid HidGuid);

    public MainWindow(MainViewModel viewModel, IServiceProvider serviceProvider)
    {
        InitializeComponent();

        _viewModel = viewModel;
        _serviceProvider = serviceProvider;
        _deviceService = serviceProvider.GetRequiredService<IDeviceService>();
        DataContext = _viewModel;

        // 监听页面变化
        _viewModel.PropertyChanged += (s, e) =>
        {
            if (e.PropertyName == nameof(MainViewModel.CurrentPage))
            {
                NavigateToPage(_viewModel.CurrentPage);
            }
        };

        // 初始页面
        Loaded += (s, e) => NavigateToPage(_viewModel.CurrentPage);

        // 启动自动连接
        _deviceService.StartAutoConnect();
    }

    protected override void OnSourceInitialized(EventArgs e)
    {
        base.OnSourceInitialized(e);

        // 添加 Windows 消息钩子
        _hwndSource = HwndSource.FromHwnd(new WindowInteropHelper(this).Handle);
        _hwndSource?.AddHook(WndProc);

        // 注册 HID 设备通知，只接收 HID 设备的插拔事件
        try
        {
            HidD_GetHidGuid(out Guid hidGuid);
            var filter = new DEV_BROADCAST_DEVICEINTERFACE
            {
                dbcc_size = Marshal.SizeOf<DEV_BROADCAST_DEVICEINTERFACE>(),
                dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE,
                dbcc_classguid = hidGuid,
                dbcc_name = string.Empty
            };
            _deviceNotificationHandle = RegisterDeviceNotification(
                new WindowInteropHelper(this).Handle,
                ref filter,
                DEVICE_NOTIFY_WINDOW_HANDLE);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"注册设备通知失败: {ex.Message}");
        }
    }

    protected override void OnClosed(EventArgs e)
    {
        // 注销设备通知
        if (_deviceNotificationHandle != IntPtr.Zero)
        {
            UnregisterDeviceNotification(_deviceNotificationHandle);
            _deviceNotificationHandle = IntPtr.Zero;
        }

        // 移除 Windows 消息钩子，避免内存泄漏
        _hwndSource?.RemoveHook(WndProc);
        _hwndSource = null;

        // 清理当前页面的资源
        if (ContentFrame.Content is FrameworkElement page)
        {
            if (page is IDisposable pageDisp)
            {
                pageDisp.Dispose();
            }
            else if (page.DataContext is IDisposable disposable)
            {
                disposable.Dispose();
            }
        }

        // 清理 MainViewModel
        _viewModel.Dispose();

        base.OnClosed(e);
    }

    /// <summary>
    /// Windows 消息处理
    /// </summary>
    private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (msg == WM_DEVICECHANGE)
        {
            int eventType = wParam.ToInt32();

            if (eventType == DBT_DEVICEARRIVAL)
            {
                // 设备插入：通知 DeviceService，延迟 500ms 后自动连接
                _deviceService.NotifyDevicePluggedIn();
            }
            else if (eventType == DBT_DEVICEREMOVECOMPLETE)
            {
                // 设备移除：心跳检测会自动处理，这里不用额外操作
            }
        }

        return IntPtr.Zero;
    }

    private void NavigateToPage(MainViewModel.PageType page)
    {
        // 清理旧页面的资源，防止内存泄漏
        if (ContentFrame.Content is FrameworkElement oldPage)
        {
            // 页面自身实现了 IDisposable（如容器页面）
            if (oldPage is IDisposable pageDisposable)
            {
                pageDisposable.Dispose();
            }
            // 页面的 DataContext 实现了 IDisposable
            else if (oldPage.DataContext is IDisposable oldDisposable)
            {
                oldDisposable.Dispose();
            }
        }

        FrameworkElement? newPage = page switch
        {
            MainViewModel.PageType.Device => _serviceProvider.GetRequiredService<DevicePage>(),
            MainViewModel.PageType.KeyManagement => _serviceProvider.GetRequiredService<KeyManagementPage>(),
            MainViewModel.PageType.Mouse => _serviceProvider.GetRequiredService<MousePage>(),
            MainViewModel.PageType.Joystick => _serviceProvider.GetRequiredService<JoystickPage>(),
            MainViewModel.PageType.Encoder => _serviceProvider.GetRequiredService<EncoderPage>(),
            MainViewModel.PageType.Macro => _serviceProvider.GetRequiredService<MacroPage>(),
            MainViewModel.PageType.ErrorLog => _serviceProvider.GetRequiredService<ErrorLogPage>(),
            MainViewModel.PageType.PerfMonitor => _serviceProvider.GetRequiredService<PerfMonitorPage>(),
            MainViewModel.PageType.Settings => _serviceProvider.GetRequiredService<SettingsPage>(),
            _ => null
        };

        if (newPage != null)
        {
            ContentFrame.Content = newPage;

            // 淡入动画
            var fadeIn = new DoubleAnimation
            {
                From = 0,
                To = 1,
                Duration = TimeSpan.FromMilliseconds(200)
            };
            newPage.BeginAnimation(OpacityProperty, fadeIn);
        }
    }

    #region 窗口控制

    private void MinimizeButton_Click(object sender, RoutedEventArgs e)
    {
        WindowState = WindowState.Minimized;
    }

    private void MaximizeButton_Click(object sender, RoutedEventArgs e)
    {
        ToggleMaximize();
    }

    private void CloseButton_Click(object sender, RoutedEventArgs e)
    {
        Close();
    }

    private void ToggleMaximize()
    {
        if (WindowState == WindowState.Maximized)
        {
            WindowState = WindowState.Normal;
        }
        else
        {
            WindowState = WindowState.Maximized;
        }
    }

    #endregion
}

