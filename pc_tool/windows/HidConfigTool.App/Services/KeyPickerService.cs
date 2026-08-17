using System.Windows;
using HidConfigTool.App.Views;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.Services;

/// <summary>
/// WPF 平台键选择对话框服务
/// </summary>
public class KeyPickerService : IKeyPickerService
{
    public (byte KeyCode, string KeyName)? ShowKeyPicker(byte currentKeyCode)
    {
        var dialog = new KeyPickerDialog(currentKeyCode)
        {
            Owner = Application.Current.MainWindow
        };

        if (dialog.ShowDialog() == true)
        {
            return (dialog.SelectedKeyCode, dialog.SelectedKeyName);
        }
        return null;
    }
}
