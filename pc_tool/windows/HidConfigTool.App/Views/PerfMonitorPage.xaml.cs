using System.Windows.Controls;

namespace HidConfigTool.App.Views;

/// <summary>
/// PerfMonitorPage.xaml 的交互逻辑
/// </summary>
public partial class PerfMonitorPage : UserControl
{
    public PerfMonitorPageViewModel ViewModel { get; }

    public PerfMonitorPage(PerfMonitorPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;

        Loaded += (s, e) => ViewModel.OnLoaded();
        Unloaded += (s, e) => ViewModel.OnUnloaded();
    }
}
