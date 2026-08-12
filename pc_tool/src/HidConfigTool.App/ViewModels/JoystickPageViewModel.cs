using CommunityToolkit.Mvvm.ComponentModel;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 摇杆设置页面视图模型
/// </summary>
public partial class JoystickPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private bool _isInitialized;
    private bool _isSaving;

    [ObservableProperty]
    private double _deadzone = 100;

    [ObservableProperty]
    private double _sensitivity = 1.0;

    [ObservableProperty]
    private bool _invertX;

    [ObservableProperty]
    private bool _invertY;

    public JoystickPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 从当前配置加载
        if (_deviceService.IsConnected && _deviceService.CurrentConfig != null)
        {
            Deadzone = _deviceService.CurrentConfig.JoystickDeadzone;
            Sensitivity = _deviceService.CurrentConfig.JoystickSensitivity;
            InvertX = _deviceService.CurrentConfig.JoystickInvertX;
            InvertY = _deviceService.CurrentConfig.JoystickInvertY;
        }

        _isInitialized = true;
    }

    partial void OnDeadzoneChanged(double value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveDeadzoneAsync();
    }

    partial void OnSensitivityChanged(double value)
    {
        if (_isInitialized && _deviceService.CurrentConfig != null)
            _deviceService.CurrentConfig.JoystickSensitivity = value;
    }

    partial void OnInvertXChanged(bool value)
    {
        if (_isInitialized && _deviceService.CurrentConfig != null)
            _deviceService.CurrentConfig.JoystickInvertX = value;
    }

    partial void OnInvertYChanged(bool value)
    {
        if (_isInitialized && _deviceService.CurrentConfig != null)
            _deviceService.CurrentConfig.JoystickInvertY = value;
    }

    private async Task SaveDeadzoneAsync()
    {
        if (!_deviceService.IsConnected || _isSaving)
            return;

        try
        {
            _isSaving = true;
            await _deviceService.SetJoystickDeadzoneAsync((ushort)Deadzone);
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

