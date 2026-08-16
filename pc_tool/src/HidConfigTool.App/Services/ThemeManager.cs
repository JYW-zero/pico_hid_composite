using System.IO;
using System.Windows;

namespace HidConfigTool.App.Services;

/// <summary>
/// 主题管理服务 - 支持深色/浅色主题切换
/// </summary>
public class ThemeManager
{
    public const string DarkTheme = "Dark";
    public const string LightTheme = "Light";

    private readonly string _settingsFilePath;
    private string _currentTheme = DarkTheme;

    public string CurrentTheme => _currentTheme;

    public ThemeManager()
    {
        _settingsFilePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "HidConfigTool", "theme.txt");
    }

    /// <summary>
    /// 加载保存的主题设置
    /// </summary>
    public void LoadTheme()
    {
        try
        {
            if (File.Exists(_settingsFilePath))
            {
                _currentTheme = File.ReadAllText(_settingsFilePath).Trim();
                if (_currentTheme != DarkTheme && _currentTheme != LightTheme)
                {
                    _currentTheme = DarkTheme;
                }
            }
        }
        catch
        {
            _currentTheme = DarkTheme;
        }

        ApplyTheme(_currentTheme);
    }

    /// <summary>
    /// 切换主题
    /// </summary>
    public void SetTheme(string theme)
    {
        if (theme != DarkTheme && theme != LightTheme)
            return;

        _currentTheme = theme;
        ApplyTheme(theme);
        SaveTheme();
    }

    /// <summary>
    /// 应用主题到应用程序
    /// </summary>
    private void ApplyTheme(string theme)
    {
        var app = Application.Current;
        if (app == null) return;

        var dictionaries = app.Resources.MergedDictionaries;

        // 移除现有的主题字典
        for (int i = dictionaries.Count - 1; i >= 0; i--)
        {
            var source = dictionaries[i].Source?.OriginalString ?? "";
            if (source.Contains("DarkTheme") || source.Contains("LightTheme"))
            {
                dictionaries.RemoveAt(i);
            }
        }

        // 添加新的主题字典
        string themeFile = theme == LightTheme ? "Themes/LightTheme.xaml" : "Themes/DarkTheme.xaml";
        dictionaries.Add(new ResourceDictionary { Source = new Uri(themeFile, UriKind.Relative) });
    }

    /// <summary>
    /// 保存主题设置
    /// </summary>
    private void SaveTheme()
    {
        try
        {
            var dir = Path.GetDirectoryName(_settingsFilePath);
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }
            File.WriteAllText(_settingsFilePath, _currentTheme);
        }
        catch
        {
            // 保存失败不影响使用
        }
    }
}
