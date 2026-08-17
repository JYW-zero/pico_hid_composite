namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 文件对话框服务抽象
/// </summary>
public interface IFileDialogService
{
    /// <summary>打开文件对话框，返回选中的文件路径，取消返回 null</summary>
    string? OpenFile(string title, string filter);

    /// <summary>保存文件对话框，返回选中的文件路径，取消返回 null</summary>
    string? SaveFile(string title, string filter, string defaultExt);

    /// <summary>用系统文件管理器打开指定文件夹</summary>
    void OpenFolder(string path);
}
