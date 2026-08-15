using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.ViewModels;

public partial class KeyStatItemViewModel : ObservableObject
{
    [ObservableProperty] private int _index;
    [ObservableProperty] private string _keyName = string.Empty;
    [ObservableProperty] private uint _count;
}

public partial class StatsPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;

    public ObservableCollection<KeyStatItemViewModel> Items { get; } = new();

    [ObservableProperty] private long _totalKeystrokes;
    [ObservableProperty] private string _statusMessage = string.Empty;
    [ObservableProperty] private bool _isLoading;

    public StatsPageViewModel(IDeviceService deviceService)
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
            uint[]? stats = await _deviceService.GetKeyStatsAsync();
            Items.Clear();
            long total = 0;
            if (stats != null)
            {
                for (int i = 0; i < stats.Length && i < 64; i++)
                {
                    total += stats[i];
                    Items.Add(new KeyStatItemViewModel
                    {
                        Index = i,
                        KeyName = $"键 {i + 1}",
                        Count = stats[i]
                    });
                }
            }
            TotalKeystrokes = total;
            StatusMessage = $"共 {total} 次按键";
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
        bool ok = await _deviceService.ResetKeyStatsAsync();
        StatusMessage = ok ? "统计已清零" : "清零失败";
        if (ok)
            await RefreshAsync();
    }
}
