namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 语言服务抽象
/// </summary>
public interface ILanguageService
{
    string CurrentLanguage { get; }
    void SetLanguage(string language);
}
