using System.Globalization;
using System.IO;
using System.Resources;
using System.Windows;

namespace HidConfigTool.App.Services;

/// <summary>
/// 语言管理服务
/// 支持中英文切换，持久化到配置文件
/// </summary>
public class LanguageManager
{
    public const string Chinese = "zh-CN";
    public const string English = "en";

    private const string ConfigFileName = "language.txt";

    private static ResourceManager? _resourceManager;

    /// <summary>
    /// 当前语言
    /// </summary>
    public string CurrentLanguage { get; private set; } = Chinese;

    /// <summary>
    /// 语言配置文件路径
    /// </summary>
    private static string ConfigPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "HIDConfigTool",
        ConfigFileName);

    /// <summary>
    /// 初始化语言管理
    /// </summary>
    public void Initialize()
    {
        // 加载保存的语言设置
        if (File.Exists(ConfigPath))
        {
            try
            {
                string saved = File.ReadAllText(ConfigPath).Trim();
                if (saved == English || saved == Chinese)
                {
                    CurrentLanguage = saved;
                }
            }
            catch
            {
                // 读取失败，使用默认
            }
        }

        ApplyLanguage(CurrentLanguage);
    }

    /// <summary>
    /// 设置语言
    /// </summary>
    public void SetLanguage(string language)
    {
        if (language != Chinese && language != English)
            return;

        CurrentLanguage = language;
        ApplyLanguage(language);

        // 持久化
        try
        {
            string? dir = Path.GetDirectoryName(ConfigPath);
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }
            File.WriteAllText(ConfigPath, language);
        }
        catch
        {
            // 保存失败静默处理
        }
    }

    /// <summary>
    /// 应用语言设置
    /// </summary>
    private void ApplyLanguage(string language)
    {
        try
        {
            var culture = new CultureInfo(language);
            CultureInfo.CurrentCulture = culture;
            CultureInfo.CurrentUICulture = culture;
            CultureInfo.DefaultThreadCurrentCulture = culture;
            CultureInfo.DefaultThreadCurrentUICulture = culture;
        }
        catch
        {
            // 应用失败静默处理
        }
    }

    /// <summary>
    /// 获取本地化字符串
    /// </summary>
    public static string GetString(string key)
    {
        try
        {
            if (_resourceManager == null)
            {
                _resourceManager = new ResourceManager(
                    "HidConfigTool.App.Resources.Strings",
                    typeof(LanguageManager).Assembly);
            }

            string? value = _resourceManager.GetString(key);
            return value ?? key;
        }
        catch
        {
            return key;
        }
    }

    /// <summary>
    /// 获取当前语言的显示名称
    /// </summary>
    public string GetCurrentLanguageDisplayName()
    {
        return CurrentLanguage == English ? "English" : "简体中文";
    }
}
