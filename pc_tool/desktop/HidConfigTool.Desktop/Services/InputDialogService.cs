using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 输入对话框服务（基础实现，后续可替换为自定义窗口）
/// </summary>
public class InputDialogService : IInputDialogService
{
    public string? ShowInput(string message, string title, string defaultValue = "")
    {
        // 简单实现：返回默认值，后续可替换为自定义对话框窗口
        return defaultValue;
    }
}
