using System.Windows.Controls;
using HidConfigTool.App.ViewModels;

namespace HidConfigTool.App.Views;

public partial class StatsPage : UserControl
{
    public StatsPageViewModel ViewModel { get; }

    public StatsPage(StatsPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }
}
