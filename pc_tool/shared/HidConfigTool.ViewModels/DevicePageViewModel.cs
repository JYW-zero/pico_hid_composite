using System.Collections.ObjectModel;
using System.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Services;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 设备页面视图模型
/// </summary>
public partial class DevicePageViewModel : PageViewModelBase
{
    private readonly MainViewModel _mainViewModel;
    private readonly IFileDialogService _fileDialogService;
    private readonly ConfigImportExportService _configImportExport;

    /// <summary>
    /// 设备列表
    /// </summary>
    public ObservableCollection<HidDeviceInfo> Devices { get; } = new();

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(CanConnect))]
    private HidDeviceInfo? _selectedDevice;

    [ObservableProperty]
    private bool _isRefreshing;

    [ObservableProperty]
    private bool _isConnecting;

    [ObservableProperty]
    private string _statusMessage = "未连接设备";

    /// <summary>
    /// 是否可以连接（选中了设备且未连接）
    /// </summary>
    public bool CanConnect => !IsDeviceConnected && SelectedDevice != null;

    /// <summary>
    /// 固件版本
    /// </summary>
    public string FirmwareVersion => DeviceService.FirmwareVersion ?? "—";

    /// <summary>
    /// 设备名称
    /// </summary>
    public string DeviceName => DeviceService.DeviceName ?? "—";

    public DevicePageViewModel(IDeviceService deviceService, MainViewModel mainViewModel,
        IFileDialogService fileDialogService, ConfigImportExportService configImportExport)
        : base(deviceService)
    {
        _mainViewModel = mainViewModel;
        _fileDialogService = fileDialogService;
        _configImportExport = configImportExport;

        // 如果已经连接，更新状态
        if (DeviceService.IsConnected)
        {
            StatusMessage = "已连接";
            OnPropertyChanged(nameof(FirmwareVersion));
            OnPropertyChanged(nameof(DeviceName));
        }

        // 页面加载时自动刷新设备列表
        _ = RefreshAsync();
    }

    /// <summary>
    /// 设备已连接（重写基类虚方法）
    /// </summary>
    protected override async Task OnDeviceConnectedAsync()
    {
        StatusMessage = "已连接";
        _mainViewModel.NotifyDeviceConnected();
        OnPropertyChanged(nameof(FirmwareVersion));
        OnPropertyChanged(nameof(DeviceName));

        // 延迟300ms确保设备完全枚举，然后刷新设备列表
        await Task.Delay(300);
        await RunOnUiThreadAsync(RefreshAsync);
    }

    /// <summary>
    /// 设备已断开（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceDisconnectedAsync()
    {
        StatusMessage = "已断开连接";
        _mainViewModel.NotifyDeviceDisconnected();
        return Task.CompletedTask;
    }

    [RelayCommand]
    private async Task RefreshAsync()
    {
        Devices.Clear();

        try
        {
            var devices = await DeviceService.GetDevicesAsync();
            foreach (var device in devices)
            {
                Devices.Add(device);
            }

            StatusMessage = $"发现 {devices.Count} 个设备";

            // 如果有设备且当前未连接，自动连接第一个
            if (devices.Count > 0 && !DeviceService.IsConnected)
            {
                SelectedDevice = devices[0];
                await ConnectAsync();
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"刷新失败: {ex.Message}";
        }
    }

    [RelayCommand]
    private async Task ConnectAsync()
    {
        if (SelectedDevice == null || IsConnecting)
            return;

        IsConnecting = true;
        StatusMessage = "正在连接...";

        try
        {
            bool result = await DeviceService.ConnectAsync(SelectedDevice);
            if (result)
            {
                // ConnectAsync 内部会触发 DeviceConnectionChanged 事件，这里不需要重复设置
                StatusMessage = "连接成功";
            }
            else
            {
                StatusMessage = "连接失败";
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"连接失败: {ex.Message}";
        }
        finally
        {
            IsConnecting = false;
        }
    }

    [RelayCommand]
    private void Disconnect()
    {
        DeviceService.Disconnect();
        // Disconnect 内部会触发 DeviceConnectionChanged 事件
    }

    [RelayCommand]
    private async Task RebootAsync()
    {
        if (!IsDeviceConnected)
            return;

        StatusMessage = "正在重启设备...";
        bool result = await DeviceService.RebootAsync();

        if (result)
        {
            StatusMessage = "重启命令已发送，设备正在重启...";
        }
        else
        {
            StatusMessage = "重启失败";
        }
    }

    [RelayCommand]
    private async Task EnterBootselAsync()
    {
        if (!IsDeviceConnected)
            return;

        StatusMessage = "正在进入烧录模式...";
        bool result = await DeviceService.EnterBootselAsync();

        if (result)
        {
            StatusMessage = "设备已进入烧录模式";
        }
        else
        {
            StatusMessage = "进入烧录模式失败";
        }
    }

    /// <summary>
    /// 导出配置为 JSON 文件
    /// </summary>
    [RelayCommand]
    private async Task ExportConfigAsync()
    {
        if (!IsDeviceConnected || DeviceService.CurrentConfig == null)
        {
            StatusMessage = "未连接设备或无配置可导出";
            return;
        }

        try
        {
            // 打开保存文件对话框
            string defaultFileName = $"hid_config_{DateTime.Now:yyyyMMdd_HHmmss}.json";
            string? filePath = _fileDialogService.SaveFile("导出配置",
                ConfigImportExportService.GetFileFilter(), ".json");

            if (string.IsNullOrEmpty(filePath))
                return;

            StatusMessage = "正在导出配置...";

            // 导出为 JSON
            bool success = await _configImportExport.ExportToJsonAsync(DeviceService.CurrentConfig, filePath);

            if (success)
            {
                StatusMessage = $"配置已导出到: {Path.GetFileName(filePath)}";
            }
            else
            {
                StatusMessage = "导出配置失败";
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"导出配置异常: {ex.Message}";
        }
    }

    /// <summary>
    /// 从文件导入配置
    /// </summary>
    [RelayCommand]
    private async Task ImportConfigAsync()
    {
        if (!IsDeviceConnected)
        {
            StatusMessage = "未连接设备";
            return;
        }

        try
        {
            // 打开文件对话框
            string? filePath = _fileDialogService.OpenFile("导入配置",
                ConfigImportExportService.GetFileFilter());

            if (string.IsNullOrEmpty(filePath))
                return;

            await ImportConfigFromFileAsync(filePath);
        }
        catch (Exception ex)
        {
            StatusMessage = $"导入配置异常: {ex.Message}";
        }
    }

    /// <summary>
    /// 从指定文件路径导入配置（用于拖拽导入）
    /// </summary>
    public async Task ImportConfigFromFileAsync(string filePath)
    {
        if (!IsDeviceConnected)
        {
            StatusMessage = "未连接设备";
            return;
        }

        if (!File.Exists(filePath))
        {
            StatusMessage = "文件不存在";
            return;
        }

        try
        {
            StatusMessage = "正在导入配置...";

            // 根据文件格式选择导入方式
            if (_configImportExport.IsJsonFile(filePath))
            {
                // JSON 格式导入
                var result = await _configImportExport.ImportFromJsonAsync(filePath);
                if (result == null || !result.Success || result.Config == null)
                {
                    StatusMessage = $"导入失败: {result?.ErrorMessage ?? "未知错误"}";
                    return;
                }

                // 版本警告
                if (result.HasVersionWarning)
                {
                    StatusMessage = $"警告: 配置版本 v{result.Version} 高于当前支持版本，部分字段可能不兼容";
                }

                // 写入设备
                bool writeSuccess = await DeviceService.SaveConfigAsync(result.Config);
                if (writeSuccess)
                {
                    StatusMessage = $"配置已导入并写入设备（v{result.Version}）";
                }
                else
                {
                    StatusMessage = "配置导入成功，但写入设备失败";
                }
            }
            else
            {
                // 二进制格式导入（使用 DeviceService 的 ImportConfigAsync）
                bool success = await DeviceService.ImportConfigAsync(filePath);
                if (success)
                {
                    StatusMessage = $"配置已导入并写入设备: {Path.GetFileName(filePath)}";
                }
                else
                {
                    StatusMessage = "导入配置失败（文件格式错误或校验失败）";
                }
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"导入配置异常: {ex.Message}";
        }
    }

}
