using System.Windows.Controls;

namespace HidConfigTool.App.Views;

public partial class MousePage : UserControl
{
    public MousePageViewModel ViewModel { get; }

    public MousePage(MousePageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }
}
