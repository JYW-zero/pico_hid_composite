using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 鼠标设置页面视图模型
/// </summary>
public partial class MousePageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private bool _isInitialized;
    private bool _isSaving;
    private CancellationTokenSource? _dpiDebounceCts;

    [ObservableProperty]
    private int _currentDpiIndex = 1;

    [ObservableProperty]
    private double _currentDpi = 800;

    [ObservableProperty]
    private bool _accelerationEnabled;

    [ObservableProperty]
    private double _accelerationThreshold = 10;

    [ObservableProperty]
    private double _accelerationRatio = 1.5;

    [ObservableProperty]
    private bool _isApplying;

    /// <summary>
    /// 4档 DPI 值（可编辑）
    /// </summary>
    [ObservableProperty]
    private int _dpiSlot0 = 400;

    [ObservableProperty]
    private int _dpiSlot1 = 800;

    [ObservableProperty]
    private int _dpiSlot2 = 1600;

    [ObservableProperty]
    private int _dpiSlot3 = 3200;

    /// <summary>
    /// 当前 DPI 档位（0-3）
    /// </summary>
    [ObservableProperty]
    private int _currentDpiSlot = 2;

    /// <summary>
    /// LOD 静默高度等级（0=低,1=中,2=高）
    /// </summary>
    [ObservableProperty]
    private int _lodLevel = 1;

    /// <summary>
    /// 角度捕捉开关
    /// </summary>
    [ObservableProperty]
    private bool _angleSnapping;

    /// <summary>
    /// 当前 DPI 文本
    /// </summary>
    public string CurrentDpiText => $"{(int)CurrentDpi} DPI";

    /// <summary>
    /// LOD 选项
    /// </summary>
    public string[] LodOptions { get; } = { "低", "中", "高" };

    public MousePageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 从当前配置加载
        if (_deviceService.IsConnected && _deviceService.CurrentConfig != null)
        {
            var cfg = _deviceService.CurrentConfig;
            CurrentDpi = cfg.Dpi;
            CurrentDpiIndex = cfg.DpiIndex;
            AccelerationEnabled = cfg.AccelerationEnabled;
            AccelerationThreshold = cfg.AccelerationThreshold;
            AccelerationRatio = cfg.AccelerationRatio;

            // v6 新增字段
            if (cfg.DpiSlots != null && cfg.DpiSlots.Length >= 4)
            {
                DpiSlot0 = cfg.DpiSlots[0];
                DpiSlot1 = cfg.DpiSlots[1];
                DpiSlot2 = cfg.DpiSlots[2];
                DpiSlot3 = cfg.DpiSlots[3];
            }
            CurrentDpiSlot = cfg.CurrentDpiSlot;
            LodLevel = cfg.LodLevel;
            AngleSnapping = cfg.AngleSnapping;
        }

        _isInitialized = true;
    }

    [RelayCommand]
    private async Task SetDpiAsync(object? parameter)
    {
        if (parameter is not string indexStr || !int.TryParse(indexStr, out int index))
            return;

        if (index < 0 || index > 3)
            return;

        int[] dpiValues = { 400, 800, 1600, 3200 };
        double dpi = dpiValues[index];

        // 先更新本地显示
        CurrentDpiIndex = index;
        CurrentDpi = dpi;

        // 取消待处理的 debounce
        _dpiDebounceCts?.Cancel();

        if (_deviceService.IsConnected)
        {
            IsApplying = true;
            try
            {
                await _deviceService.SetDpiAsync(index);
            }
            catch
            {
                // 忽略写入错误
            }
            finally
            {
                IsApplying = false;
            }
        }
    }

    partial void OnCurrentDpiChanged(double value)
    {
        if (!_isInitialized || _isSaving) return;

        // 检查是否匹配预设档位
        int[] dpiValues = { 400, 800, 1600, 3200 };
        int idx = Array.IndexOf(dpiValues, (int)value);
        if (idx >= 0) CurrentDpiIndex = idx;

        if (_deviceService.IsConnected)
        {
            // debounce: 延迟300ms写入，避免拖动滑块时频繁写Flash
            _dpiDebounceCts?.Cancel();
            _dpiDebounceCts = new CancellationTokenSource();
            var token = _dpiDebounceCts.Token;
            _ = Task.Run(async () =>
            {
                try
                {
                    await Task.Delay(300, token);
                    if (!token.IsCancellationRequested)
                    {
                        await SaveCustomDpiAsync((ushort)value);
                    }
                }
                catch (TaskCanceledException) { }
            }, token);
        }
    }

    private async Task SaveCustomDpiAsync(ushort dpi)
    {
        if (!_deviceService.IsConnected || _isSaving) return;

        try
        {
            _isSaving = true;
            await _deviceService.SetDpiValueAsync(dpi);
        }
        catch
        {
            // 忽略保存错误
        }
        finally
        {
            _isSaving = false;
        }
    }

    partial void OnAccelerationEnabledChanged(bool value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveAccelerationAsync();
    }

    partial void OnAccelerationThresholdChanged(double value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveAccelerationAsync();
    }

    partial void OnAccelerationRatioChanged(double value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveAccelerationAsync();
    }

    private async Task SaveAccelerationAsync()
    {
        if (!_deviceService.IsConnected || _isSaving)
            return;

        try
        {
            _isSaving = true;
            await _deviceService.SetAccelerationAsync(
                AccelerationEnabled,
                AccelerationThreshold,
                AccelerationRatio);
        }
        catch
        {
            // 忽略保存错误
        }
        finally
        {
            _isSaving = false;
        }
    }

    /// <summary>
    /// 循环切换 DPI 档位
    /// </summary>
    [RelayCommand]
    private async Task SwitchDpiSlotAsync()
    {
        if (!_deviceService.IsConnected) return;

        try
        {
            IsApplying = true;
            await _deviceService.SwitchDpiSlotAsync(0xFF);

            // 刷新本地显示
            if (_deviceService.CurrentConfig != null)
            {
                var cfg = _deviceService.CurrentConfig;
                CurrentDpi = cfg.Dpi;
                CurrentDpiSlot = cfg.CurrentDpiSlot;
                if (cfg.DpiSlots != null && cfg.DpiSlots.Length >= 4)
                {
                    DpiSlot0 = cfg.DpiSlots[0];
                    DpiSlot1 = cfg.DpiSlots[1];
                    DpiSlot2 = cfg.DpiSlots[2];
                    DpiSlot3 = cfg.DpiSlots[3];
                }
            }
        }
        catch
        {
            // 忽略错误
        }
        finally
        {
            IsApplying = false;
        }
    }

    /// <summary>
    /// 保存高级配置（多档DPI/LOD/角度捕捉）
    /// </summary>
    [RelayCommand]
    private async Task SaveAdvancedConfigAsync()
    {
        if (!_deviceService.IsConnected || _deviceService.CurrentConfig == null || _isSaving)
            return;

        try
        {
            _isSaving = true;
            IsApplying = true;

            var cfg = _deviceService.CurrentConfig;

            // 更新 4 档 DPI 值（限制范围并对齐到25的倍数）
            int[] slots = { DpiSlot0, DpiSlot1, DpiSlot2, DpiSlot3 };
            for (int i = 0; i < 4; i++)
            {
                int val = slots[i];
                if (val < 100) val = 100;
                if (val > 6400) val = 6400;
                val = (val / 25) * 25;
                cfg.DpiSlots[i] = (ushort)val;
            }

            // 更新当前档位
            cfg.CurrentDpiSlot = (byte)Math.Clamp(CurrentDpiSlot, 0, 3);

            // 更新 LOD 和角度捕捉
            cfg.LodLevel = (byte)Math.Clamp(LodLevel, 0, 2);
            cfg.AngleSnapping = AngleSnapping;

            // 同步更新 Dpi 字段
            if (cfg.CurrentDpiSlot < 4 && cfg.DpiSlots[cfg.CurrentDpiSlot] > 0)
            {
                cfg.Dpi = cfg.DpiSlots[cfg.CurrentDpiSlot];
                CurrentDpi = cfg.Dpi;
            }

            // 写入设备
            await _deviceService.SaveConfigAsync(cfg);
        }
        catch
        {
            // 忽略保存错误
        }
        finally
        {
            _isSaving = false;
            IsApplying = false;
        }
    }

    partial void OnCurrentDpiSlotChanged(int value)
    {
        if (!_isInitialized || _isSaving) return;

        // 切换当前档位时更新显示的 DPI 值
        int[] slots = { DpiSlot0, DpiSlot1, DpiSlot2, DpiSlot3 };
        if (value >= 0 && value < 4)
        {
            CurrentDpi = slots[value];
            CurrentDpiIndex = value;
        }
    }

    partial void OnLodLevelChanged(int value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveAdvancedConfigAsync();
    }

    partial void OnAngleSnappingChanged(bool value)
    {
        if (_isInitialized && !_isSaving)
            _ = SaveAdvancedConfigAsync();
    }
}
