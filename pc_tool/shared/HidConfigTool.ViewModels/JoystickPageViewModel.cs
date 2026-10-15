using System.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 摇杆设置页面视图模型
/// </summary>
public partial class JoystickPageViewModel : PageViewModelBase
{
    private readonly ITimerService _refreshTimer;
    private bool _isInitialized;
    private CancellationTokenSource? _deadzoneDebounceCts;

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
    /// 是否正在保存
    /// </summary>
    [ObservableProperty]
    private bool _isSaving;
    private CancellationTokenSource? _configDebounceCts;

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

    public JoystickPageViewModel(IDeviceService deviceService, ITimerService timerService)
        : base(deviceService)
    {
        // 从当前配置加载
        if (DeviceService.IsConnected && DeviceService.CurrentConfig != null)
        {
            // 防御性修复：旧配置中死区可能为0（无效值），重置为默认100
            if (DeviceService.CurrentConfig.JoystickDeadzone == 0)
            {
                DeviceService.CurrentConfig.JoystickDeadzone = 100;
            }
            Deadzone = DeviceService.CurrentConfig.JoystickDeadzone;
            Sensitivity = DeviceService.CurrentConfig.JoystickSensitivity;
            InvertX = DeviceService.CurrentConfig.JoystickInvertX;
            InvertY = DeviceService.CurrentConfig.JoystickInvertY;
        }

        _refreshTimer = timerService;
        _refreshTimer.Interval = TimeSpan.FromMilliseconds(50);
        _refreshTimer.Tick += OnRefreshTimerTick;

        if (IsDeviceConnected)
        {
            _refreshTimer.Start();
        }

        _isInitialized = true;
    }

    /// <summary>
    /// 设备已连接（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceConnectedAsync()
    {
        _refreshTimer.Start();
        return Task.CompletedTask;
    }

    /// <summary>
    /// 设备已断开（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceDisconnectedAsync()
    {
        _refreshTimer.Stop();
        JoystickX = 0;
        JoystickY = 0;
        JoystickButton = false;
        return Task.CompletedTask;
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
            var state = await DeviceService.GetJoystickStateAsync();
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
        if (_isInitialized)
        {
            // 实时应用死区：debounce 50ms，确保发送最新值
            _deadzoneDebounceCts?.Cancel();
            _deadzoneDebounceCts = new CancellationTokenSource();
            var token = _deadzoneDebounceCts.Token;
            _ = Task.Run(async () =>
            {
                try
                {
                    await Task.Delay(50, token);
                    if (!token.IsCancellationRequested)
                    {
                        await ApplyDeadzoneRealtimeAsync();
                    }
                }
                catch (TaskCanceledException) { }
            });
            ScheduleConfigSave();  // 延迟保存到Flash
        }
    }

    partial void OnSensitivityChanged(double value)
    {
        if (_isInitialized && DeviceService.CurrentConfig != null)
        {
            DeviceService.CurrentConfig.JoystickSensitivity = value;
            ScheduleConfigSave();
        }
    }

    partial void OnInvertXChanged(bool value)
    {
        if (_isInitialized && DeviceService.CurrentConfig != null)
        {
            DeviceService.CurrentConfig.JoystickInvertX = value;
            ScheduleConfigSave();
        }
    }

    partial void OnInvertYChanged(bool value)
    {
        if (_isInitialized && DeviceService.CurrentConfig != null)
        {
            DeviceService.CurrentConfig.JoystickInvertY = value;
            ScheduleConfigSave();
        }
    }

    /// <summary>
    /// 延迟保存配置到设备（debounce 500ms，避免频繁写Flash）
    /// </summary>
    private void ScheduleConfigSave()
    {
        _configDebounceCts?.Cancel();
        _configDebounceCts = new CancellationTokenSource();
        var token = _configDebounceCts.Token;
        _ = Task.Run(async () =>
        {
            try
            {
                await Task.Delay(500, token);
                if (!token.IsCancellationRequested)
                {
                    await SaveFullConfigAsync();
                }
            }
            catch (TaskCanceledException) { }
        });
    }

