using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Platform.Storage;
using System.Diagnostics;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 文件对话框服务实现（使用 StorageProvider API）
/// </summary>
public class FileDialogService : IFileDialogService
{
    private Window? GetMainWindow()
    {
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            return desktop.MainWindow;
        return null;
    }

    public string? OpenFile(string title, string filter)
    {
        var window = GetMainWindow();
        if (window == null) return null;

        var options = new FilePickerOpenOptions
        {
            Title = title,
            AllowMultiple = false,
            FileTypeFilter = ParseFilterForOpen(filter)
        };

        var result = window.StorageProvider.OpenFilePickerAsync(options).GetAwaiter().GetResult();
        return result?.Count > 0 ? result[0].Path.LocalPath : null;
    }

    public string? SaveFile(string title, string filter, string defaultExt)
    {
        var window = GetMainWindow();
        if (window == null) return null;

        var options = new FilePickerSaveOptions
        {
            Title = title,
            DefaultExtension = defaultExt,
            FileTypeChoices = ParseFilterForSave(filter)
        };

        var result = window.StorageProvider.SaveFilePickerAsync(options).GetAwaiter().GetResult();
        return result?.Path.LocalPath;
    }

    public void OpenFolder(string path)
    {
        if (Directory.Exists(path))
        {
            Process.Start(new ProcessStartInfo { FileName = path, UseShellExecute = true });
        }
    }

    /// <summary>
    /// 解析过滤器字符串为 FilePickerFileType 列表（用于打开文件）
    /// 格式: "描述1|*.ext1;*.ext2|描述2|*.ext3"
    /// </summary>
    private IReadOnlyList<FilePickerFileType> ParseFilterForOpen(string filter)
    {
        var result = new List<FilePickerFileType>();
        var parts = filter.Split('|');
        for (int i = 0; i + 1 < parts.Length; i += 2)
        {
            var name = parts[i].Trim();
            var exts = parts[i + 1].Split(';')
                .Select(e => e.Trim().TrimStart('*'))
                .Where(e => !string.IsNullOrEmpty(e))
                .ToList();
            if (exts.Count > 0)
            {
                result.Add(new FilePickerFileType(name)
                {
                    Patterns = exts.Select(e => $"*{e}").ToArray()
                });
            }
        }
        // 如果没有解析到任何过滤器，添加"所有文件"
        if (result.Count == 0)
        {
            result.Add(new FilePickerFileType("所有文件") { Patterns = new[] { "*" } });
        }
        return result;
    }

    /// <summary>
    /// 解析过滤器字符串为 FilePickerFileType 列表（用于保存文件）
    /// </summary>
    private IReadOnlyList<FilePickerFileType>? ParseFilterForSave(string filter)
    {
        var result = ParseFilterForOpen(filter);
        return result.Count > 0 ? result : null;
    }
}
