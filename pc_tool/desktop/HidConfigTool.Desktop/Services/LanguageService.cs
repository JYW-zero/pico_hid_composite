using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 语言服务实现
/// </summary>
public class LanguageService : ILanguageService
{
    public string CurrentLanguage { get; private set; } = LanguageConstants.Chinese;

    /// <summary>
    /// 语言变化事件
    /// </summary>
    public event EventHandler<string>? LanguageChanged;

    public void SetLanguage(string language)
    {
        if (CurrentLanguage != language)
        {
            CurrentLanguage = language;
            LanguageChanged?.Invoke(this, language);
        }
    }
}
