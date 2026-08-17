using Avalonia.Controls;
using HidConfigTool.ViewModels;
using HidConfigTool.Desktop.Views.Pages;
using Microsoft.Extensions.DependencyInjection;

namespace HidConfigTool.Desktop.Views;

public partial class MainWindow : Window
{
    private readonly DevicePage _devicePage;
    private readonly KeyPage _keyPage;
    private readonly MousePage _mousePage;
    private readonly JoystickPage _joystickPage;
    private readonly EncoderPage _encoderPage;
    private readonly MacroPage _macroPage;
    private readonly StatsPage _statsPage;
    private readonly PerfPage _perfPage;
    private readonly ErrorLogPage _errorLogPage;
    private readonly SettingsPage _settingsPage;

    public MainWindow()
    {
        InitializeComponent();
    }

    public MainWindow(MainViewModel viewModel) : this()
    {
        DataContext = viewModel;

        // 从 DI 容器获取各个页面
        var services = App.Services;
        _devicePage = new DevicePage { DataContext = services.GetRequiredService<DevicePageViewModel>() };
        _keyPage = new KeyPage { DataContext = services.GetRequiredService<KeyPageViewModel>() };
        _mousePage = new MousePage { DataContext = services.GetRequiredService<MousePageViewModel>() };
        _joystickPage = new JoystickPage { DataContext = services.GetRequiredService<JoystickPageViewModel>() };
        _encoderPage = new EncoderPage { DataContext = services.GetRequiredService<EncoderPageViewModel>() };
        _macroPage = new MacroPage { DataContext = services.GetRequiredService<MacroPageViewModel>() };
        _statsPage = new StatsPage { DataContext = services.GetRequiredService<StatsPageViewModel>() };
        _perfPage = new PerfPage { DataContext = services.GetRequiredService<PerfMonitorPageViewModel>() };
        _errorLogPage = new ErrorLogPage { DataContext = services.GetRequiredService<ErrorLogPageViewModel>() };
        _settingsPage = new SettingsPage { DataContext = services.GetRequiredService<SettingsPageViewModel>() };

        viewModel.PropertyChanged += (s, e) =>
        {
            if (e.PropertyName == nameof(MainViewModel.CurrentPage))
                UpdateCurrentPage();
        };

        UpdateCurrentPage();
    }

    private void UpdateCurrentPage()
    {
        if (DataContext is not MainViewModel vm) return;
        if (PageContent == null) return;

        PageContent.Content = vm.CurrentPage switch
        {
            MainViewModel.PageType.Device => _devicePage,
            MainViewModel.PageType.KeyManagement => _keyPage,
            MainViewModel.PageType.Mouse => _mousePage,
            MainViewModel.PageType.Joystick => _joystickPage,
            MainViewModel.PageType.Encoder => _encoderPage,
            MainViewModel.PageType.Macro => _macroPage,
            MainViewModel.PageType.ErrorLog => _errorLogPage,
            MainViewModel.PageType.PerfMonitor => _perfPage,
            MainViewModel.PageType.Settings => _settingsPage,
            _ => _devicePage
        };
    }
}
