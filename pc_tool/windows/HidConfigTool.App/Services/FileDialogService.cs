using Microsoft.Win32;
using System.Diagnostics;
using System.IO;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.Services;

/// <summary>
/// WPF 平台文件对话框服务
/// </summary>
public class FileDialogService : IFileDialogService
{
    public string? OpenFile(string title, string filter)
    {
        var dialog = new OpenFileDialog
        {
            Title = title,
            Filter = filter
        };
        return dialog.ShowDialog() == true ? dialog.FileName : null;
    }

    public string? SaveFile(string title, string filter, string defaultExt)
    {
        var dialog = new SaveFileDialog
        {
            Title = title,
            Filter = filter,
            DefaultExt = defaultExt
        };
        return dialog.ShowDialog() == true ? dialog.FileName : null;
    }

    public void OpenFolder(string path)
    {
        if (Directory.Exists(path))
        {
            Process.Start(new ProcessStartInfo("explorer.exe", path) { UseShellExecute = true });
        }
    }
}
