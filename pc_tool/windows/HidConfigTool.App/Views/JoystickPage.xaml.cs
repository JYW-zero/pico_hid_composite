using System.Windows.Controls;

namespace HidConfigTool.App.Views;

public partial class JoystickPage : UserControl
{
    public JoystickPageViewModel ViewModel { get; }

    public JoystickPage(JoystickPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }
}
