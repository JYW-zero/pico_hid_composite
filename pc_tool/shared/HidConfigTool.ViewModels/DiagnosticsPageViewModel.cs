using System.Collections.ObjectModel;
using System.IO;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;
using HidConfigTool.Core.Services;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 设备诊断页面视图模型
/// </summary>
public partial class DiagnosticsPageViewModel : PageViewModelBase
{
    private readonly DeviceDiagnosticsService _diagnosticsService;
    private readonly IDialogService _dialogService;
    private readonly IFileDialogService _fileDialogService;

    /// <summary>
    /// 是否正在读取诊断数据
    /// </summary>
    [ObservableProperty]
    private bool _isReading;

    /// <summary>
    /// 读取进度（0-100）
    /// </summary>
    [ObservableProperty]
    private int _readProgress;

    /// <summary>
    /// 设备健康状态
    /// </summary>
    [ObservableProperty]
    private DeviceHealthStatus _healthStatus = DeviceHealthStatus.Unknown;

    /// <summary>
    /// 诊断数据
    /// </summary>
    [ObservableProperty]
    private DeviceDiagnostics? _diagnostics;

    /// <summary>
    /// 错误日志列表
    /// </summary>
    public ObservableCollection<ErrorLogEntry> ErrorLogs { get; } = new();

    /// <summary>
    /// 任务性能统计列表
    /// </summary>
    public ObservableCollection<PerfTaskStat> TaskStats { get; } = new();

    /// <summary>
    /// 健康状态文本
    /// </summary>
    public string HealthStatusText => HealthStatus switch
    {
        DeviceHealthStatus.Healthy => "健康",
        DeviceHealthStatus.Warning => "警告",
        DeviceHealthStatus.Error => "错误",
        _ => "未知"
    };

    /// <summary>
    /// 健康状态颜色
    /// </summary>
    public string HealthStatusColor => HealthStatus switch
    {
        DeviceHealthStatus.Healthy => "#FF9ECE6A",
        DeviceHealthStatus.Warning => "#FFE0AF68",
        DeviceHealthStatus.Error => "#FFF7768E",
        _ => "#FF565F89"
    };

    public DiagnosticsPageViewModel(
        IDeviceService deviceService,
        DeviceDiagnosticsService diagnosticsService,
        IDialogService dialogService,
        IFileDialogService fileDialogService)
        : base(deviceService)
    {
        _diagnosticsService = diagnosticsService;
        _dialogService = dialogService;
        _fileDialogService = fileDialogService;
        StatusMessage = "就绪";
    }

    /// <summary>
    /// 设备已断开（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceDisconnectedAsync()
    {
        Diagnostics = null;
        ErrorLogs.Clear();
        TaskStats.Clear();
        HealthStatus = DeviceHealthStatus.Unknown;
        StatusMessage = "设备已断开";
        return Task.CompletedTask;
    }

    /// <summary>
    /// 读取诊断数据命令
    /// </summary>
    [RelayCommand(CanExecute = nameof(CanReadDiagnostics))]
    private async Task ReadDiagnosticsAsync()
    {
        if (!IsDeviceConnected)
        {
            _dialogService.ShowWarning("请先连接设备", "未连接设备");
            return;
        }

        try
        {
            IsReading = true;
            ReadProgress = 0;
            StatusMessage = "正在读取诊断数据...";
            ErrorLogs.Clear();
            TaskStats.Clear();

            var progress = new Progress<int>(p => ReadProgress = p);
            var diagnostics = await _diagnosticsService.ReadDiagnosticsAsync(progress);

            if (diagnostics != null)
            {
                Diagnostics = diagnostics;
                HealthStatus = diagnostics.HealthStatus;
                OnPropertyChanged(nameof(HealthStatusText));
                OnPropertyChanged(nameof(HealthStatusColor));

                // 填充错误日志
                if (diagnostics.ErrorLogs != null)
                {
                    foreach (var log in diagnostics.ErrorLogs)
                    {
                        ErrorLogs.Add(log);
                    }
                }

                // 填充任务性能统计
                if (diagnostics.PerfTaskStats != null)
                {
                    foreach (var task in diagnostics.PerfTaskStats)
                    {
                        TaskStats.Add(task);
                    }
                }

                StatusMessage = $"诊断完成 - 健康状态: {HealthStatusText}";
            }
            else
            {
                StatusMessage = "诊断数据读取失败";
                _dialogService.ShowError("诊断数据读取失败，请检查设备连接", "错误");
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"诊断失败: {ex.Message}";
            _dialogService.ShowError($"诊断失败: {ex.Message}", "错误");
        }
        finally
        {
            IsReading = false;
        }
    }

    /// <summary>
    /// 是否可以读取诊断数据
    /// </summary>
    private bool CanReadDiagnostics() => IsDeviceConnected && !IsReading;

    /// <summary>
    /// 保存诊断数据命令
    /// </summary>
    [RelayCommand(CanExecute = nameof(CanSaveDiagnostics))]
    private async Task SaveDiagnosticsAsync()
    {
        if (Diagnostics == null)
        {
            _dialogService.ShowWarning("请先读取诊断数据", "无数据");
            return;
        }

        try
        {
            string? filePath = _fileDialogService.SaveFile(
                "保存诊断报告",
                "文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*",
                "txt");

            if (string.IsNullOrEmpty(filePath))
                return;

            await _diagnosticsService.SaveDiagnosticsToFileAsync(Diagnostics, filePath);
            StatusMessage = $"诊断报告已保存到: {Path.GetFileName(filePath)}";
            _dialogService.ShowInfo($"诊断报告已保存到:\n{filePath}", "保存成功");
        }
        catch (Exception ex)
        {
            StatusMessage = $"保存失败: {ex.Message}";
            _dialogService.ShowError($"保存失败: {ex.Message}", "错误");
        }
    }

    /// <summary>
    /// 是否可以保存诊断数据
    /// </summary>
    private bool CanSaveDiagnostics() => Diagnostics != null;

}
