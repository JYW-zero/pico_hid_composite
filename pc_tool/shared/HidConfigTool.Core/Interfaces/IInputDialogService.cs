namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 输入对话框服务抽象，用于获取用户文本输入
/// </summary>
public interface IInputDialogService
{
    /// <summary>
    /// 显示输入对话框，返回用户输入的文本，取消返回 null
    /// </summary>
    string? ShowInput(string message, string title, string defaultValue = "");
}
