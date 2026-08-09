using System.Windows.Controls;
using HidConfigTool.App.ViewModels;

namespace HidConfigTool.App.Views;

public partial class EncoderPage : UserControl
{
    public EncoderPageViewModel ViewModel { get; }

    public EncoderPage(EncoderPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }
}
