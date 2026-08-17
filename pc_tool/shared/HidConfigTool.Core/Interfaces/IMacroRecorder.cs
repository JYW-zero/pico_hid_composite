using HidConfigTool.Core.Models;

namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 宏录制服务抽象，用于替代平台特定的键盘钩子录制
/// </summary>
public interface IMacroRecorder
{
    /// <summary>是否正在录制</summary>
    bool IsRecording { get; }

    /// <summary>已录制的动作列表</summary>
    IReadOnlyList<MacroAction> Actions { get; }

    /// <summary>录制开始时间</summary>
    DateTime StartTime { get; }

    /// <summary>录制时长</summary>
    TimeSpan Duration { get; }

    /// <summary>录制到新动作时触发</summary>
    event EventHandler<MacroAction>? ActionRecorded;

    /// <summary>开始录制</summary>
    bool StartRecording();

    /// <summary>停止录制，返回录制的动作列表</summary>
    List<MacroAction> StopRecording();

    /// <summary>清空已录制的动作</summary>
    void Clear();
}
