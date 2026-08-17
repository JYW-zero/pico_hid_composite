using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Desktop.Views;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 对话框服务实现
/// </summary>
public class DialogService : IDialogService
{
    private Avalonia.Controls.Window? GetMainWindow()
    {
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            return desktop.MainWindow;
        return null;
    }

    public async void ShowInfo(string message, string title = "提示")
    {
        var window = GetMainWindow();
        if (window != null)
        {
            var dlg = new MessageBoxWindow(message, title, MessageBoxButtons.Ok);
            await dlg.ShowDialog(window);
        }
    }

    public async void ShowWarning(string message, string title = "警告")
    {
        var window = GetMainWindow();
        if (window != null)
        {
            var dlg = new MessageBoxWindow(message, title, MessageBoxButtons.Ok);
            await dlg.ShowDialog(window);
        }
    }

    public async void ShowError(string message, string title = "错误")
    {
        var window = GetMainWindow();
        if (window != null)
        {
            var dlg = new MessageBoxWindow(message, title, MessageBoxButtons.Ok);
            await dlg.ShowDialog(window);
        }
    }

    public bool ShowConfirm(string message, string title = "确认")
    {
        var window = GetMainWindow();
        if (window != null)
        {
            var dlg = new MessageBoxWindow(message, title, MessageBoxButtons.YesNo);
            dlg.ShowDialog(window).Wait();
            return dlg.Result == MessageBoxResult.Yes;
        }
        return false;
    }
}
