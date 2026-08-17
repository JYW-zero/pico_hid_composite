using System.Windows.Controls;

namespace HidConfigTool.App.Views;

/// <summary>
/// ErrorLogPage.xaml 的交互逻辑
/// </summary>
public partial class ErrorLogPage : UserControl
{
    public ErrorLogPageViewModel ViewModel { get; }

    public ErrorLogPage(ErrorLogPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;

        Loaded += (s, e) => ViewModel.OnLoaded();
    }
}
