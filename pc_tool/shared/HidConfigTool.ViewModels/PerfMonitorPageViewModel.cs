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
public partial class PerfMonitorPageViewModel : PageViewModelBase
{
    private readonly ITimerService _refreshTimer;
    private readonly IDialogService _dialogService;

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
    /// 选中的任务索引（用于设置阈值）
    /// </summary>
    [ObservableProperty]
    private int _selectedTaskIndex = -1;

    /// <summary>
    /// 阈值输入（微秒，0=禁用告警）
    /// </summary>
    [ObservableProperty]
    private int _thresholdInput = 1000;

    /// <summary>
    /// 阈值预设选项（微秒）
    /// </summary>
    public List<int> ThresholdPresets { get; } = new() { 500, 1000, 2000, 5000, 10000, 20000 };

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
        : base(deviceService)
    {
        _dialogService = dialogService;

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
    /// 设备已连接（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceConnectedAsync()
    {
        SystemStat = null;
        if (IsPerfMonitorEnabled)
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
        SystemStat = null;
        _refreshTimer.Stop();
        return Task.CompletedTask;
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
                await RefreshCoreAsync(isAutoRefresh: true);
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
            var stat = await DeviceService.GetPerfSystemStatAsync();
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
            bool success = await DeviceService.ResetPerfStatsAsync();
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
            bool success = await DeviceService.SetPerfMonitorEnabledAsync(targetEnabled);
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
    /// 设置选中任务的超时阈值
    /// </summary>
    [RelayCommand]
    private async Task SetThresholdAsync()
    {
        if (!IsDeviceConnected || SelectedTaskIndex < 0 || SelectedTaskIndex > 15)
        {
            _dialogService.ShowWarning("请先在任务列表中选择一个任务", "提示");
            return;
        }

        if (ThresholdInput < 0 || ThresholdInput > 25500)
        {
            _dialogService.ShowWarning("阈值必须在 0-25500 微秒之间（0=禁用告警）", "无效阈值");
            return;
        }

        try
        {
            bool success = await DeviceService.SetPerfThresholdAsync(SelectedTaskIndex, ThresholdInput);
            if (success)
            {
                if (ThresholdInput == 0)
                {
                    _dialogService.ShowInfo($"任务 {SelectedTaskIndex} 超时告警已禁用", "设置成功");
                }
                else
                {
                    _dialogService.ShowInfo($"任务 {SelectedTaskIndex} 超时阈值已设置为 {ThresholdInput}us", "设置成功");
                }
                // 刷新任务统计以显示新阈值
                if (SystemStat != null)
                {
                    await LoadTaskStatsAsync(SystemStat.TaskCount);
                }
            }
            else
            {
                _dialogService.ShowError("设置阈值失败，请检查设备连接", "设置失败");
            }
        }
        catch (Exception ex)
        {
            _dialogService.ShowError($"设置阈值异常: {ex.Message}", "错误");
        }
    }

    /// <summary>
    /// 禁用选中任务的超时告警
    /// </summary>
    [RelayCommand]
    private async Task DisableThresholdAsync()
    {
        if (!IsDeviceConnected || SelectedTaskIndex < 0 || SelectedTaskIndex > 15)
        {
            _dialogService.ShowWarning("请先在任务列表中选择一个任务", "提示");
            return;
        }

        try
        {
            bool success = await DeviceService.SetPerfThresholdAsync(SelectedTaskIndex, 0);
            if (success)
            {
                _dialogService.ShowInfo($"任务 {SelectedTaskIndex} 超时告警已禁用", "设置成功");
                // 刷新任务统计以显示新阈值
                if (SystemStat != null)
                {
                    await LoadTaskStatsAsync(SystemStat.TaskCount);
                }
            }
            else
            {
                _dialogService.ShowError("禁用告警失败，请检查设备连接", "设置失败");
            }
        }
        catch (Exception ex)
        {
            _dialogService.ShowError($"禁用告警异常: {ex.Message}", "错误");
        }
    }

    /// <summary>
    /// 加载所有任务统计
    /// </summary>
    private async Task LoadTaskStatsAsync(byte taskCount)
    {
        // 先收集所有数据，再批量更新，避免UI闪烁
        // 注意：任务索引可能不连续（如0-3和8-11），需遍历到最大索引而非taskCount
        var newStats = new List<PerfTaskStat>();
        for (byte i = 0; i < 16; i++)
        {
            var taskStat = await DeviceService.GetPerfTaskStatAsync(i);
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
    /// 释放子类特有资源（重写基类虚方法）
    /// </summary>
    protected override void DisposeManagedResources()
    {
        _refreshTimer.Stop();
        _refreshTimer.Tick -= OnRefreshTimerTick;
    }
}