    /// <summary>
    /// 保存完整摇杆配置到设备（死区+灵敏度+方向）
    /// </summary>
    private async Task SaveFullConfigAsync()
    {
        if (!DeviceService.IsConnected || IsSaving) return;
        try
        {
            IsSaving = true;

            // 直接从ViewModel属性更新CurrentConfig所有字段，确保保存的是当前UI值
            var cfg = DeviceService.CurrentConfig;
            if (cfg != null)
            {
                cfg.JoystickDeadzone = (ushort)Math.Clamp(Deadzone, 0, 4095);
                cfg.JoystickSensitivity = Sensitivity;
                cfg.JoystickInvertX = InvertX;
                cfg.JoystickInvertY = InvertY;
            }

            // 保存完整配置到Flash
            ushort dz = (ushort)Math.Clamp(Deadzone, 0, 4095);
            bool result = await DeviceService.SetJoystickDeadzoneAsync(dz);
            if (result)
            {
                StatusMessage = $"配置已保存: 死区={dz}, 灵敏度={Sensitivity:F1}x, 反转X={InvertX}, 反转Y={InvertY}";
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

    partial void OnJoystickXChanged(double value)
    {
        OnPropertyChanged(nameof(DotX));
        OnPropertyChanged(nameof(DotLeft));
    }

    partial void OnJoystickYChanged(double value)
    {
        OnPropertyChanged(nameof(DotY));
        OnPropertyChanged(nameof(DotTop));
    }

    /// <summary>
    /// 实时应用死区（不写Flash）
    /// </summary>
    private async Task ApplyDeadzoneRealtimeAsync()
    {
        if (!DeviceService.IsConnected)
            return;

        // 捕获当前死区值，避免异步延迟后属性被修改
        ushort dz = (ushort)Math.Clamp(Deadzone, 0, 4095);

        try
        {
            bool result = await DeviceService.SetJoystickDeadzoneRealtimeAsync(dz);
            if (result)
            {
                StatusMessage = $"死区已实时应用: {dz}（未保存到Flash）";
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
    }

    /// <summary>
    /// 是否正在校准
    /// </summary>
    [ObservableProperty]
    private bool _isCalibrating;

    /// <summary>
    /// 触发摇杆中心点校准
    /// 校准期间用户应保持摇杆居中，固件采样50次后计算中心点
    /// </summary>
    [RelayCommand]
    private async Task CalibrateAsync()
    {
        if (!DeviceService.IsConnected || IsCalibrating)
            return;

        try
        {
            IsCalibrating = true;
            StatusMessage = "正在校准摇杆中心点，请保持摇杆居中... (约0.5秒)";

            // 发送校准命令，采样50次（约500ms）
            bool result = await DeviceService.CalibrateJoystickAsync(50);
            if (result)
            {
                StatusMessage = "✅ 摇杆中心点校准完成";
                // 校准后刷新一次摇杆状态
                await Task.Delay(100);
                var state = await DeviceService.GetJoystickStateAsync();
                if (state.HasValue)
                {
                    JoystickX = state.Value.X;
                    JoystickY = state.Value.Y;
                }
            }
            else
            {
                StatusMessage = "❌ 摇杆校准失败，请重试";
            }
        }
        catch
        {
            StatusMessage = "❌ 摇杆校准异常";
        }
        finally
        {
            IsCalibrating = false;
        }
    }

    /// <summary>
    /// 保存配置到Flash
    /// </summary>
    [RelayCommand]
    private async Task SaveConfigAsync()
    {
        if (!DeviceService.IsConnected || IsSaving)
            return;

        try
        {
            IsSaving = true;
            StatusMessage = "正在保存到Flash...";

            // 先实时应用当前值
            await DeviceService.SetJoystickDeadzoneRealtimeAsync((ushort)Deadzone);

            // 然后保存到Flash
            bool result = await DeviceService.SetJoystickDeadzoneAsync((ushort)Deadzone);
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
    /// 释放子类特有资源（重写基类虚方法）
    /// </summary>
    protected override void DisposeManagedResources()
    {
        _refreshTimer.Stop();
        _refreshTimer.Tick -= OnRefreshTimerTick;
    }
}
