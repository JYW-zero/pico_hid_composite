using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 应用感知服务（基础实现，跨平台前台窗口检测需要平台API）
/// </summary>
public class AppAwarenessService : IAppAwarenessService
{
    public bool IsEnabled { get; set; }
    public List<AppAwarenessRule> Rules { get; private set; } = new();

    public void Start() { }
    public void Stop() { }
    public void AddRule(string processName, string appName, string profileName)
    {
        Rules.Add(new AppAwarenessRule { ProcessName = processName, AppName = appName, ProfileName = profileName });
    }
    public void RemoveRule(string processName)
    {
        Rules.RemoveAll(r => r.ProcessName == processName);
    }
}
