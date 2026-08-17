using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 语言服务实现
/// </summary>
public class LanguageService : ILanguageService
{
    public string CurrentLanguage { get; private set; } = LanguageConstants.Chinese;

    public void SetLanguage(string language)
    {
        CurrentLanguage = language;
    }
}
