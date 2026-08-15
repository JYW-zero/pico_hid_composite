using Avalonia.Controls;
using Avalonia.Interactivity;
using HidConfigTool.Desktop.ViewModels;

namespace HidConfigTool.Desktop.Views.Pages;

public partial class KeyPage : UserControl
{
    public KeyPage() => InitializeComponent();

    private void OnKeyClick(object? sender, RoutedEventArgs e)
    {
        if (sender is Button { Tag: KeyItemViewModel key } && DataContext is KeyPageViewModel vm)
            vm.SelectKeyCommand.Execute(key);
    }
}
