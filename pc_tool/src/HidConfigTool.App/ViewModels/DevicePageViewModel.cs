using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 设备页面视图模型
/// </summary>
public partial class DevicePageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private readonly MainViewModel _mainViewModel;

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
    /// 是否已连接
    /// </summary>
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(CanConnect))]
    private bool _isConnected;

    /// <summary>
    /// 是否可以连接（选中了设备且未连接）
    /// </summary>
    public bool CanConnect => !IsConnected && SelectedDevice != null;

    /// <summary>
    /// 固件版本
    /// </summary>
    public string FirmwareVersion => _deviceService.FirmwareVersion ?? "—";

    /// <summary>
    /// 设备名称
    /// </summary>
    public string DeviceName => _deviceService.DeviceName ?? "—";

    public DevicePageViewModel(IDeviceService deviceService, MainViewModel mainViewModel)
    {
        _deviceService = deviceService;
        _mainViewModel = mainViewModel;

        // 如果已经连接，更新状态
        if (_deviceService.IsConnected)
        {
            IsConnected = true;
            StatusMessage = "已连接";
            OnPropertyChanged(nameof(FirmwareVersion));
            OnPropertyChanged(nameof(DeviceName));
        }

        // 页面加载时自动刷新设备列表
        _ = RefreshAsync();
    }

    [RelayCommand]
    private async Task RefreshAsync()
    {
        if (IsRefreshing)
            return;

        IsRefreshing = true;
        Devices.Clear();

        try
        {
            var devices = await _deviceService.GetDevicesAsync();
            foreach (var device in devices)
            {
                Devices.Add(device);
            }

            StatusMessage = $"发现 {devices.Count} 个设备";

            // 如果有设备且当前未连接，自动连接第一个
            if (devices.Count > 0 && !_deviceService.IsConnected)
            {
                SelectedDevice = devices[0];
                await ConnectAsync();
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"刷新失败: {ex.Message}";
        }
        finally
        {
            IsRefreshing = false;
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
            bool result = await _deviceService.ConnectAsync(SelectedDevice);
            if (result)
            {
                IsConnected = true;
                StatusMessage = "连接成功";
                _mainViewModel.NotifyDeviceConnected();
                OnPropertyChanged(nameof(FirmwareVersion));
                OnPropertyChanged(nameof(DeviceName));
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
        _deviceService.Disconnect();
        IsConnected = false;
        StatusMessage = "已断开连接";
        _mainViewModel.NotifyDeviceDisconnected();
        OnPropertyChanged(nameof(FirmwareVersion));
        OnPropertyChanged(nameof(DeviceName));
    }

    [RelayCommand]
    private async Task RebootAsync()
    {
        if (!IsConnected)
            return;

        StatusMessage = "正在重启设备...";
        bool result = await _deviceService.RebootAsync();

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
        if (!IsConnected)
            return;

        StatusMessage = "正在进入烧录模式...";
        bool result = await _deviceService.EnterBootselAsync();

        if (result)
        {
            IsConnected = false;
            StatusMessage = "设备已进入烧录模式";
            _mainViewModel.NotifyDeviceDisconnected();
            OnPropertyChanged(nameof(FirmwareVersion));
            OnPropertyChanged(nameof(DeviceName));
        }
        else
        {
            StatusMessage = "进入烧录模式失败";
        }
    }
}
