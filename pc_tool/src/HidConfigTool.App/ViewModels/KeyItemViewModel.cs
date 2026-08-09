using CommunityToolkit.Mvvm.ComponentModel;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 单个按键视图模型
/// </summary>
public partial class KeyItemViewModel : ObservableObject
{
    /// <summary>
    /// 按键索引（0-63）
    /// </summary>
    public int Index { get; set; }

    /// <summary>
    /// 行号
    /// </summary>
    public int Row { get; set; }

    /// <summary>
    /// 列号
    /// </summary>
    public int Column { get; set; }

    /// <summary>
    /// 键码（HID 用法码）
    /// </summary>
    [ObservableProperty]
    private byte _keyCode;

    /// <summary>
    /// 显示的键名
    /// </summary>
    [ObservableProperty]
    private string _keyName = "?";

    /// <summary>
    /// 是否是修饰键
    /// </summary>
    [ObservableProperty]
    private bool _isModifier;

    /// <summary>
    /// 是否是 Fn 层
    /// </summary>
    [ObservableProperty]
    private bool _isFnLayer;
}
