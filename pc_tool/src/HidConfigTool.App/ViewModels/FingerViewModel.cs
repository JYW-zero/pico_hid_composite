using CommunityToolkit.Mvvm.ComponentModel;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 六向按键方向
/// </summary>
public enum SixWayDirection
{
    Up,     // +Y 上
    Down,   // -Y 下
    Left,   // -X 左
    Right,  // +X 右
    Front,  // -Z 前
    Back    // +Z 后
}

/// <summary>
/// 手指视图模型
/// </summary>
public partial class FingerViewModel : ObservableObject
{
    /// <summary>
    /// 手指名称
    /// </summary>
    public string Name { get; set; } = string.Empty;

    /// <summary>
    /// 所属手：Left / Right
    /// </summary>
    public string Hand { get; set; } = string.Empty;

    /// <summary>
    /// 手指索引（0-4）
    /// </summary>
    public int FingerIndex { get; set; }

    /// <summary>
    /// 是否选中
    /// </summary>
    [ObservableProperty]
    private bool _isSelected;

    // 六个方向的按键
    public KeyItemViewModel? Up { get; set; }    // +Y 上
    public KeyItemViewModel? Down { get; set; }  // -Y 下
    public KeyItemViewModel? Left { get; set; }  // -X 左
    public KeyItemViewModel? Right { get; set; } // +X 右
    public KeyItemViewModel? Front { get; set; } // -Z 前
    public KeyItemViewModel? Back { get; set; }  // +Z 后

    /// <summary>
    /// 根据方向获取按键
    /// </summary>
    public KeyItemViewModel? GetKey(SixWayDirection direction)
    {
        return direction switch
        {
            SixWayDirection.Up => Up,
            SixWayDirection.Down => Down,
            SixWayDirection.Left => Left,
            SixWayDirection.Right => Right,
            SixWayDirection.Front => Front,
            SixWayDirection.Back => Back,
            _ => null
        };
    }
}
