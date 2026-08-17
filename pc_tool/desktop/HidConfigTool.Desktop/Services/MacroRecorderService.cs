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

    public bool StartRecording() => false;
    public List<MacroAction> StopRecording() => new List<MacroAction>();
    public void Clear() { }
}
