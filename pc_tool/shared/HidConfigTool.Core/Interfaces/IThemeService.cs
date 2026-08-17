namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 主题服务抽象
/// </summary>
public interface IThemeService
{
    string CurrentTheme { get; }
    void SetTheme(string theme);
}
