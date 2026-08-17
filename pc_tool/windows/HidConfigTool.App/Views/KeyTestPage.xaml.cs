using System.Windows.Controls;
using Microsoft.Extensions.DependencyInjection;

namespace HidConfigTool.App.Views;

/// <summary>
/// 按键测试页面
/// </summary>
public partial class KeyTestPage : UserControl
{
    public KeyTestPage(IServiceProvider serviceProvider)
    {
        InitializeComponent();
        DataContext = serviceProvider.GetRequiredService<KeyTestPageViewModel>();
    }
}
