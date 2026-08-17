using System.Windows.Controls;

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
