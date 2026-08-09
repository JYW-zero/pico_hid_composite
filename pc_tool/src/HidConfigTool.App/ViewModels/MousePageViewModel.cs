using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.App.Services;
using HidConfigTool.Core.Models;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 鼠标设置页面视图模型
/// </summary>
public partial class MousePageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private readonly OsdManager _osdManager;
    private bool _isInitialized;
    private bool _isSaving;

    [ObservableProperty]
    private int _currentDpiIndex = 1;

    [ObservableProperty]
    private bool _accelerationEnabled;

    [ObservableProperty]
    private double _accelerationThreshold = 10;

    [ObservableProperty]
    private double _accelerationRatio = 1.5;

    [ObservableProperty]
    private bool _isApplying;

    /// <summary>
    /// 当前 DPI 文本
    /// </summary>
    public string CurrentDpiText
    {
        get
        {
            int[] dpiValues = { 400, 800, 1600, 3200 };
            if (CurrentDpiIndex >= 0 && CurrentDpiIndex < dpiValues.Length)
                return $"{dpiValues[CurrentDpiIndex]} DPI";
            return "未知";
        }
    }

    public MousePageViewModel(IDeviceService deviceService, OsdManager osdManager)
    {
        _deviceService = deviceService;
        _osdManager = osdManager;

        // 从当前配置加载
        if (_deviceService.IsConnected && _deviceService.CurrentConfig != null)
        {
            CurrentDpiIndex = _deviceService.CurrentConfig.DpiIndex;
            AccelerationEnabled = _deviceService.CurrentConfig.AccelerationEnabled;
            AccelerationThreshold = _deviceService.CurrentConfig.AccelerationThreshold;
            AccelerationRatio = _deviceService.CurrentConfig.AccelerationRatio;
        }

        _isInitialized = true;
    }

    [RelayCommand]
    private async Task SetDpiAsync(object? parameter)
    {
        if (parameter is not string indexStr || !int.TryParse(indexStr, out int index))
            return;

        if (index < 0 || index > 3)
            return;

        // 先更新本地显示
        CurrentDpiIndex = index;
        OnPropertyChanged(nameof(CurrentDpiText));

        if (_deviceService.IsConnected)
        {
            IsApplying = true;
            try
            {
                // 尝试写入设备，失败也不影响本地显示
                await _deviceService.SetDpiAsync(index);
            }
            catch
            {
                // 忽略写入错误
            }
            finally
            {
                IsApplying = false;
            }
        }
    }

    partial void OnAccelerationEnabledChanged(bool value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveAccelerationAsync();
    }

    partial void OnAccelerationThresholdChanged(double value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveAccelerationAsync();
    }

    partial void OnAccelerationRatioChanged(double value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveAccelerationAsync();
    }

    private async Task SaveAccelerationAsync()
    {
        if (!_deviceService.IsConnected || _isSaving)
            return;

        try
        {
            _isSaving = true;
            await _deviceService.SetAccelerationAsync(
                AccelerationEnabled,
                AccelerationThreshold,
                AccelerationRatio);
        }
        catch
        {
            // 忽略保存错误
        }
        finally
        {
            _isSaving = false;
        }
    }
}

