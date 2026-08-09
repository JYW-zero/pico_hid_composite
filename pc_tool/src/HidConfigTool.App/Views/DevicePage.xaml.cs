using System.Windows.Controls;
using HidConfigTool.App.ViewModels;
using Microsoft.Extensions.DependencyInjection;

namespace HidConfigTool.App.Views;

public partial class DevicePage : UserControl
{
    public DevicePageViewModel ViewModel { get; }

    public DevicePage(DevicePageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }
}
