using System.Windows.Input;
using HidConfigTool.Core.Models;

namespace HidConfigTool.App.Services;

/// <summary>
/// 宏录制器
/// 使用全局键盘钩子录制按键序列
/// </summary>
public class MacroRecorder
{
    private readonly KeyboardHook _keyboardHook;
    private readonly List<MacroAction> _actions = new();
    private DateTime _startTime;
    private DateTime _lastEventTime;

    /// <summary>
    /// 是否正在录制
    /// </summary>
    public bool IsRecording { get; private set; }

    /// <summary>
    /// 录制的动作列表
    /// </summary>
    public IReadOnlyList<MacroAction> Actions => _actions;

    /// <summary>
    /// 录制开始时间
    /// </summary>
    public DateTime StartTime => _startTime;

    /// <summary>
    /// 录制时长
    /// </summary>
    public TimeSpan Duration => _lastEventTime - _startTime;

    /// <summary>
    /// 录制过程中事件触发
    /// </summary>
    public event EventHandler<MacroAction>? ActionRecorded;

    public MacroRecorder(KeyboardHook keyboardHook)
    {
        _keyboardHook = keyboardHook;
        _keyboardHook.KeyEvent += OnKeyEvent;
    }

    /// <summary>
    /// 开始录制
    /// </summary>
    public bool StartRecording()
    {
        if (IsRecording)
            return false;

        _actions.Clear();
        _startTime = DateTime.Now;
        _lastEventTime = _startTime;

        if (_keyboardHook.Hook())
        {
            IsRecording = true;
            return true;
        }

        return false;
    }

    /// <summary>
    /// 停止录制
    /// </summary>
    public List<MacroAction> StopRecording()
    {
        if (!IsRecording)
            return _actions;

        _keyboardHook.Unhook();
        IsRecording = false;

        return new List<MacroAction>(_actions);
    }

    private void OnKeyEvent(object? sender, KeyHookEventArgs e)
    {
        if (!IsRecording)
            return;

        // 计算相对延迟（毫秒）
        int delayMs = (int)(e.Timestamp - _lastEventTime).TotalMilliseconds;
        _lastEventTime = e.Timestamp;

        // 将 WPF Key 转换为 HID Usage 码（固件使用 HID Usage 码，不是 VK 码）
        byte hidUsage = HidKeyConverter.KeyToHidUsage(e.Key);
        string keyName = hidUsage != 0
            ? HidKeyConverter.HidUsageToName(hidUsage)
            : e.Key.ToString();

        // 创建宏动作
        var action = new MacroAction
        {
            Type = e.IsPressed ? MacroActionType.KeyDown : MacroActionType.KeyUp,
            KeyCode = hidUsage,
            KeyName = keyName,
            DelayMs = delayMs
        };

        _actions.Add(action);
        ActionRecorded?.Invoke(this, action);
    }

    /// <summary>
    /// 清空录制内容
    /// </summary>
    public void Clear()
    {
        _actions.Clear();
    }
}
