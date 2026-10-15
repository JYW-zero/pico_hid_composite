using System.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 页面视图模型基类（MVVM 最佳实践）
///
/// 统一处理所有页面 ViewModel 的共同逻辑：
/// 1. 设备连接状态管理（IsDeviceConnected）
/// 2. 设备连接事件订阅/取消订阅（自动 Dispose）
/// 3. UI 线程调度（RunOnUiThread）
/// 4. 状态消息（StatusMessage）
/// 5. Dispose 模式（标准 IDisposable 实现）
///
/// 设计原则：
/// - 模板方法模式：基类定义算法骨架，子类重写特定步骤
/// - 关注点分离：基类处理通用逻辑，子类专注业务逻辑
/// - DRY 原则：避免12个 ViewModel 重复相同代码
///
/// 子类只需：
/// 1. 调用 base 构造函数
/// 2. 重写 OnDeviceConnectedAsync / OnDeviceDisconnectedAsync（可选）
/// 3. 重写 DisposeManagedResources（可选，释放子类特有资源）
/// </summary>
public abstract partial class PageViewModelBase : ObservableObject, IDisposable
{
    /// <summary>
    /// 设备服务（保护字段，子类可直接访问）
    /// </summary>
    protected readonly IDeviceService DeviceService;

    /// <summary>
    /// UI 同步上下文（用于在非UI线程回调中切回UI线程）
    /// </summary>
    protected readonly SynchronizationContext? UiContext;

    /// <summary>
    /// 是否已释放（Dispose 模式标志）
    /// </summary>
    private bool _disposed;

    /// <summary>
    /// 是否已连接设备
    /// </summary>
    [ObservableProperty]
    private bool _isDeviceConnected;

    /// <summary>
    /// 状态消息（显示在UI上的当前状态）
    /// </summary>
    [ObservableProperty]
    private string _statusMessage = "未连接设备";

    /// <summary>
    /// 基类构造函数
    /// 自动订阅设备连接事件，并初始化连接状态
    /// </summary>
    /// <param name="deviceService">设备服务</param>
    protected PageViewModelBase(IDeviceService deviceService)
    {
        DeviceService = deviceService ?? throw new ArgumentNullException(nameof(deviceService));
        UiContext = SynchronizationContext.Current;

        // 订阅连接状态变化事件（自动连接/热插拔时更新UI）
        DeviceService.DeviceConnectionChanged += OnDeviceConnectionChanged;

        // 如果已经连接，初始化状态
        if (DeviceService.IsConnected)
        {
            IsDeviceConnected = true;
            StatusMessage = "已连接";
            // 不调用 OnDeviceConnectedAsync，因为构造函数中不能 await
            // 子类可以在构造函数后手动调用 InitializeAsync
        }
    }

    /// <summary>
    /// 异步初始化（可选）
    /// 用于在构造函数后执行需要 await 的初始化逻辑
    /// 如果设备已连接，会调用 OnDeviceConnectedAsync
    /// </summary>
    protected virtual async Task InitializeAsync()
    {
        if (DeviceService.IsConnected)
        {
            await OnDeviceConnectedAsync();
        }
    }

    /// <summary>
    /// 设备连接状态变化事件处理（模板方法）
    /// 基类处理通用逻辑（更新状态、调用虚方法），子类重写虚方法实现特定逻辑
    /// </summary>
    private async void OnDeviceConnectionChanged(object? sender, bool isConnected)
    {
        if (_disposed) return;

        try
        {
            IsDeviceConnected = isConnected;

            if (isConnected)
            {
                StatusMessage = "已连接";
                await OnDeviceConnectedAsync();
            }
            else
            {
                StatusMessage = "已断开连接";
                await OnDeviceDisconnectedAsync();
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"连接状态处理失败: {ex.Message}";
            System.Diagnostics.Debug.WriteLine($"[{GetType().Name}] OnDeviceConnectionChanged 异常: {ex}");
        }
    }

    /// <summary>
    /// 设备已连接（虚方法，子类重写）
    /// 用于执行连接后的初始化逻辑，如加载配置、启动定时器等
    /// </summary>
    protected virtual Task OnDeviceConnectedAsync()
    {
        return Task.CompletedTask;
    }

    /// <summary>
    /// 设备已断开（虚方法，子类重写）
    /// 用于执行断开后的清理逻辑，如清除数据、停止定时器等
    /// </summary>
    protected virtual Task OnDeviceDisconnectedAsync()
    {
        return Task.CompletedTask;
    }

    /// <summary>
    /// 在UI线程执行操作
    /// 如果当前不在UI线程，通过 SynchronizationContext 切换
    /// </summary>
    /// <param name="action">要执行的操作</param>
    protected void RunOnUiThread(Action action)
    {
        if (action == null) throw new ArgumentNullException(nameof(action));

        if (UiContext != null)
        {
            UiContext.Post(_ => action(), null);
        }
        else
        {
            action();
        }
    }

    /// <summary>
    /// 在UI线程执行异步操作
    /// </summary>
    /// <param name="func">要执行的异步操作</param>
    protected async Task RunOnUiThreadAsync(Func<Task> func)
    {
        if (func == null) throw new ArgumentNullException(nameof(func));

        if (UiContext != null)
        {
            var tcs = new TaskCompletionSource<bool>();
            UiContext.Post(async _ =>
            {
                try
                {
                    await func();
                    tcs.SetResult(true);
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
            }, null);
            await tcs.Task;
        }
        else
        {
            await func();
        }
    }

    /// <summary>
    /// 设置状态消息（便捷方法）
    /// </summary>
    /// <param name="message">消息内容</param>
    protected void SetStatus(string message)
    {
        StatusMessage = message ?? string.Empty;
    }

    #region IDisposable 实现

    /// <summary>
    /// 释放资源（标准 Dispose 模式）
    /// </summary>
    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    /// <summary>
    /// 释放资源（模板方法）
    /// 基类释放通用资源，子类重写 DisposeManagedResources 释放特有资源
    /// </summary>
    /// <param name="disposing">是否释放托管资源</param>
    protected virtual void Dispose(bool disposing)
    {
        if (_disposed) return;

        if (disposing)
        {
            // 取消订阅事件（必须，否则会内存泄漏）
            DeviceService.DeviceConnectionChanged -= OnDeviceConnectionChanged;

            // 释放子类特有资源
            DisposeManagedResources();
        }

        _disposed = true;
    }

    /// <summary>
    /// 释放子类特有的托管资源（虚方法，子类重写）
    /// 用于释放定时器、事件订阅、集合等子类特有资源
    /// </summary>
    protected virtual void DisposeManagedResources()
    {
        // 子类重写此方法释放特有资源
    }

    /// <summary>
    /// 析构函数（用于非托管资源释放，当前无未托管资源，保留为标准模式）
    /// </summary>
    ~PageViewModelBase()
    {
        Dispose(false);
    }

    #endregion
}
