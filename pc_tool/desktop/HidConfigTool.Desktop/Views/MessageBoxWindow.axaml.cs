using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;

namespace HidConfigTool.Desktop.Views;

/// <summary>
/// 简单的消息对话框窗口
/// </summary>
public class MessageBoxWindow : Window
{
    public MessageBoxResult Result { get; private set; } = MessageBoxResult.Ok;

    public MessageBoxWindow(string message, string title, MessageBoxButtons buttons)
    {
        Title = title;
        Width = 350;
        Height = 180;
        CanResize = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;

        var panel = new StackPanel
        {
            Margin = new Avalonia.Thickness(20),
            Spacing = 15
        };

        var textBlock = new TextBlock
        {
            Text = message,
            TextWrapping = TextWrapping.Wrap,
            HorizontalAlignment = HorizontalAlignment.Center
        };
        panel.Children.Add(textBlock);

        var buttonPanel = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Center,
            Spacing = 10
        };

        if (buttons == MessageBoxButtons.YesNo)
        {
            var yesBtn = new Button { Content = "是", Width = 80 };
            yesBtn.Click += (_, _) => { Result = MessageBoxResult.Yes; Close(); };
            var noBtn = new Button { Content = "否", Width = 80 };
            noBtn.Click += (_, _) => { Result = MessageBoxResult.No; Close(); };
            buttonPanel.Children.Add(yesBtn);
            buttonPanel.Children.Add(noBtn);
        }
        else
        {
            var okBtn = new Button { Content = "确定", Width = 80 };
            okBtn.Click += (_, _) => { Result = MessageBoxResult.Ok; Close(); };
            buttonPanel.Children.Add(okBtn);
        }

        panel.Children.Add(buttonPanel);
        Content = panel;
    }
}

public enum MessageBoxButtons { Ok, YesNo }
public enum MessageBoxResult { Ok, Yes, No }
