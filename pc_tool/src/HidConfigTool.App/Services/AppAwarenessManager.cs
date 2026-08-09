using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows.Threading;

namespace HidConfigTool.App.Services;

/// <summary>
/// 应用感知规则
/// </summary>
public class AppAwarenessRule
{
    public string ProcessName { get; set; } = string.Empty;
    public string AppName { get; set; } = string.Empty;
    public string ProfileName { get; set; } = string.Empty;
    public bool IsEnabled { get; set; } = true;
}

/// <summary>
/// 应用感知管理器
/// 自动检测前台应用，切换对应配置
/// </summary>
public class AppAwarenessManager
{
    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("psapi.dll")]
    private static extern uint GetModuleBaseName(IntPtr hProcess, IntPtr hModule, System.Text.StringBuilder lpBaseName, uint nSize);

    [DllImport("kernel32.dll")]
    private static extern IntPtr OpenProcess(uint processAccess, bool bInheritHandle, uint processId);

    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(IntPtr hObject);

    private const uint PROCESS_QUERY_INFORMATION = 0x0400;
    private const uint PROCESS_VM_READ = 0x0010;

    private readonly DispatcherTimer _timer;
    private string _lastProcessName = string.Empty;
    private readonly ConfigProfileManager _profileManager;
    private readonly OsdManager _osdManager;

    /// <summary>
    /// 是否启用应用感知
    /// </summary>
    public bool IsEnabled { get; set; }

    /// <summary>
    /// 应用规则列表
    /// </summary>
    public List<AppAwarenessRule> Rules { get; private set; } = new();

    /// <summary>
    /// 当前前台应用进程名
    /// </summary>
    public string CurrentProcessName { get; private set; } = string.Empty;

    /// <summary>
    /// 当前激活的配置名
    /// </summary>
    public string CurrentProfileName { get; private set; } = string.Empty;

    public AppAwarenessManager(ConfigProfileManager profileManager, OsdManager osdManager)
    {
        _profileManager = profileManager;
        _osdManager = osdManager;

        _timer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(500)
        };
        _timer.Tick += OnTimerTick;

        // 添加一些默认规则示例
        AddDefaultRules();
    }

    private void AddDefaultRules()
    {
        Rules.Add(new AppAwarenessRule
        {
            ProcessName = "notepad",
            AppName = "记事本",
            ProfileName = "默认配置",
            IsEnabled = true
        });

        Rules.Add(new AppAwarenessRule
        {
            ProcessName = "chrome",
            AppName = "Google Chrome",
            ProfileName = "办公配置",
            IsEnabled = true
        });

        Rules.Add(new AppAwarenessRule
        {
            ProcessName = "code",
            AppName = "VS Code",
            ProfileName = "办公配置",
            IsEnabled = true
        });

        Rules.Add(new AppAwarenessRule
        {
            ProcessName = "csgo",
            AppName = "CS:GO",
            ProfileName = "游戏配置",
            IsEnabled = true
        });
    }

    /// <summary>
    /// 启动应用感知
    /// </summary>
    public void Start()
    {
        if (!IsEnabled)
        {
            IsEnabled = true;
            _timer.Start();
        }
    }

    /// <summary>
    /// 停止应用感知
    /// </summary>
    public void Stop()
    {
        IsEnabled = false;
        _timer.Stop();
    }

    private void OnTimerTick(object? sender, EventArgs e)
    {
        try
        {
            string processName = GetForegroundProcessName();

            if (string.IsNullOrEmpty(processName))
                return;

            // 进程名没变，跳过
            if (processName.Equals(_lastProcessName, StringComparison.OrdinalIgnoreCase))
                return;

            _lastProcessName = processName;
            CurrentProcessName = processName;

            // 查找匹配的规则
            var rule = Rules.FirstOrDefault(r =>
                r.IsEnabled &&
                r.ProcessName.Equals(processName, StringComparison.OrdinalIgnoreCase));

            if (rule != null && !string.IsNullOrEmpty(rule.ProfileName))
            {
                // 切换配置
                SwitchProfile(rule.ProfileName);
            }
        }
        catch
        {
            // 忽略错误
        }
    }

    private string GetForegroundProcessName()
    {
        try
        {
            IntPtr hWnd = GetForegroundWindow();
            if (hWnd == IntPtr.Zero)
                return string.Empty;

            GetWindowThreadProcessId(hWnd, out uint processId);
            if (processId == 0)
                return string.Empty;

            IntPtr hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, processId);
            if (hProcess == IntPtr.Zero)
                return string.Empty;

            try
            {
                var sb = new System.Text.StringBuilder(256);
                GetModuleBaseName(hProcess, IntPtr.Zero, sb, (uint)sb.Capacity);
                string name = sb.ToString();

                // 去掉 .exe 后缀
                if (name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
                {
                    name = name.Substring(0, name.Length - 4);
                }

                return name;
            }
            finally
            {
                CloseHandle(hProcess);
            }
        }
        catch
        {
            return string.Empty;
        }
    }

    private void SwitchProfile(string profileName)
    {
        if (profileName == CurrentProfileName)
            return;

        CurrentProfileName = profileName;
        _osdManager.ShowProfileChange(profileName);

        // 实际项目中这里会加载配置并应用到设备
    }

    /// <summary>
    /// 添加规则
    /// </summary>
    public void AddRule(string processName, string appName, string profileName)
    {
        Rules.Add(new AppAwarenessRule
        {
            ProcessName = processName,
            AppName = appName,
            ProfileName = profileName,
            IsEnabled = true
        });
    }

    /// <summary>
    /// 删除规则
    /// </summary>
    public void RemoveRule(string processName)
    {
        var rule = Rules.FirstOrDefault(r => r.ProcessName == processName);
        if (rule != null)
        {
            Rules.Remove(rule);
        }
    }
}
