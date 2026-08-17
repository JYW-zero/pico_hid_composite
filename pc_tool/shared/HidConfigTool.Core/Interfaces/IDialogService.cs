namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 对话框服务抽象，用于在 ViewModel 中显示消息框和确认对话框
/// </summary>
public interface IDialogService
{
    /// <summary>显示信息提示</summary>
    void ShowInfo(string message, string title = "提示");

    /// <summary>显示警告提示</summary>
    void ShowWarning(string message, string title = "警告");

    /// <summary>显示错误提示</summary>
    void ShowError(string message, string title = "错误");

    /// <summary>显示确认对话框，返回 true 表示用户点击了"是"</summary>
    bool ShowConfirm(string message, string title = "确认");
}
