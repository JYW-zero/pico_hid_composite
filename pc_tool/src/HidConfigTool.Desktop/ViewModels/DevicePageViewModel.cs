using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.ViewModels;

public partial class DevicePageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;

    public ObservableCollection<HidDeviceInfo> Devices { get; } = new();

    [ObservableProperty] private HidDeviceInfo? _selectedDevice;
    [ObservableProperty] private bool _isRefreshing;
    [ObservableProperty] private bool _isConnecting;
    [ObservableProperty] private bool _isConnected;
    [ObservableProperty] private string _statusMessage = "未连接设备";
    [ObservableProperty] private string _firmwareVersion = "—";
    [ObservableProperty] private string _deviceName = "—";

    public DevicePageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        if (_deviceService.IsConnected)
        {
            IsConnected = true;
            StatusMessage = "已连接";
            FirmwareVersion = _deviceService.FirmwareVersion ?? "—";
            DeviceName = _deviceService.DeviceName ?? "—";
        }

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
                Devices.Add(device);

            StatusMessage = $"发现 {devices.Count} 个配置接口";
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
            bool ok = await _deviceService.ConnectAsync(SelectedDevice);
            IsConnected = ok;
            StatusMessage = ok ? "连接成功" : "连接失败";
            FirmwareVersion = _deviceService.FirmwareVersion ?? "—";
            DeviceName = _deviceService.DeviceName ?? "—";
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
        FirmwareVersion = "—";
        DeviceName = "—";
    }

    [RelayCommand]
    private async Task RebootAsync()
    {
        if (!IsConnected)
            return;
        StatusMessage = await _deviceService.RebootAsync() ? "重启命令已发送" : "重启失败";
    }

    [RelayCommand]
    private async Task EnterBootselAsync()
    {
        if (!IsConnected)
            return;
        bool ok = await _deviceService.EnterBootselAsync();
        StatusMessage = ok ? "已进入烧录模式" : "进入烧录模式失败";
        if (ok)
        {
            IsConnected = false;
            FirmwareVersion = "—";
            DeviceName = "—";
        }
    }
}
