using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 摇杆设置页面视图模型
/// </summary>
public partial class JoystickPageViewModel : ObservableObject, IDisposable
{
    private readonly IDeviceService _deviceService;
    private readonly DispatcherTimer _refreshTimer;
    private bool _disposed;
    private bool _isInitialized;
    private bool _isApplyingRealtime;

    [ObservableProperty]
    private double _deadzone = 100;

    [ObservableProperty]
    private double _sensitivity = 1.0;

    [ObservableProperty]
    private bool _invertX;

    [ObservableProperty]
    private bool _invertY;

    /// <summary>
    /// 实时摇杆X值（-127到127）
    /// </summary>
    [ObservableProperty]
    private double _joystickX;

    /// <summary>
    /// 实时摇杆Y值（-127到127）
    /// </summary>
    [ObservableProperty]
    private double _joystickY;

    /// <summary>
    /// 摇杆按键是否按下
    /// </summary>
    [ObservableProperty]
    private bool _joystickButton;

    /// <summary>
    /// 是否有设备连接
    /// </summary>
    [ObservableProperty]
    private bool _isDeviceConnected;

    /// <summary>
    /// 是否正在保存
    /// </summary>
    [ObservableProperty]
    private bool _isSaving;

    /// <summary>
    /// 状态消息
    /// </summary>
    [ObservableProperty]
    private string _statusMessage = "";

    /// <summary>
    /// 坐标图中点的X位置（百分比，0-100）
    /// </summary>
    public double DotX => 50 + (JoystickX / 127.0) * 45;

    /// <summary>
    /// 坐标图中点的Y位置（百分比，0-100）
    /// </summary>
    public double DotY => 50 - (JoystickY / 127.0) * 45;

    /// <summary>
    /// 死区圆的半径（百分比）
    /// </summary>
    public double DeadzoneRadius => (Deadzone / 2048.0) * 45;

    /// <summary>
    /// 死区圆的宽度（像素，坐标图220px）
    /// </summary>
    public double DeadzoneCircleWidth => (Deadzone / 2048.0) * 198;

    /// <summary>
    /// 死区圆的左边距（居中）
    /// </summary>
    public double DeadzoneCircleLeft => (220 - DeadzoneCircleWidth) / 2;

    /// <summary>
    /// 死区圆的上边距（居中）
    /// </summary>
    public double DeadzoneCircleTop => (220 - DeadzoneCircleWidth) / 2;

    /// <summary>
    /// 摇杆点的左边距（像素）
    /// </summary>
    public double DotLeft => (DotX / 100.0) * 220 - 8;

    /// <summary>
    /// 摇杆点的上边距（像素）
    /// </summary>
    public double DotTop => (DotY / 100.0) * 220 - 8;

    /// <summary>
    /// 摇杆点的Margin
    /// </summary>
    public System.Windows.Thickness DotMargin => new(DotLeft, DotTop, 0, 0);

    public JoystickPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        _deviceService.DeviceConnectionChanged += OnDeviceConnectionChanged;

        IsDeviceConnected = _deviceService.IsConnected;

        // 从当前配置加载
        if (_deviceService.IsConnected && _deviceService.CurrentConfig != null)
        {
            Deadzone = _deviceService.CurrentConfig.JoystickDeadzone;
            Sensitivity = _deviceService.CurrentConfig.JoystickSensitivity;
            InvertX = _deviceService.CurrentConfig.JoystickInvertX;
            InvertY = _deviceService.CurrentConfig.JoystickInvertY;
        }

        _refreshTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(50)
        };
        _refreshTimer.Tick += OnRefreshTimerTick;

        if (IsDeviceConnected)
        {
            _refreshTimer.Start();
        }

        _isInitialized = true;
    }

    /// <summary>
    /// 设备连接状态变化
    /// </summary>
    private void OnDeviceConnectionChanged(object? sender, bool isConnected)
    {
        IsDeviceConnected = isConnected;

        if (isConnected)
        {
            _refreshTimer.Start();
        }
        else
        {
            _refreshTimer.Stop();
            JoystickX = 0;
            JoystickY = 0;
            JoystickButton = false;
        }
    }

    /// <summary>
    /// 刷新定时器tick
    /// </summary>
    private async void OnRefreshTimerTick(object? sender, EventArgs e)
    {
        if (!IsDeviceConnected)
            return;

        try
        {
            var state = await _deviceService.GetJoystickStateAsync();
            if (state.HasValue)
            {
                JoystickX = state.Value.X;
                JoystickY = state.Value.Y;
                JoystickButton = state.Value.Button;
            }
        }
        catch
        {
            // 忽略读取错误
        }
    }

    partial void OnDeadzoneChanged(double value)
    {
        OnPropertyChanged(nameof(DeadzoneCircleWidth));
        OnPropertyChanged(nameof(DeadzoneCircleLeft));
        OnPropertyChanged(nameof(DeadzoneCircleTop));
        if (_isInitialized && !_isApplyingRealtime)
        {
            _ = ApplyDeadzoneRealtimeAsync();
        }
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

    partial void OnJoystickXChanged(double value)
    {
        OnPropertyChanged(nameof(DotX));
        OnPropertyChanged(nameof(DotLeft));
        OnPropertyChanged(nameof(DotMargin));
    }

    partial void OnJoystickYChanged(double value)
    {
        OnPropertyChanged(nameof(DotY));
        OnPropertyChanged(nameof(DotTop));
        OnPropertyChanged(nameof(DotMargin));
    }

    /// <summary>
    /// 实时应用死区（不写Flash）
    /// </summary>
    private async Task ApplyDeadzoneRealtimeAsync()
    {
        if (!_deviceService.IsConnected)
            return;

        try
        {
            _isApplyingRealtime = true;
            bool result = await _deviceService.SetJoystickDeadzoneRealtimeAsync((ushort)Deadzone);
            if (result)
            {
                StatusMessage = $"死区已实时应用: {Deadzone:F0}（未保存到Flash）";
            }
            else
            {
                StatusMessage = "死区应用失败";
            }
        }
        catch
        {
            StatusMessage = "死区应用异常";
        }
        finally
        {
            _isApplyingRealtime = false;
        }
    }

    /// <summary>
    /// 保存配置到Flash
    /// </summary>
    [RelayCommand]
    private async Task SaveConfigAsync()
    {
        if (!_deviceService.IsConnected || IsSaving)
            return;

        try
        {
            IsSaving = true;
            StatusMessage = "正在保存到Flash...";

            // 先实时应用当前值
            await _deviceService.SetJoystickDeadzoneRealtimeAsync((ushort)Deadzone);

            // 然后保存到Flash
            bool result = await _deviceService.SetJoystickDeadzoneAsync((ushort)Deadzone);
            if (result)
            {
                StatusMessage = $"配置已保存到Flash: 死区={Deadzone:F0}";
            }
            else
            {
                StatusMessage = "保存失败";
            }
        }
        catch
        {
            StatusMessage = "保存异常";
        }
        finally
        {
            IsSaving = false;
        }
    }

    /// <summary>
    /// 释放资源，取消事件订阅和定时器，防止内存泄漏
    /// </summary>
    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _refreshTimer.Stop();
        _refreshTimer.Tick -= OnRefreshTimerTick;
        _deviceService.DeviceConnectionChanged -= OnDeviceConnectionChanged;
    }
}
