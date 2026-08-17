using System.Windows.Controls;

namespace HidConfigTool.App.Views;

public partial class KeyPage : UserControl
{
    public KeyPageViewModel ViewModel { get; }

    public KeyPage(KeyPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }
}
