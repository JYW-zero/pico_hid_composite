using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 按键测试项
/// </summary>
public partial class KeyTestItem : ObservableObject
{
    /// <summary>
    /// 键索引（0-63）
    /// </summary>
    public int Index { get; set; }

    /// <summary>
    /// 是否按下
    /// </summary>
    [ObservableProperty]
    private bool _isPressed;
}

/// <summary>
/// 按键测试页面视图模型
/// </summary>
public partial class KeyTestPageViewModel : PageViewModelBase
{
    private readonly ITimerService _refreshTimer;

    /// <summary>
    /// 64个按键的状态
    /// </summary>
    public ObservableCollection<KeyTestItem> KeyItems { get; } = new();

    /// <summary>
    /// 当前按下的键数量
    /// </summary>
    [ObservableProperty]
    private int _pressedCount;

    /// <summary>
    /// 当前按下的键索引列表文本
    /// </summary>
    [ObservableProperty]
    private string _pressedKeysText = "无";

    /// <summary>
    /// 是否正在加载
    /// </summary>
    [ObservableProperty]
    private bool _isLoading;

    /// <summary>
    /// 是否自动刷新
    /// </summary>
    [ObservableProperty]
    private bool _autoRefresh = true;

    /// <summary>
    /// 构造函数
    /// </summary>
    public KeyTestPageViewModel(IDeviceService deviceService, ITimerService timerService)
        : base(deviceService)
    {
        _refreshTimer = timerService;

        // 初始化64个按键
        for (int i = 0; i < 64; i++)
        {
            KeyItems.Add(new KeyTestItem { Index = i, IsPressed = false });
        }

        _refreshTimer.Interval = TimeSpan.FromMilliseconds(100);  // 10Hz，足够按键测试，避免USB总线拥塞
        _refreshTimer.Tick += OnRefreshTimerTick;

        if (AutoRefresh && IsDeviceConnected)
        {
            _refreshTimer.Start();
        }
    }

    /// <summary>
    /// 设备已连接（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceConnectedAsync()
    {
        if (AutoRefresh)
        {
            _refreshTimer.Start();
        }
        return Task.CompletedTask;
    }

    /// <summary>
    /// 设备已断开（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceDisconnectedAsync()
    {
        _refreshTimer.Stop();
        // 清空按键状态
        foreach (var item in KeyItems)
        {
            item.IsPressed = false;
        }
        PressedCount = 0;
        PressedKeysText = "无";
        return Task.CompletedTask;
    }

    /// <summary>
    /// 自动刷新属性变化时启动/停止定时器
    /// </summary>
    partial void OnAutoRefreshChanged(bool value)
    {
        if (value && IsDeviceConnected)
        {
            _refreshTimer.Start();
        }
        else
        {
            _refreshTimer.Stop();
        }
    }

    /// <summary>
    /// 刷新定时器tick
    /// </summary>
    private async void OnRefreshTimerTick(object? sender, EventArgs e)
    {
        try
        {
            if (!IsLoading && IsDeviceConnected)
            {
                await RefreshCoreAsync();
            }
        }
        catch
        {
            // async void 定时器回调必须捕获异常，防止未观察异常导致应用崩溃
        }
    }

    /// <summary>
    /// 刷新命令
    /// </summary>
    [RelayCommand]
    private async Task RefreshAsync()
    {
        if (!IsDeviceConnected || IsLoading)
            return;

        await RefreshCoreAsync();
    }

    /// <summary>
    /// 核心刷新逻辑
    /// </summary>
    private async Task RefreshCoreAsync()
    {
        IsLoading = true;
        try
        {
            var keyState = await DeviceService.GetKeyStateAsync();
            if (keyState.HasValue)
            {
                ulong keys = keyState.Value;
                int count = 0;
                var pressedList = new List<int>();

                for (int i = 0; i < 64; i++)
                {
                    bool pressed = (keys & (1UL << i)) != 0;
                    KeyItems[i].IsPressed = pressed;
                    if (pressed)
                    {
                        count++;
                        pressedList.Add(i);
                    }
                }

                PressedCount = count;
                PressedKeysText = pressedList.Count > 0
                    ? string.Join(", ", pressedList.Select(x => $"键{x}"))
                    : "无";
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"读取按键状态失败: {ex.Message}");
        }
        finally
        {
            IsLoading = false;
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
