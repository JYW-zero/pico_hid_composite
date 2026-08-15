using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.ViewModels;

public partial class SettingsPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;

    [ObservableProperty] private string _statusMessage = string.Empty;
    [ObservableProperty] private string _exportPath = string.Empty;
    [ObservableProperty] private string _importPath = string.Empty;

    public SettingsPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
    }

    [RelayCommand]
    private async Task ExportAsync()
    {
        if (string.IsNullOrWhiteSpace(ExportPath))
        {
            StatusMessage = "请填写导出路径";
            return;
        }

        bool ok = await _deviceService.ExportConfigAsync(ExportPath);
        StatusMessage = ok ? "配置已导出" : "导出失败";
    }

    [RelayCommand]
    private async Task ImportAsync()
    {
        if (string.IsNullOrWhiteSpace(ImportPath))
        {
            StatusMessage = "请填写导入路径";
            return;
        }

        bool ok = await _deviceService.ImportConfigAsync(ImportPath);
        StatusMessage = ok ? "配置已导入并写入设备" : "导入失败";
    }

    [RelayCommand]
    private async Task ResetAsync()
    {
        bool ok = await _deviceService.ResetConfigAsync();
        StatusMessage = ok ? "已恢复默认配置" : "恢复默认失败";
    }
}
