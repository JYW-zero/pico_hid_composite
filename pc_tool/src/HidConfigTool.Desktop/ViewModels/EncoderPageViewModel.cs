using CommunityToolkit.Mvvm.ComponentModel;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.ViewModels;

public partial class EncoderPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private readonly bool _ready;
    private bool _saving;

    [ObservableProperty] private bool _reverseDirection;
    [ObservableProperty] private decimal _stepsPerTick = 1;
    [ObservableProperty] private decimal _scrollSpeed = 3;

    public EncoderPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        if (_deviceService.CurrentConfig != null)
        {
            ReverseDirection = _deviceService.CurrentConfig.EncoderReverse;
            StepsPerTick = _deviceService.CurrentConfig.EncoderStepsPerTick;
            ScrollSpeed = _deviceService.CurrentConfig.EncoderScrollSpeed;
        }
        _ready = true;
    }

    partial void OnReverseDirectionChanged(bool value)
    {
        if (_ready && !_saving)
            _ = SaveAsync();
    }

    partial void OnStepsPerTickChanged(decimal value)
    {
        if (_ready && _deviceService.CurrentConfig != null)
            _deviceService.CurrentConfig.EncoderStepsPerTick = (int)value;
    }

    partial void OnScrollSpeedChanged(decimal value)
    {
        if (_ready && _deviceService.CurrentConfig != null)
            _deviceService.CurrentConfig.EncoderScrollSpeed = (int)value;
    }

    private async Task SaveAsync()
    {
        if (!_deviceService.IsConnected || _saving)
            return;
        _saving = true;
        try
        {
            await _deviceService.SetEncoderReverseAsync(ReverseDirection);
        }
        finally
        {
            _saving = false;
        }
    }
}
