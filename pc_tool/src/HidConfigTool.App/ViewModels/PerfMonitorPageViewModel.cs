using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Media;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 性能监控页面视图模型
/// </summary>
public partial class PerfMonitorPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private readonly DispatcherTimer _refreshTimer;

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
    private PointCollection _cpuChartPoints = new();

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
    /// 是否自动刷新
    /// </summary>
    [ObservableProperty]
    private bool _autoRefresh = true;

    /// <summary>
    /// 刷新间隔（秒）
    /// </summary>
    [ObservableProperty]
    private int _refreshInterval = 1;

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
    public PerfMonitorPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        _deviceService.DeviceConnectionChanged += OnDeviceConnectionChanged;

        IsDeviceConnected = _deviceService.IsConnected;

        _refreshTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromSeconds(1)
        };
        _refreshTimer.Tick += OnRefreshTimerTick;

        if (AutoRefresh && IsDeviceConnected)
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

        if (isConnected && AutoRefresh)
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
            await RefreshAsync();
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

        IsLoading = true;
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
            IsLoading = false;
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
        var result = MessageBox.Show("确定要重置所有性能统计吗？此操作不可撤销。", "确认重置", MessageBoxButton.YesNo, MessageBoxImage.Warning);
        if (result != MessageBoxResult.Yes)
            return;

        IsLoading = true;
        try
        {
            bool success = await _deviceService.ResetPerfStatsAsync();
            if (success)
            {
                // 重置后刷新一下
                await RefreshAsync();
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
    /// 加载所有任务统计
    /// </summary>
    private async Task LoadTaskStatsAsync(byte taskCount)
    {
        TaskStats.Clear();

        for (byte i = 0; i < taskCount && i < 16; i++)
        {
            var taskStat = await _deviceService.GetPerfTaskStatAsync(i);
            if (taskStat != null)
            {
                TaskStats.Add(taskStat);
            }
        }
    }

    /// <summary>
    /// 更新CPU折线图
    /// </summary>
    private void UpdateCpuChart()
    {
        var points = new PointCollection();
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
            points.Add(new Point(x, y));
        }

        CpuChartPoints = points;
    }

    /// <summary>
    /// 切换自动刷新
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
    /// 刷新间隔变化
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
}
