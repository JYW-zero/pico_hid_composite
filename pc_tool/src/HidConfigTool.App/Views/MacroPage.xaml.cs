using System.Collections.Specialized;
using System.Windows.Controls;
using HidConfigTool.App.ViewModels;

namespace HidConfigTool.App.Views;

public partial class MacroPage : UserControl
{
    public MacroPageViewModel ViewModel { get; }

    public MacroPage(MacroPageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;

        // 监听动作列表变化，自动滚动到最新添加的动作
        ViewModel.Actions.CollectionChanged += Actions_CollectionChanged;
    }

    private void Actions_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        if (e.Action == NotifyCollectionChangedAction.Add && ViewModel.Actions.Count > 0)
        {
            // 滚动到最后一项
            var lastItem = ViewModel.Actions[ViewModel.Actions.Count - 1];
            ActionListBox.ScrollIntoView(lastItem);
        }
    }
}
