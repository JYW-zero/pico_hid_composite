using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 榧犳爣璁剧疆椤甸潰瑙嗗浘妯″瀷
/// </summary>
public partial class MousePageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
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
    /// 褰撳墠 DPI 鏂囨湰
    /// </summary>
    public string CurrentDpiText
    {
        get
        {
            int[] dpiValues = { 400, 800, 1600, 3200 };
            if (CurrentDpiIndex >= 0 && CurrentDpiIndex < dpiValues.Length)
                return $"{dpiValues[CurrentDpiIndex]} DPI";
            return "鏈煡";
        }
    }

    public MousePageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 浠庡綋鍓嶉厤缃姞杞?
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

        // 鍏堟洿鏂版湰鍦版樉绀?
        CurrentDpiIndex = index;
        OnPropertyChanged(nameof(CurrentDpiText));

        if (_deviceService.IsConnected)
        {
            IsApplying = true;
            try
            {
                // 灏濊瘯鍐欏叆璁惧锛屽け璐ヤ篃涓嶅奖鍝嶆湰鍦版樉绀?
                await _deviceService.SetDpiAsync(index);
            }
            catch
            {
                // 蹇界暐鍐欏叆閿欒
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
            // 蹇界暐淇濆瓨閿欒
        }
        finally
        {
            _isSaving = false;
        }
    }
}

