using CommunityToolkit.Mvvm.ComponentModel;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 编码器设置页面视图模型
/// </summary>
public partial class EncoderPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private bool _isInitialized;
    private bool _isSaving;
    private CancellationTokenSource? _saveDebounceCts;

    [ObservableProperty]
    private bool _reverseDirection;

    [ObservableProperty]
    private int _stepsPerTick = 1;

    [ObservableProperty]
    private int _scrollSpeed = 3;

    [ObservableProperty]
    private string _statusMessage = "就绪";

    public EncoderPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 从当前配置加载
        if (_deviceService.IsConnected && _deviceService.CurrentConfig != null)
        {
            var cfg = _deviceService.CurrentConfig;
            // 防御性修复：v3新字段可能为0（旧配置），重置为默认值
            if (cfg.EncoderStepsPerTick == 0)
                cfg.EncoderStepsPerTick = 1;
            if (cfg.EncoderScrollSpeed == 0)
                cfg.EncoderScrollSpeed = 3;

            ReverseDirection = cfg.EncoderReverse;
            StepsPerTick = cfg.EncoderStepsPerTick;
            ScrollSpeed = cfg.EncoderScrollSpeed;
        }

        _isInitialized = true;
    }

    partial void OnReverseDirectionChanged(bool value)
    {
        if (_isInitialized)
            ScheduleSave();
    }

    partial void OnStepsPerTickChanged(int value)
    {
        if (_isInitialized)
            ScheduleSave();
    }

    partial void OnScrollSpeedChanged(int value)
    {
        if (_isInitialized)
            ScheduleSave();
    }

    /// <summary>
    /// 延迟保存，避免频繁写Flash
    /// </summary>
    private void ScheduleSave()
    {
        _saveDebounceCts?.Cancel();
        _saveDebounceCts = new CancellationTokenSource();
        var token = _saveDebounceCts.Token;
        _ = Task.Run(async () =>
        {
            try
            {
                await Task.Delay(500, token);
                if (!token.IsCancellationRequested)
                    await SaveConfigAsync();
            }
            catch (TaskCanceledException) { }
        });
    }

    /// <summary>
    /// 保存完整配置到Flash，直接从ViewModel属性更新所有字段
    /// </summary>
    private async Task SaveConfigAsync()
    {
        if (!_deviceService.IsConnected || _isSaving)
            return;

        try
        {
            _isSaving = true;

            // 直接从ViewModel属性更新CurrentConfig所有字段
            var cfg = _deviceService.CurrentConfig;
            if (cfg != null)
            {
                cfg.EncoderReverse = ReverseDirection;
                cfg.EncoderStepsPerTick = Math.Clamp(StepsPerTick, 1, 10);
                cfg.EncoderScrollSpeed = Math.Clamp(ScrollSpeed, 1, 10);
            }

            bool result = await _deviceService.SetEncoderReverseAsync(ReverseDirection);
            if (result)
            {
                StatusMessage = $"配置已保存: 反转={ReverseDirection}, 步长={StepsPerTick}, 速度={ScrollSpeed}";
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
            _isSaving = false;
        }
    }
}
