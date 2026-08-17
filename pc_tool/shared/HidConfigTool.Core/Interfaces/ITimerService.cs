namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 定时器服务抽象，用于替代平台特定的 DispatcherTimer
/// </summary>
public interface ITimerService
{
    /// <summary>定时器间隔</summary>
    TimeSpan Interval { get; set; }

    /// <summary>是否正在运行</summary>
    bool IsEnabled { get; }

    /// <summary>定时触发事件</summary>
    event EventHandler Tick;

    /// <summary>启动定时器</summary>
    void Start();

    /// <summary>停止定时器</summary>
    void Stop();
}
