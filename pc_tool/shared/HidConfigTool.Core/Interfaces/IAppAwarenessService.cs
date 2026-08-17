using HidConfigTool.Core.Models;

namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 应用感知服务抽象
/// </summary>
public interface IAppAwarenessService
{
    bool IsEnabled { get; set; }
    List<AppAwarenessRule> Rules { get; }
    void Start();
    void Stop();
    void AddRule(string processName, string appName, string profileName);
    void RemoveRule(string processName);
}
