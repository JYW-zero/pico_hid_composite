using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 文件对话框服务实现
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

        var dialog = new OpenFileDialog
        {
            Title = title,
            Filters = ParseFilter(filter),
            AllowMultiple = false
        };
        var result = dialog.ShowAsync(window).GetAwaiter().GetResult();
        return result?.Length > 0 ? result[0] : null;
    }

    public string? SaveFile(string title, string filter, string defaultExt)
    {
        var window = GetMainWindow();
        if (window == null) return null;

        var dialog = new SaveFileDialog
        {
            Title = title,
            Filters = ParseFilter(filter),
            DefaultExtension = defaultExt
        };
        return dialog.ShowAsync(window).GetAwaiter().GetResult();
    }

    private List<FileDialogFilter> ParseFilter(string filter)
    {
        var filters = new List<FileDialogFilter>();
        foreach (var part in filter.Split('|'))
        {
            if (part.Contains('.'))
            {
                var name = part.Trim();
                var exts = name.Split(';').Select(e => e.TrimStart('*', '.')).ToList();
                filters.Add(new FileDialogFilter { Name = name, Extensions = exts });
            }
        }
        return filters;
    }
}
