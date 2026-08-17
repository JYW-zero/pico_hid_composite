using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 性能监控页面视图模型
/// </summary>
public partial class PerfMonitorPageViewModel : ObservableObject, IDisposable
{
    private readonly IDeviceService _deviceService;
    private readonly ITimerService _refreshTimer;
    private readonly IDialogService _dialogService;
    private bool _disposed;

    /// <summary>
    /// 系统性能统计
    /// </summary>
    [ObservableProperty]
    private PerfSystemStat? _systemStat;

    /// <summary>
    /// 任务性能统计列表
    /// </summary>
    public ObservableCollection<PerfTaskStat> TaskStats { get; } = new();

    /// <summary>
    /// CPU使用率历史数据（最多60个点）
    /// </summary>
    public Queue<double> CpuHistory { get; } = new();

    /// <summary>
    /// CPU折线图的点集合
    /// </summary>
    [ObservableProperty]
    private List<UiPoint> _cpuChartPoints = new();

    /// <summary>
    /// 是否正在加载
    /// </summary>
    [ObservableProperty]
    private bool _isLoading;

    /// <summary>
    /// 是否有设备连接
    /// </summary>
    [ObservableProperty]
    private bool _isDeviceConnected;

    /// <summary>
    /// 刷新间隔（秒）
    /// </summary>
    [ObservableProperty]
    private int _refreshInterval = 5;

    /// <summary>
    /// 刷新间隔选项
    /// </summary>
    public List<int> RefreshIntervalOptions { get; } = new() { 1, 2, 3, 5, 10, 30, 60 };

    /// <summary>
    /// 设备端性能监控是否开启
    /// </summary>
    [ObservableProperty]
    private bool _isPerfMonitorEnabled;

    /// <summary>
    /// CPU使用率文本
    /// </summary>
    public string CpuUsageText => SystemStat?.CpuUsage.ToString() ?? "--";

    /// <summary>
    /// 循环频率文本
    /// </summary>
    public string LoopFreqText => SystemStat?.LoopFreqHz.ToString() ?? "--";

    /// <summary>
    /// 运行时间文本
    /// </summary>
    public string UptimeText => SystemStat?.UptimeFormatted ?? "--";

    /// <summary>
    /// 任务数量文本
    /// </summary>
    public string TaskCountText => SystemStat?.TaskCount.ToString() ?? "--";

    /// <summary>
    /// 10秒平均CPU使用率文本
    /// </summary>
    public string CpuAvg10sText => SystemStat?.CpuUsageAvg10s.ToString() ?? "--";

    /// <summary>
    /// 30秒平均CPU使用率文本
    /// </summary>
    public string CpuAvg30sText => SystemStat?.CpuUsageAvg30s.ToString() ?? "--";

    /// <summary>
    /// 10秒平均频率文本
    /// </summary>
    public string LoopFreqAvg10sText => SystemStat?.LoopFreqAvg10s.ToString() ?? "--";

    /// <summary>
    /// 构造函数
    /// </summary>
    public PerfMonitorPageViewModel(IDeviceService deviceService, ITimerService timerService, IDialogService dialogService)
    {
        _deviceService = deviceService;
        _dialogService = dialogService;
        _deviceService.DeviceConnectionChanged += OnDeviceConnectionChanged;

        IsDeviceConnected = _deviceService.IsConnected;

        _refreshTimer = timerService;
        _refreshTimer.Interval = TimeSpan.FromSeconds(5);  // 5秒刷新，减少USB总线占用
        _refreshTimer.Tick += OnRefreshTimerTick;

        // 设备已连接且监控已开启时启动自动刷新
        if (IsDeviceConnected && IsPerfMonitorEnabled)
        {
            _refreshTimer.Start();
        }
    }

