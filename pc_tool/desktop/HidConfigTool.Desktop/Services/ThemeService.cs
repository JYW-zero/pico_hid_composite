using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 主题服务实现
/// </summary>
public class ThemeService : IThemeService
{
    public string CurrentTheme { get; private set; } = ThemeConstants.Dark;

    public void SetTheme(string theme)
    {
        CurrentTheme = theme;
        // Avalonia 主题切换需要在 App.axaml 中处理
        // 这里可以通过事件通知 UI 层切换主题
    }
}
