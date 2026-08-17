using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.Services;

/// <summary>
/// 开机自启动服务实现（包装 AutoStartManager 静态类）
/// </summary>
public class AutoStartService : IAutoStartService
{
    public bool IsEnabled() => AutoStartManager.IsEnabled();
    public bool Toggle(bool enable) => AutoStartManager.Toggle(enable);
}
