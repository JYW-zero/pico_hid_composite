namespace HidConfigTool.App.Views;

using System.Windows.Controls;
using System.Windows.Input;
using HidConfigTool.Core.Models;
using HidConfigTool.ViewModels;

/// <summary>
/// PresetPage.xaml 的交互逻辑
/// </summary>
public partial class PresetPage : UserControl
{
    public PresetPage(PresetPageViewModel viewModel)
    {
        InitializeComponent();
        DataContext = viewModel;
    }

    /// <summary>
    /// 预设卡片点击事件 - 选中预设
    /// </summary>
    private void PresetCard_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
    {
        if (sender is Border border && border.DataContext is ConfigPreset preset)
        {
            if (DataContext is PresetPageViewModel viewModel)
            {
                viewModel.SelectedPreset = preset;
            }
        }
    }
}
