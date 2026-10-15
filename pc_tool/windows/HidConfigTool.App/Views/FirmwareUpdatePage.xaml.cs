using System.Windows.Controls;
using HidConfigTool.ViewModels;

namespace HidConfigTool.App.Views;

/// <summary>
/// FirmwareUpdatePage.xaml 的交互逻辑
/// </summary>
public partial class FirmwareUpdatePage : UserControl
{
    public FirmwareUpdatePageViewModel ViewModel { get; }

    public FirmwareUpdatePage(FirmwareUpdatePageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }
}
