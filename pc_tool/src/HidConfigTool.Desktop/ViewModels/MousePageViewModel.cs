using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.ViewModels;

public partial class MousePageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private bool _ready;
    private bool _saving;

    [ObservableProperty] private int _currentDpiIndex = 1;
    [ObservableProperty] private bool _accelerationEnabled;
    [ObservableProperty] private double _accelerationThreshold = 10;
    [ObservableProperty] private double _accelerationRatio = 1.5;
    [ObservableProperty] private bool _isApplying;
    [ObservableProperty] private string _statusMessage = string.Empty;

    public string CurrentDpiText
    {
        get
        {
            int[] values = { 400, 800, 1600, 3200 };
            return CurrentDpiIndex is >= 0 and < 4 ? $"{values[CurrentDpiIndex]} DPI" : "未知";
        }
    }

    public MousePageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        if (_deviceService.CurrentConfig != null)
        {
            CurrentDpiIndex = _deviceService.CurrentConfig.DpiIndex;
            AccelerationEnabled = _deviceService.CurrentConfig.AccelerationEnabled;
            AccelerationThreshold = _deviceService.CurrentConfig.AccelerationThreshold;
            AccelerationRatio = _deviceService.CurrentConfig.AccelerationRatio;
        }
        _ready = true;
    }

    partial void OnCurrentDpiIndexChanged(int value) => OnPropertyChanged(nameof(CurrentDpiText));
    partial void OnAccelerationEnabledChanged(bool value) { if (_ready && !_saving) _ = SaveAccelerationAsync(); }
    partial void OnAccelerationThresholdChanged(double value) { if (_ready && !_saving) _ = SaveAccelerationAsync(); }
    partial void OnAccelerationRatioChanged(double value) { if (_ready && !_saving) _ = SaveAccelerationAsync(); }

    [RelayCommand]
    private async Task SetDpiAsync(string? indexStr)
    {
        if (!int.TryParse(indexStr, out int index) || index is < 0 or > 3)
            return;
        CurrentDpiIndex = index;
        if (!_deviceService.IsConnected)
            return;
        IsApplying = true;
        try
        {
            bool ok = await _deviceService.SetDpiAsync(index);
            StatusMessage = ok ? $"已设置 {CurrentDpiText}" : "DPI 写入失败";
        }
        finally
        {
            IsApplying = false;
        }
    }

    private async Task SaveAccelerationAsync()
    {
        if (!_deviceService.IsConnected || _saving)
            return;
        _saving = true;
        try
        {
            await _deviceService.SetAccelerationAsync(AccelerationEnabled, AccelerationThreshold, AccelerationRatio);
        }
        finally
        {
            _saving = false;
        }
    }
}
