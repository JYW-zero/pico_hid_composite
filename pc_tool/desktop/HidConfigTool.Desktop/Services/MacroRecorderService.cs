using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 宏录制服务（基础实现，跨平台键盘钩子需要平台API）
/// </summary>
public class MacroRecorderService : IMacroRecorder
{
    public bool IsRecording => false;
    public IReadOnlyList<MacroAction> Actions => new List<MacroAction>();
    public DateTime StartTime => DateTime.Now;
    public TimeSpan Duration => TimeSpan.Zero;

    public event EventHandler<MacroAction>? ActionRecorded;

    /// <summary>
    /// 触发 ActionRecorded 事件（派生类可重写以实现实际录制）
    /// </summary>
    protected virtual void OnActionRecorded(MacroAction action)
    {
        ActionRecorded?.Invoke(this, action);
    }

    public bool StartRecording() => false;
    public List<MacroAction> StopRecording() => new List<MacroAction>();
    public void Clear() { }
}
