using System.IO;
using System.Windows;
using System.Windows.Threading;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.App.Drivers;
using HidConfigTool.App.ViewModels;
using HidConfigTool.App.Views;
using HidConfigTool.App.Services;
using HidConfigTool.Core.Services;

namespace HidConfigTool.App;

public partial class App : Application
{
    public static IHost? Host { get; private set; }
    private static string _logPath = string.Empty;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        // 初始化日志路径
        string appDataPath = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        string logDir = Path.Combine(appDataPath, "HIDConfigTool", "Logs");
        Directory.CreateDirectory(logDir);
        _logPath = Path.Combine(logDir, $"crash_{DateTime.Now:yyyyMMdd_HHmmss}.log");

        // 全局异常处理
        DispatcherUnhandledException += OnDispatcherUnhandledException;
        AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
        TaskScheduler.UnobservedTaskException += OnUnobservedTaskException;

        Log("程序启动");

        try
        {
            // 构建主机
            Host = Microsoft.Extensions.Hosting.Host.CreateDefaultBuilder()
                .ConfigureServices((context, services) =>
                {
                    ConfigureServices(services);
                })
                .Build();

            Log("依赖注入构建完成");

            // 加载主题设置
            var themeManager = Host.Services.GetRequiredService<ThemeManager>();
            themeManager.LoadTheme();
            Log("主题加载完成");

            // 初始化语言设置
            var languageManager = Host.Services.GetRequiredService<LanguageManager>();
            languageManager.Initialize();
            Log("语言初始化完成");

            // 显示主窗口
            var mainWindow = Host.Services.GetRequiredService<MainWindow>();
            Log("主窗口创建成功");
            mainWindow.Show();
            Log("主窗口显示成功");
        }
        catch (Exception ex)
        {
            Log($"启动失败: {ex}");
            MessageBox.Show($"启动失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private static void ConfigureServices(IServiceCollection services)
    {
        Log("开始注册服务...");

        // 驱动层
        services.AddSingleton<IHidDriver, HidDriver>();
        Log("注册: IHidDriver");

        // 服务层
        services.AddSingleton<IDeviceService, DeviceService>();
        Log("注册: IDeviceService");

        services.AddSingleton<ICloudSyncService, LocalCloudSyncService>();
        Log("注册: ICloudSyncService");

        services.AddSingleton<IUpdateService, UpdateService>();
        Log("注册: IUpdateService");

        services.AddSingleton<TrayIconManager>();
        Log("注册: TrayIconManager");

        services.AddSingleton<AutoStartManager>();
        Log("注册: AutoStartManager");

        services.AddSingleton<ConfigProfileManager>();
        Log("注册: ConfigProfileManager");

        services.AddSingleton<OsdManager>();
        Log("注册: OsdManager");

        services.AddSingleton<AppAwarenessManager>();
        Log("注册: AppAwarenessManager");

        services.AddSingleton<KeyboardHook>();
        Log("注册: KeyboardHook");

        services.AddSingleton<MacroRecorder>();
        Log("注册: MacroRecorder");

        services.AddSingleton<ThemeManager>();
        Log("注册: ThemeManager");

        services.AddSingleton<LanguageManager>();
        Log("注册: LanguageManager");

        // 视图模型
        services.AddSingleton<MainViewModel>();
        Log("注册: MainViewModel");

        services.AddTransient<DevicePageViewModel>();
        Log("注册: DevicePageViewModel");

        services.AddTransient<KeyPageViewModel>();
        Log("注册: KeyPageViewModel");

        services.AddTransient<KeyTestPageViewModel>();
        Log("注册: KeyTestPageViewModel");

        services.AddTransient<MousePageViewModel>();
        Log("注册: MousePageViewModel");

        services.AddTransient<JoystickPageViewModel>();
        Log("注册: JoystickPageViewModel");

        services.AddTransient<EncoderPageViewModel>();
        Log("注册: EncoderPageViewModel");

        services.AddTransient<MacroPageViewModel>();
        Log("注册: MacroPageViewModel");

        services.AddTransient<ErrorLogPageViewModel>();
        Log("注册: ErrorLogPageViewModel");

        services.AddTransient<PerfMonitorPageViewModel>();
        Log("注册: PerfMonitorPageViewModel");

        services.AddTransient<SettingsPageViewModel>();
        Log("注册: SettingsPageViewModel");

        services.AddTransient<StatsPageViewModel>();
        Log("注册: StatsPageViewModel");

        // 视图
        services.AddTransient<MainWindow>();
        Log("注册: MainWindow");

        services.AddTransient<DevicePage>();
        Log("注册: DevicePage");

        services.AddTransient<KeyPage>();
        Log("注册: KeyPage");

        services.AddTransient<KeyTestPage>();
        Log("注册: KeyTestPage");

        services.AddTransient<KeyManagementPage>();
        Log("注册: KeyManagementPage");

        services.AddTransient<MousePage>();
        Log("注册: MousePage");

        services.AddTransient<JoystickPage>();
        Log("注册: JoystickPage");

        services.AddTransient<EncoderPage>();
        Log("注册: EncoderPage");

        services.AddTransient<MacroPage>();
        Log("注册: MacroPage");

        services.AddTransient<ErrorLogPage>();
        Log("注册: ErrorLogPage");

        services.AddTransient<PerfMonitorPage>();
        Log("注册: PerfMonitorPage");

        services.AddTransient<SettingsPage>();
        Log("注册: SettingsPage");

        services.AddTransient<StatsPage>();
        Log("注册: StatsPage");

        Log("所有服务注册完成");
    }

    private void OnDispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        Log($"UI线程未处理异常: {e.Exception}");
        e.Handled = true;
        MessageBox.Show($"发生错误: {e.Exception.Message}\n\n详情请查看日志文件: {_logPath}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
    }

    private void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
    {
        if (e.ExceptionObject is Exception ex)
        {
            Log($"非UI线程未处理异常: {ex}");
        }
        else
        {
            Log($"非UI线程未处理异常: {e.ExceptionObject}");
        }
    }

    private void OnUnobservedTaskException(object? sender, UnobservedTaskExceptionEventArgs e)
    {
        Log($"任务未观察异常: {e.Exception}");
        e.SetObserved();
    }

    private static void Log(string message)
    {
        try
        {
            if (!string.IsNullOrEmpty(_logPath))
            {
                File.AppendAllText(_logPath, $"[{DateTime.Now:HH:mm:ss.fff}] {message}\n");
            }
        }
        catch
        {
            // 忽略日志写入错误
        }
    }

    protected override void OnExit(ExitEventArgs e)
    {
        Log("程序退出");
        Host?.Dispose();
        base.OnExit(e);
    }
}
