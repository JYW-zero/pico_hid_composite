using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Desktop.ViewModels;

public partial class ErrorLogPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;

    public ObservableCollection<ErrorLogEntry> Logs { get; } = new();

    [ObservableProperty] private uint _totalLogCount;
    [ObservableProperty] private uint _totalFaultCount;
    [ObservableProperty] private bool _isLoading;
    [ObservableProperty] private string _statusMessage = string.Empty;
    [ObservableProperty] private ErrorLogEntry? _selectedLog;

    public ErrorLogPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        _ = RefreshAsync();
    }

    [RelayCommand]
    private async Task RefreshAsync()
    {
        if (!_deviceService.IsConnected)
        {
            StatusMessage = "设备未连接";
            return;
        }

        IsLoading = true;
        try
        {
            var info = await _deviceService.GetErrorLogInfoAsync();
            TotalLogCount = info?.LogCount ?? 0;
            TotalFaultCount = info?.TotalFaultCount ?? 0;

            var logs = await _deviceService.GetAllErrorLogsAsync();
            Logs.Clear();
            if (logs != null)
            {
                foreach (var log in logs)
                    Logs.Add(log);
            }
            StatusMessage = $"已读取 {Logs.Count} 条日志";
        }
        finally
        {
            IsLoading = false;
        }
    }

    [RelayCommand]
    private async Task ClearAsync()
    {
        if (!_deviceService.IsConnected)
            return;
        bool ok = await _deviceService.ClearErrorLogsAsync();
        StatusMessage = ok ? "日志已清除" : "清除失败";
        if (ok)
            await RefreshAsync();
    }
}
