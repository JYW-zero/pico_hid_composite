using System.Windows.Controls;
using System.Windows.Input;
using HidConfigTool.App.ViewModels;

namespace HidConfigTool.App.Views;

public partial class SettingsPage : UserControl
{
    public SettingsPageViewModel ViewModel { get; }

    public SettingsPage(SettingsPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }

    private void ProfileComboBox_MouseDoubleClick(object sender, MouseButtonEventArgs e)
    {
        if (ViewModel.RenameProfileCommand.CanExecute(null))
        {
            ViewModel.RenameProfileCommand.Execute(null);
        }
    }
}
