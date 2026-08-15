using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Desktop.ViewModels;

public partial class PerfPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;

    public ObservableCollection<PerfTaskStat> TaskStats { get; } = new();

    [ObservableProperty] private PerfSystemStat? _systemStat;
    [ObservableProperty] private bool _isLoading;
    [ObservableProperty] private string _statusMessage = string.Empty;

    public string CpuUsageText => SystemStat?.CpuUsage.ToString() ?? "--";
    public string LoopFreqText => SystemStat?.LoopFreqHz.ToString() ?? "--";
    public string UptimeText => SystemStat?.UptimeFormatted ?? "--";

    public PerfPageViewModel(IDeviceService deviceService)
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
            SystemStat = await _deviceService.GetPerfSystemStatAsync();
            OnPropertyChanged(nameof(CpuUsageText));
            OnPropertyChanged(nameof(LoopFreqText));
            OnPropertyChanged(nameof(UptimeText));

            TaskStats.Clear();
            byte count = SystemStat?.TaskCount ?? 0;
            for (byte i = 0; i < count; i++)
            {
                var stat = await _deviceService.GetPerfTaskStatAsync(i);
                if (stat != null)
                    TaskStats.Add(stat);
            }
            StatusMessage = "已刷新";
        }
        finally
        {
            IsLoading = false;
        }
    }

    [RelayCommand]
    private async Task ResetAsync()
    {
        if (!_deviceService.IsConnected)
            return;
        bool ok = await _deviceService.ResetPerfStatsAsync();
        StatusMessage = ok ? "性能统计已重置" : "重置失败";
        if (ok)
            await RefreshAsync();
    }
}
