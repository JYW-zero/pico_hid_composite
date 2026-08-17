using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 开机自启动服务（基础实现，跨平台需要不同处理）
/// </summary>
public class AutoStartService : IAutoStartService
{
    public bool IsEnabled() => false;
    public bool Toggle(bool enable) => false;
}
