using System.Collections.ObjectModel;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 错误日志页面视图模型
/// </summary>
public partial class ErrorLogPageViewModel : ObservableObject, IDisposable
{
    private readonly IDeviceService _deviceService;
    private readonly IDialogService _dialogService;
    private bool _disposed;

    // 所有日志的完整列表（用于筛选）
    private List<ErrorLogEntry> _allLogs = new();

    /// <summary>
    /// 日志列表
    /// </summary>
    [ObservableProperty]
    private ObservableCollection<ErrorLogEntry> _logs = new();

    /// <summary>
    /// 总日志数
    /// </summary>
    [ObservableProperty]
    private uint _totalLogCount;

    /// <summary>
    /// 总故障数
    /// </summary>
    [ObservableProperty]
    private uint _totalFaultCount;

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
    /// 选中的日志
    /// </summary>
    [ObservableProperty]
    private ErrorLogEntry? _selectedLog;

    /// <summary>
    /// 当前筛选级别（0=全部，1=INFO，2=WARN，3=ERROR，4=FATAL）
    /// </summary>
    [ObservableProperty]
    private int _currentFilter = 0;

    /// <summary>
    /// 构造函数
    /// </summary>
    public ErrorLogPageViewModel(IDeviceService deviceService, IDialogService dialogService)
    {
        _deviceService = deviceService;
        _dialogService = dialogService;
        _deviceService.DeviceConnectionChanged += OnDeviceConnectionChanged;

        IsDeviceConnected = _deviceService.IsConnected;
    }

    /// <summary>
    /// 设备连接状态变化
    /// </summary>
    private void OnDeviceConnectionChanged(object? sender, bool isConnected)
    {
        IsDeviceConnected = isConnected;
        if (!isConnected)
        {
            Logs.Clear();
            TotalLogCount = 0;
            TotalFaultCount = 0;
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
            // 获取所有日志
            var logs = await _deviceService.GetAllErrorLogsAsync();
            if (logs != null)
            {
                _allLogs = logs.ToList();
                ApplyFilter();
            }

            // 获取日志信息
            var info = await _deviceService.GetErrorLogInfoAsync();
            if (info != null)
            {
                TotalLogCount = info.LogCount;
                TotalFaultCount = info.TotalFaultCount;
            }
        }
        catch (Exception ex)
        {
            // 错误处理
            System.Diagnostics.Debug.WriteLine($"刷新错误日志失败: {ex.Message}");
        }
        finally
        {
            IsLoading = false;
        }
    }

    /// <summary>
    /// 应用筛选
    /// </summary>
    private void ApplyFilter()
    {
        Logs.Clear();

        var filtered = _allLogs.AsEnumerable();

        // 按级别筛选
        if (CurrentFilter > 0)
        {
            byte level = (byte)(CurrentFilter - 1); // 0=INFO, 1=WARN, 2=ERROR, 3=FATAL
            filtered = filtered.Where(log => log.Level == level);
        }

        // 倒序显示（最新的在上面）
        var reversed = filtered.Reverse().ToList();
        foreach (var log in reversed)
        {
            Logs.Add(log);
        }
    }

    /// <summary>
    /// 筛选级别变化时重新过滤
    /// </summary>
    partial void OnCurrentFilterChanged(int value)
    {
        ApplyFilter();
    }

    /// <summary>
    /// 清除日志命令
    /// </summary>
    [RelayCommand]
    private async Task ClearAsync()
    {
        if (!IsDeviceConnected || IsLoading)
            return;

        // 确认对话框
        if (!_dialogService.ShowConfirm("确定要清除所有错误日志吗？此操作不可撤销。", "确认清除"))
            return;

        IsLoading = true;
        try
        {
            bool success = await _deviceService.ClearErrorLogsAsync();
            if (success)
            {
                _allLogs.Clear();
                Logs.Clear();
                TotalLogCount = 0;
                TotalFaultCount = 0;
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"清除错误日志失败: {ex.Message}");
        }
        finally
        {
            IsLoading = false;
        }
    }

    /// <summary>
    /// 页面加载时调用
    /// </summary>
    public void OnLoaded()
    {
        if (IsDeviceConnected && Logs.Count == 0)
        {
            _ = RefreshAsync();
        }
    }

    /// <summary>
    /// 释放资源，取消事件订阅，防止内存泄漏
    /// </summary>
    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _deviceService.DeviceConnectionChanged -= OnDeviceConnectionChanged;
    }
}
