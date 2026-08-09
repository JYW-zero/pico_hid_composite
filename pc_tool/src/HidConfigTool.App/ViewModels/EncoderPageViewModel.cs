using CommunityToolkit.Mvvm.ComponentModel;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 编码器设置页面视图模型
/// </summary>
public partial class EncoderPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private bool _isInitialized;
    private bool _isSaving;

    [ObservableProperty]
    private bool _reverseDirection;

    [ObservableProperty]
    private int _stepsPerTick = 1;

    [ObservableProperty]
    private int _scrollSpeed = 3;

    public EncoderPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 从当前配置加载
        if (_deviceService.IsConnected && _deviceService.CurrentConfig != null)
        {
            ReverseDirection = _deviceService.CurrentConfig.EncoderReverse;
        }

        _isInitialized = true;
    }

    partial void OnReverseDirectionChanged(bool value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveDirectionAsync();
    }

    private async Task SaveDirectionAsync()
    {
        if (!_deviceService.IsConnected || _isSaving)
            return;

        try
        {
            _isSaving = true;
            await _deviceService.SetEncoderReverseAsync(ReverseDirection);
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
