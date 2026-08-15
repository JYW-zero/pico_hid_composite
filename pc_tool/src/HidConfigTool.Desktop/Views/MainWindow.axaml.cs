using Avalonia.Controls;
using HidConfigTool.Desktop.ViewModels;

namespace HidConfigTool.Desktop.Views;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
    }

    public MainWindow(MainViewModel viewModel) : this()
    {
        DataContext = viewModel;
    }
}
