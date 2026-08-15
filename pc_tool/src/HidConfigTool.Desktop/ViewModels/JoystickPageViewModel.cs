using CommunityToolkit.Mvvm.ComponentModel;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.ViewModels;

public partial class JoystickPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private readonly bool _ready;
    private bool _saving;

    [ObservableProperty] private double _deadzone = 100;
    [ObservableProperty] private double _sensitivity = 1.0;
    [ObservableProperty] private bool _invertX;
    [ObservableProperty] private bool _invertY;

    public JoystickPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        if (_deviceService.CurrentConfig != null)
        {
            Deadzone = _deviceService.CurrentConfig.JoystickDeadzone;
            Sensitivity = _deviceService.CurrentConfig.JoystickSensitivity;
            InvertX = _deviceService.CurrentConfig.JoystickInvertX;
            InvertY = _deviceService.CurrentConfig.JoystickInvertY;
        }
        _ready = true;
    }

    partial void OnDeadzoneChanged(double value)
    {
        if (_ready && !_saving)
            _ = SaveAsync();
    }

    partial void OnSensitivityChanged(double value)
    {
        if (_ready && _deviceService.CurrentConfig != null)
            _deviceService.CurrentConfig.JoystickSensitivity = value;
    }

    partial void OnInvertXChanged(bool value)
    {
        if (_ready && _deviceService.CurrentConfig != null)
            _deviceService.CurrentConfig.JoystickInvertX = value;
    }

    partial void OnInvertYChanged(bool value)
    {
        if (_ready && _deviceService.CurrentConfig != null)
            _deviceService.CurrentConfig.JoystickInvertY = value;
    }

    private async Task SaveAsync()
    {
        if (!_deviceService.IsConnected || _saving)
            return;
        _saving = true;
        try
        {
            await _deviceService.SetJoystickDeadzoneAsync((ushort)Deadzone);
        }
        finally
        {
            _saving = false;
        }
    }
}
