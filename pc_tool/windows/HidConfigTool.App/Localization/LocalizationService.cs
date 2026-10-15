using System.ComponentModel;
using System.Runtime.CompilerServices;
using HidConfigTool.App.Services;

namespace HidConfigTool.App.Localization;

/// <summary>
/// 本地化服务
/// 实现 INotifyPropertyChanged，提供索引器，用于 XAML 绑定
/// 运行时切换语言时自动通知 UI 刷新
/// </summary>
public class LocalizationService : INotifyPropertyChanged
{
    private static LocalizationService? _instance;
    private readonly LanguageManager _languageManager;

    /// <summary>
    /// 单例实例
    /// </summary>
    public static LocalizationService Instance
    {
        get
        {
            _instance ??= new LocalizationService();
            return _instance;
        }
    }

    /// <summary>
    /// 索引器，通过键获取本地化字符串
    /// XAML 绑定方式：{Binding [App_Title], Source={x:Static loc:LocalizationService.Instance}}
    /// 注意：setter 为空实现，仅为了支持 WPF 的默认 TwoWay 绑定模式
    ///       实际本地化字符串是只读的，setter 不会做任何事情
    /// </summary>
    /// <param name="key">资源键</param>
    /// <returns>本地化字符串</returns>
    public string this[string key]
    {
        get => LanguageManager.GetString(key);
        set { /* 空实现，仅为了支持 WPF TwoWay 绑定 */ }
    }

    /// <summary>
    /// 当前语言
    /// </summary>
    public string CurrentLanguage => _languageManager.CurrentLanguage;

    /// <summary>
    /// 属性变化事件
    /// </summary>
    public event PropertyChangedEventHandler? PropertyChanged;

    /// <summary>
    /// 构造函数
    /// 注意：为了支持 XAML 中作为 StaticResource 创建实例，构造函数设为 public
    /// 实际使用时应通过 Instance 属性访问单例，或在 App.xaml 中定义一次后复用
    /// </summary>
    public LocalizationService()
    {
        // 直接创建 LanguageManager 实例
        _languageManager = new LanguageManager();
        _languageManager.Initialize();

        // 订阅语言变化事件
        _languageManager.LanguageChanged += OnLanguageChanged;
    }

    /// <summary>
    /// 语言变化处理
    /// </summary>
    private void OnLanguageChanged(object? sender, string language)
    {
        // 通知所有绑定的属性刷新
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(string.Empty));
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CurrentLanguage)));
    }

    /// <summary>
    /// 设置语言
    /// </summary>
    /// <param name="language">语言代码（zh-CN 或 en）</param>
    public void SetLanguage(string language)
    {
        _languageManager.SetLanguage(language);
    }

    /// <summary>
    /// 切换到中文
    /// </summary>
    public void SetChinese()
    {
        SetLanguage(LanguageManager.Chinese);
    }

    /// <summary>
    /// 切换到英文
    /// </summary>
    public void SetEnglish()
    {
        SetLanguage(LanguageManager.English);
    }

    /// <summary>
    /// 获取本地化字符串
    /// </summary>
    /// <param name="key">资源键</param>
    /// <returns>本地化字符串</returns>
    public string GetString(string key)
    {
        return LanguageManager.GetString(key);
    }

    /// <summary>
    /// 获取带参数的本地化字符串
    /// </summary>
    /// <param name="key">资源键</param>
    /// <param name="args">参数</param>
    /// <returns>格式化后的字符串</returns>
    public string GetString(string key, params object[] args)
    {
        string format = LanguageManager.GetString(key);
        try
        {
            return string.Format(format, args);
        }
        catch
        {
            return format;
        }
    }
}
