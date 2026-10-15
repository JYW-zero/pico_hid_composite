using System.Windows;
using System.Windows.Controls;
using HidConfigTool.ViewModels;

namespace HidConfigTool.App.Views;

/// <summary>
/// DashboardPage.xaml 的交互逻辑
/// 首页仪表盘，参考 Logitech G HUB / Razer Synapse 设计
/// </summary>
public partial class DashboardPage : UserControl
{
    public DashboardPageViewModel ViewModel { get; }

    public DashboardPage(DashboardPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;

        // 订阅导航请求，转发给 MainWindow
        ViewModel.NavigateRequested += (sender, pageType) =>
        {
            // 通过 Application 主窗口的 ViewModel 导航
            if (Application.Current?.MainWindow?.DataContext is MainViewModel mainVm)
            {
                mainVm.CurrentPage = pageType;
            }
        };
    }
}
