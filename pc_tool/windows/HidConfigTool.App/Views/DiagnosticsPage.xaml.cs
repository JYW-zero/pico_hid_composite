using System.Windows.Controls;
using HidConfigTool.ViewModels;

namespace HidConfigTool.App.Views;

/// <summary>
/// DiagnosticsPage.xaml 的交互逻辑
/// </summary>
public partial class DiagnosticsPage : UserControl
{
    public DiagnosticsPage(DiagnosticsPageViewModel viewModel)
    {
        InitializeComponent();
        DataContext = viewModel;
    }
}
