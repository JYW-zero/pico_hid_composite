using System.Windows;
using System.Windows.Input;

namespace HidConfigTool.App.Views;

/// <summary>
/// 简单的文本输入对话框
/// </summary>
public partial class InputDialog : Window
{
    /// <summary>
    /// 用户输入的文本
    /// </summary>
    public string InputText => InputBox.Text;

    public InputDialog()
    {
        InitializeComponent();
        Loaded += (s, e) =>
        {
            InputBox.Focus();
            InputBox.SelectAll();
        };
    }

    /// <summary>
    /// 显示输入对话框
    /// </summary>
    /// <param name="owner">所有者窗口</param>
    /// <param name="prompt">提示文本</param>
    /// <param name="title">窗口标题</param>
    /// <param name="defaultValue">默认值</param>
    /// <returns>用户输入的文本，取消返回 null</returns>
    public static string? Show(Window? owner, string prompt, string title = "输入", string defaultValue = "")
    {
        var dialog = new InputDialog
        {
            Title = title,
            Owner = owner
        };
        dialog.PromptText.Text = prompt;
        dialog.InputBox.Text = defaultValue;

        bool? result = dialog.ShowDialog();
        return result == true ? dialog.InputText : null;
    }

    private void OkButton_Click(object sender, RoutedEventArgs e)
    {
        DialogResult = true;
        Close();
    }

    private void CancelButton_Click(object sender, RoutedEventArgs e)
    {
        DialogResult = false;
        Close();
    }

    private void InputBox_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
        {
            DialogResult = true;
            Close();
        }
        else if (e.Key == Key.Escape)
        {
            DialogResult = false;
            Close();
        }
    }
}
