using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 按键统计项
/// </summary>
public partial class KeyStatItemViewModel : ObservableObject
{
    [ObservableProperty]
    private int _index;

    [ObservableProperty]
    private string _keyName = string.Empty;

    [ObservableProperty]
    private int _count;

    [ObservableProperty]
    private double _percentage;
}

/// <summary>
/// 热力图按键项
/// </summary>
public partial class HeatmapKeyViewModel : ObservableObject
{
    [ObservableProperty]
    private int _index;

    [ObservableProperty]
    private int _row;

    [ObservableProperty]
    private int _column;

    [ObservableProperty]
    private string _keyName = string.Empty;

    [ObservableProperty]
    private int _count;

    [ObservableProperty]
    private UiColor _backgroundBrush = UiColor.Transparent;
}

/// <summary>
/// 统计页面视图模型
/// </summary>
public partial class StatsPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private readonly ITimerService _refreshTimer;
    private readonly IDialogService _dialogService;

    /// <summary>
    /// 总按键次数
    /// </summary>
    [ObservableProperty]
    private long _totalKeystrokes;

    /// <summary>
    /// 今日按键次数（暂用总次数代替，后续实现本地历史统计）
    /// </summary>
    [ObservableProperty]
    private int _todayKeystrokes;

    /// <summary>
    /// 平均每分钟按键数（暂不计算）
    /// </summary>
    [ObservableProperty]
    private double _averageKpm;

    /// <summary>
    /// 当前时间范围：0=今日，1=本周，2=本月，3=全部
    /// 注意：当前固件只支持总统计，时间范围功能待实现
    /// </summary>
    [ObservableProperty]
    private int _timeRange = 3; // 默认全部

    /// <summary>
    /// 是否正在加载
    /// </summary>
    [ObservableProperty]
    private bool _isLoading;

    /// <summary>
    /// 最常用按键 Top 10
    /// </summary>
    public ObservableCollection<KeyStatItemViewModel> TopKeys { get; } = new();

    /// <summary>
    /// 热力图按键
    /// </summary>
    public ObservableCollection<HeatmapKeyViewModel> HeatmapKeys { get; } = new();

    // 键位名称（8x8 布局）
    private static readonly string[] KeyNames = {
        "Esc", "1", "2", "3", "4", "5", "6", "7",
        "Tab", "Q", "W", "E", "R", "T", "Y", "U",
        "Caps", "A", "S", "D", "F", "G", "H", "J",
        "Shift", "Z", "X", "C", "V", "B", "N", "M",
        "Ctrl", "Win", "Alt", "Space", "Alt", "Fn", "Menu", "Ctrl",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
        "F9", "F10", "F11", "F12", "PrtSc", "ScrLk", "Pause", "Ins",
        "Home", "PgUp", "Del", "End", "PgDn", "↑", "↓", "←"
    };

    public StatsPageViewModel(IDeviceService deviceService, ITimerService timerService, IDialogService dialogService)
    {
        _deviceService = deviceService;
        _dialogService = dialogService;

        // 初始化热力图（先创建空的）
        InitHeatmap();

        // 定时刷新计时器（每5秒刷新一次）
        _refreshTimer = timerService;
        _refreshTimer.Interval = TimeSpan.FromSeconds(5);
        _refreshTimer.Tick += async (s, e) => await RefreshStatsAsync();

        // 页面加载时开始刷新
        _ = RefreshStatsAsync();
        _refreshTimer.Start();
    }

    /// <summary>
    /// 初始化热力图（创建空的按键项）
    /// </summary>
    private void InitHeatmap()
    {
        HeatmapKeys.Clear();
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                int index = row * 8 + col;
                string name = index < KeyNames.Length ? KeyNames[index] : $"K{index}";

                HeatmapKeys.Add(new HeatmapKeyViewModel
                {
                    Index = index,
                    Row = row,
                    Column = col,
                    KeyName = name,
                    Count = 0,
                    BackgroundBrush = new UiColor(26, 40, 59) // 深色背景
                });
            }
        }
    }

    /// <summary>
    /// 从设备刷新统计数据
    /// </summary>
    private async Task RefreshStatsAsync()
    {
        if (!_deviceService.IsConnected || IsLoading)
            return;

        try
        {
            IsLoading = true;

            uint[]? stats = await _deviceService.GetKeyStatsAsync();
            if (stats == null || stats.Length < 64)
                return;

            // 计算总按键数
            long total = 0;
            var allCounts = new List<(int Index, uint Count, string Name)>();
            for (int i = 0; i < 64; i++)
            {
                total += stats[i];
                string name = i < KeyNames.Length ? KeyNames[i] : $"K{i}";
                allCounts.Add((i, stats[i], name));
            }

            TotalKeystrokes = total;
            TodayKeystrokes = (int)Math.Min(total, int.MaxValue); // 暂用总次数代替

            // 更新热力图
            uint maxCount = stats.Max();
            if (maxCount == 0) maxCount = 1;

            for (int i = 0; i < 64 && i < HeatmapKeys.Count; i++)
            {
                var key = HeatmapKeys[i];
                key.Count = (int)stats[i];

                // 计算颜色：从深到浅的蓝色
                double ratio = (double)stats[i] / maxCount;
                byte r = (byte)(26 + ratio * 60);
                byte g = (byte)(40 + ratio * 80);
                byte b = (byte)(59 + ratio * 160);
                key.BackgroundBrush = new UiColor(r, g, b);
            }

            // 更新 Top 10
            var top10 = allCounts.OrderByDescending(x => x.Count).Take(10).ToList();
            TopKeys.Clear();
            for (int i = 0; i < top10.Count; i++)
            {
                TopKeys.Add(new KeyStatItemViewModel
                {
                    Index = i + 1,
                    KeyName = top10[i].Name,
                    Count = (int)top10[i].Count,
                    Percentage = total > 0 ? (double)top10[i].Count / total * 100 : 0
                });
            }
        }
        catch
        {
            // 忽略错误
        }
        finally
        {
            IsLoading = false;
        }
    }

    partial void OnTimeRangeChanged(int value)
    {
        // 时间范围功能待实现，暂时刷新一下
        _ = RefreshStatsAsync();
    }

    [RelayCommand]
    private void SwitchTimeRange(string range)
    {
        if (int.TryParse(range, out int index))
        {
            TimeRange = index;
        }
    }

    [RelayCommand]
    private async Task ResetStatsAsync()
    {
        if (_dialogService.ShowConfirm("确定要重置所有统计数据吗？", "确认重置"))
        {
            if (_deviceService.IsConnected)
            {
                await _deviceService.ResetKeyStatsAsync();
                await RefreshStatsAsync();
            }
        }
    }
}
