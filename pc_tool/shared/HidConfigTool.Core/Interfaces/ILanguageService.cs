namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 语言服务抽象
/// </summary>
public interface ILanguageService
{
    string CurrentLanguage { get; }
    void SetLanguage(string language);

    /// <summary>
    /// 语言变化事件（运行时切换语言时触发）
    /// </summary>
    event EventHandler<string>? LanguageChanged;
}
