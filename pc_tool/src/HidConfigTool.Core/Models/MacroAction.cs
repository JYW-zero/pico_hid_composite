namespace HidConfigTool.Core.Models;

/// <summary>
/// 宏动作
/// </summary>
public class MacroAction
{
    public MacroActionType Type { get; set; }
    public int KeyCode { get; set; }
    public string KeyName { get; set; } = string.Empty;
    public int DelayMs { get; set; }
    public int DeltaX { get; set; }
    public int DeltaY { get; set; }
}
