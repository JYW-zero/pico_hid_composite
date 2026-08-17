using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Services;
using HidConfigTool.Desktop.Services;
using HidConfigTool.Desktop.Views;
using HidConfigTool.Hid;
using HidConfigTool.ViewModels;
using Microsoft.Extensions.DependencyInjection;

namespace HidConfigTool.Desktop;

public partial class App : Application
{
    public static IServiceProvider Services { get; private set; } = null!;

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        var collection = new ServiceCollection();

        // HID 驱动和设备服务
        collection.AddSingleton<IHidDriver, HidSharpDriver>();
        collection.AddSingleton<IDeviceService, DeviceService>();

        // 平台服务实现
        collection.AddSingleton<IDialogService, DialogService>();
        collection.AddSingleton<ITimerService, TimerService>();
        collection.AddSingleton<IUiThreadService, UiThreadService>();
        collection.AddSingleton<IFileDialogService, FileDialogService>();
        collection.AddSingleton<IInputDialogService, InputDialogService>();
        collection.AddSingleton<IKeyPickerService, KeyPickerService>();
        collection.AddSingleton<IHelpWindowService, HelpWindowService>();
        collection.AddSingleton<ITrayIconService, TrayIconService>();
        collection.AddSingleton<IThemeService, ThemeService>();
        collection.AddSingleton<ILanguageService, LanguageService>();
        collection.AddSingleton<IOsdService, OsdService>();
        collection.AddSingleton<IAppAwarenessService, AppAwarenessService>();
        collection.AddSingleton<IConfigProfileService, ConfigProfileService>();
        collection.AddSingleton<IAutoStartService, AutoStartService>();
        collection.AddSingleton<IMacroRecorder, MacroRecorderService>();

        // 共享 ViewModel（全部使用共享层）
        collection.AddSingleton<MainViewModel>();
        collection.AddSingleton<DevicePageViewModel>();
        collection.AddSingleton<KeyPageViewModel>();
        collection.AddSingleton<MousePageViewModel>();
        collection.AddSingleton<JoystickPageViewModel>();
        collection.AddSingleton<EncoderPageViewModel>();
        collection.AddSingleton<MacroPageViewModel>();
        collection.AddSingleton<StatsPageViewModel>();
        collection.AddSingleton<PerfMonitorPageViewModel>();
        collection.AddSingleton<ErrorLogPageViewModel>();
        collection.AddSingleton<SettingsPageViewModel>();
        collection.AddSingleton<KeyTestPageViewModel>();

        collection.AddSingleton<MainWindow>();
        Services = collection.BuildServiceProvider();

        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.MainWindow = Services.GetRequiredService<MainWindow>();
            desktop.Exit += (_, _) =>
            {
                Services.GetService<IDeviceService>()?.Dispose();
                Services.GetService<IHidDriver>()?.Dispose();
            };
        }

        base.OnFrameworkInitializationCompleted();
    }
}
