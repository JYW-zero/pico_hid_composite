using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace HidConfigTool.Core.Models;

/// <summary>
/// 宏定义
/// </summary>
public class Macro : INotifyPropertyChanged
{
    private string _name = "新建宏";
    private int _repeatCount = 1;
    private bool _repeatUntilReleased;

    public string Id { get; set; } = Guid.NewGuid().ToString();

    public string Name
    {
        get => _name;
        set
        {
            _name = value;
            OnPropertyChanged();
        }
    }

    public List<MacroAction> Actions { get; set; } = new();

    /// <summary>
    /// 循环次数（0=无限循环，直到松开触发键）
    /// </summary>
    public int RepeatCount
    {
        get => _repeatCount;
        set
        {
            _repeatCount = value;
            // 循环次数为0时自动开启"按住重复"模式
            if (value == 0 && !_repeatUntilReleased)
            {
                _repeatUntilReleased = true;
                OnPropertyChanged(nameof(RepeatUntilReleased));
            }
            else if (value > 0 && _repeatUntilReleased)
            {
                _repeatUntilReleased = false;
                OnPropertyChanged(nameof(RepeatUntilReleased));
            }
            OnPropertyChanged();
        }
    }

    /// <summary>
    /// 按住重复，松开停止（对应固件 repeat_count=0）
    /// </summary>
    public bool RepeatUntilReleased
    {
        get => _repeatUntilReleased;
        set
        {
            _repeatUntilReleased = value;
            // 开启"按住重复"时，循环次数自动设为0
            if (value && _repeatCount != 0)
            {
                _repeatCount = 0;
                OnPropertyChanged(nameof(RepeatCount));
            }
            // 关闭"按住重复"时，循环次数恢复为1
            else if (!value && _repeatCount == 0)
            {
                _repeatCount = 1;
                OnPropertyChanged(nameof(RepeatCount));
            }
            OnPropertyChanged();
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    protected void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}
