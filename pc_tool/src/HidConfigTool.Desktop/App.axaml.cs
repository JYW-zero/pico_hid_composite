using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Services;
using HidConfigTool.Desktop.ViewModels;
using HidConfigTool.Desktop.Views;
using HidConfigTool.Hid;
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
        collection.AddSingleton<IHidDriver, HidSharpDriver>();
        collection.AddSingleton<IDeviceService, DeviceService>();
        collection.AddSingleton<MainViewModel>();
        collection.AddSingleton<DevicePageViewModel>();
        collection.AddSingleton<KeyPageViewModel>();
        collection.AddSingleton<MousePageViewModel>();
        collection.AddSingleton<JoystickPageViewModel>();
        collection.AddSingleton<EncoderPageViewModel>();
        collection.AddSingleton<MacroPageViewModel>();
        collection.AddSingleton<StatsPageViewModel>();
        collection.AddSingleton<PerfPageViewModel>();
        collection.AddSingleton<ErrorLogPageViewModel>();
        collection.AddSingleton<SettingsPageViewModel>();
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
