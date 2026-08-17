using System.Windows;
using System.Windows.Threading;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.Services;

/// <summary>
/// WPF 平台 UI 线程调度服务
/// </summary>
public class UiThreadService : IUiThreadService
{
    public void Invoke(Action action)
    {
        if (Application.Current?.Dispatcher.CheckAccess() == true)
        {
            action();
        }
        else
        {
            Application.Current?.Dispatcher.Invoke(action);
        }
    }

    public async Task InvokeAsync(Action action)
    {
        if (Application.Current?.Dispatcher.CheckAccess() == true)
        {
            action();
        }
        else
        {
            await Application.Current?.Dispatcher.InvokeAsync(action)!;
        }
    }
}
