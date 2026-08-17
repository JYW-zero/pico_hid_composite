using System.Windows;
using HidConfigTool.App.Views;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.Services;

/// <summary>
/// WPF 平台输入对话框服务
/// </summary>
public class InputDialogService : IInputDialogService
{
    public string? ShowInput(string message, string title, string defaultValue = "")
    {
        return InputDialog.Show(Application.Current.MainWindow, message, title, defaultValue);
    }
}
