namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// UI 线程调度服务抽象，用于在非 UI 线程更新绑定属性
/// </summary>
public interface IUiThreadService
{
    /// <summary>在 UI 线程上同步执行</summary>
    void Invoke(Action action);

    /// <summary>在 UI 线程上异步执行</summary>
    Task InvokeAsync(Action action);
}