    /// <summary>
    /// 设备连接状态变化
    /// </summary>
    private void OnDeviceConnectionChanged(object? sender, bool isConnected)
    {
        IsDeviceConnected = isConnected;
        SystemStat = null;

        if (isConnected && IsPerfMonitorEnabled)
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
        if (!IsLoading && IsDeviceConnected)
        {
            await RefreshCoreAsync(isAutoRefresh: true);
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

        await RefreshCoreAsync(isAutoRefresh: false);
    }

    /// <summary>
    /// 核心刷新逻辑
    /// </summary>
    private async Task RefreshCoreAsync(bool isAutoRefresh)
    {
        if (!isAutoRefresh)
        {
            IsLoading = true;
        }
        try
        {
            // 加载系统状态
            var stat = await _deviceService.GetPerfSystemStatAsync();
            if (stat != null)
            {
                SystemStat = stat;
                OnPropertyChanged(nameof(CpuUsageText));
                OnPropertyChanged(nameof(LoopFreqText));
                OnPropertyChanged(nameof(UptimeText));
                OnPropertyChanged(nameof(TaskCountText));
                OnPropertyChanged(nameof(CpuAvg10sText));
                OnPropertyChanged(nameof(CpuAvg30sText));
                OnPropertyChanged(nameof(LoopFreqAvg10sText));

                // 记录CPU历史数据
                CpuHistory.Enqueue(stat.CpuUsage);
                if (CpuHistory.Count > 60)
                {
                    CpuHistory.Dequeue();
                }
                UpdateCpuChart();

                // 加载任务统计
                await LoadTaskStatsAsync(stat.TaskCount);
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"刷新性能统计失败: {ex.Message}");
        }
        finally
        {
            if (!isAutoRefresh)
            {
                IsLoading = false;
            }
        }
    }

    /// <summary>
    /// 重置命令
    /// </summary>
    [RelayCommand]
    private async Task ResetAsync()
    {
        if (!IsDeviceConnected || IsLoading)
            return;

        // 确认对话框
        if (!_dialogService.ShowConfirm("确定要重置所有性能统计吗？此操作不可撤销。", "确认重置"))
            return;

        IsLoading = true;
        try
        {
            bool success = await _deviceService.ResetPerfStatsAsync();
            if (success)
            {
                // 清空历史数据
                CpuHistory.Clear();
                UpdateCpuChart();
                TaskStats.Clear();
                SystemStat = null;
                OnPropertyChanged(nameof(CpuUsageText));
                OnPropertyChanged(nameof(LoopFreqText));
                OnPropertyChanged(nameof(UptimeText));
                OnPropertyChanged(nameof(TaskCountText));
                OnPropertyChanged(nameof(CpuAvg10sText));
                OnPropertyChanged(nameof(CpuAvg30sText));
                OnPropertyChanged(nameof(LoopFreqAvg10sText));
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"重置性能统计失败: {ex.Message}");
        }
        finally
        {
            IsLoading = false;
        }
    }

    /// <summary>
    /// 切换性能监控开关
    /// </summary>
    [RelayCommand]
    private async Task TogglePerfMonitorAsync()
    {
        if (!IsDeviceConnected)
            return;

        bool targetEnabled = !IsPerfMonitorEnabled;
        try
        {
            bool success = await _deviceService.SetPerfMonitorEnabledAsync(targetEnabled);
            if (success)
            {
                IsPerfMonitorEnabled = targetEnabled;
                if (targetEnabled)
                {
                    // 开启后启动自动刷新并立即刷新一次
                    _refreshTimer.Start();
                    await RefreshCoreAsync(isAutoRefresh: false);
                }
                else
                {
                    // 关闭时停止刷新并清空显示
                    _refreshTimer.Stop();
                    SystemStat = null;
                    TaskStats.Clear();
                    CpuHistory.Clear();
                    UpdateCpuChart();
                    OnPropertyChanged(nameof(CpuUsageText));
                    OnPropertyChanged(nameof(LoopFreqText));
                    OnPropertyChanged(nameof(UptimeText));
                    OnPropertyChanged(nameof(TaskCountText));
                    OnPropertyChanged(nameof(CpuAvg10sText));
                    OnPropertyChanged(nameof(CpuAvg30sText));
                    OnPropertyChanged(nameof(LoopFreqAvg10sText));
                }
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"切换性能监控失败: {ex.Message}");
        }
    }

    /// <summary>
    /// 加载所有任务统计
    /// </summary>
    private async Task LoadTaskStatsAsync(byte taskCount)
    {
        // 先收集所有数据，再批量更新，避免UI闪烁
        var newStats = new List<PerfTaskStat>();
        for (byte i = 0; i < taskCount && i < 16; i++)
        {
            var taskStat = await _deviceService.GetPerfTaskStatAsync(i);
            if (taskStat != null)
            {
                newStats.Add(taskStat);
            }
        }

        // 数量相同时更新现有项（不触发集合变更，减少闪烁）
        // 数量不同时重建（仅在任务注册数变化时发生）
        if (TaskStats.Count == newStats.Count)
        {
            for (int i = 0; i < newStats.Count; i++)
            {
                TaskStats[i] = newStats[i];
            }
        }
        else
        {
            TaskStats.Clear();
            foreach (var stat in newStats)
            {
                TaskStats.Add(stat);
            }
        }
    }

    /// <summary>
    /// 更新CPU折线图
    /// </summary>
    private void UpdateCpuChart()
    {
        var points = new List<UiPoint>();
        double chartWidth = 400;
        double chartHeight = 120;
        int maxPoints = 60;

        if (CpuHistory.Count == 0)
        {
            CpuChartPoints = points;
            return;
        }

        double stepX = chartWidth / (maxPoints - 1);
        int startIndex = 0;

        for (int i = 0; i < CpuHistory.Count; i++)
        {
            double cpu = CpuHistory.ElementAt(i);
            double x = startIndex * stepX + i * stepX;
            double y = chartHeight - (cpu / 100.0) * chartHeight;
            points.Add(new UiPoint(x, y));
        }

        CpuChartPoints = points;
    }

    /// <summary>
    /// 刷新间隔变化时立即更新定时器
    /// </summary>
    partial void OnRefreshIntervalChanged(int value)
    {
        if (value < 1) value = 1;
        if (value > 60) value = 60;
        _refreshTimer.Interval = TimeSpan.FromSeconds(value);
    }

    /// <summary>
    /// 页面加载时调用
    /// </summary>
    public void OnLoaded()
    {
        if (IsDeviceConnected && SystemStat == null)
        {
            _ = RefreshAsync();
        }
    }

    /// <summary>
    /// 页面卸载时调用
    /// </summary>
    public void OnUnloaded()
    {
        _refreshTimer.Stop();
    }

    /// <summary>
    /// 释放资源，取消事件订阅，防止内存泄漏
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
