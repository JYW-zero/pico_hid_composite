using System.IO;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Extensions.DependencyInjection;

namespace HidConfigTool.App.Views;

public partial class DevicePage : UserControl
{
    public DevicePageViewModel ViewModel { get; }

    public DevicePage(DevicePageViewModel viewModel)
    {
        InitializeComponent();
        ViewModel = viewModel;
        DataContext = ViewModel;
    }

    /// <summary>
    /// 拖拽进入时显示效果
    /// </summary>
    private void DevicePage_DragOver(object sender, DragEventArgs e)
    {
        if (e.Data.GetDataPresent(DataFormats.FileDrop))
        {
            e.Effects = DragDropEffects.Copy;
        }
        else
        {
            e.Effects = DragDropEffects.None;
        }
        e.Handled = true;
    }

    /// <summary>
    /// 拖拽释放时导入配置
    /// </summary>
    private async void DevicePage_Drop(object sender, DragEventArgs e)
    {
        if (!e.Data.GetDataPresent(DataFormats.FileDrop))
            return;

        string[]? files = e.Data.GetData(DataFormats.FileDrop) as string[];
        if (files == null || files.Length == 0)
            return;

        // 只处理第一个文件
        string filePath = files[0];

        // 验证文件扩展名
        string ext = Path.GetExtension(filePath).ToLowerInvariant();
        if (ext != ".json" && ext != ".bin" && ext != ".hidcfg")
        {
            MessageBox.Show($"不支持的文件格式: {ext}\n支持的格式: .json, .bin, .hidcfg",
                "导入配置", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        // 导入配置
        await ViewModel.ImportConfigFromFileAsync(filePath);
    }
}
