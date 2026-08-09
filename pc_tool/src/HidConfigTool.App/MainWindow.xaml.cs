using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Animation;
using System.Windows.Interop;
using System.Runtime.InteropServices;
using HidConfigTool.App.ViewModels;
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

    // Windows 消息常量
    private const int WM_DEVICECHANGE = 0x0219;
    private const int DBT_DEVICEARRIVAL = 0x8000;       // 设备到达
    private const int DBT_DEVICEREMOVECOMPLETE = 0x8004; // 设备移除

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
        var source = HwndSource.FromHwnd(new WindowInteropHelper(this).Handle);
        source?.AddHook(WndProc);
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
        FrameworkElement? newPage = page switch
        {
            MainViewModel.PageType.Device => _serviceProvider.GetRequiredService<DevicePage>(),
            MainViewModel.PageType.Keys => _serviceProvider.GetRequiredService<KeyPage>(),
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

    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ClickCount == 2)
        {
            ToggleMaximize();
        }
        else
        {
            DragMove();
        }
    }

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
            BorderBrush = (System.Windows.Media.Brush)FindResource("BorderBrush");
            BorderThickness = new Thickness(1);
            Margin = new Thickness(0);
        }
        else
        {
            WindowState = WindowState.Maximized;
            BorderThickness = new Thickness(8);
        }
    }

    #endregion
}

