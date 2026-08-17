using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 鼠标设置页面视图模型
/// </summary>
public partial class MousePageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private bool _isInitialized;
    private bool _isSaving;
    private CancellationTokenSource? _dpiDebounceCts;

    [ObservableProperty]
    private int _currentDpiIndex = 1;

    [ObservableProperty]
    private double _currentDpi = 800;

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
    public string CurrentDpiText => $"{(int)CurrentDpi} DPI";

    public MousePageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 从当前配置加载
        if (_deviceService.IsConnected && _deviceService.CurrentConfig != null)
        {
            CurrentDpi = _deviceService.CurrentConfig.Dpi;
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

        int[] dpiValues = { 400, 800, 1600, 3200 };
        double dpi = dpiValues[index];

        // 先更新本地显示
        CurrentDpiIndex = index;
        CurrentDpi = dpi;

        // 取消待处理的 debounce
        _dpiDebounceCts?.Cancel();

        if (_deviceService.IsConnected)
        {
            IsApplying = true;
            try
            {
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

    partial void OnCurrentDpiChanged(double value)
    {
        if (!_isInitialized || _isSaving) return;

        // 检查是否匹配预设档位
        int[] dpiValues = { 400, 800, 1600, 3200 };
        int idx = Array.IndexOf(dpiValues, (int)value);
        if (idx >= 0) CurrentDpiIndex = idx;

        if (_deviceService.IsConnected)
        {
            // debounce: 延迟300ms写入，避免拖动滑块时频繁写Flash
            _dpiDebounceCts?.Cancel();
            _dpiDebounceCts = new CancellationTokenSource();
            var token = _dpiDebounceCts.Token;
            _ = Task.Run(async () =>
            {
                try
                {
                    await Task.Delay(300, token);
                    if (!token.IsCancellationRequested)
                    {
                        await SaveCustomDpiAsync((ushort)value);
                    }
                }
                catch (TaskCanceledException) { }
            }, token);
        }
    }

    private async Task SaveCustomDpiAsync(ushort dpi)
    {
        if (!_deviceService.IsConnected || _isSaving) return;

        try
        {
            _isSaving = true;
            await _deviceService.SetDpiValueAsync(dpi);
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
