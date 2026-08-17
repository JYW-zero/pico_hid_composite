using System.Windows.Threading;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.Services;

/// <summary>
/// WPF 平台定时器服务，基于 DispatcherTimer
/// </summary>
public class TimerService : ITimerService
{
    private readonly DispatcherTimer _timer;

    public TimerService()
    {
        _timer = new DispatcherTimer();
        _timer.Tick += (s, e) => Tick?.Invoke(s, e);
    }

    public TimeSpan Interval
    {
        get => _timer.Interval;
        set => _timer.Interval = value;
    }

    public bool IsEnabled => _timer.IsEnabled;

    public event EventHandler? Tick;

    public void Start() => _timer.Start();

    public void Stop() => _timer.Stop();
}
