namespace HidConfigTool.Core.Models;

/// <summary>
/// 宏定义
/// </summary>
public class Macro
{
    public string Id { get; set; } = Guid.NewGuid().ToString();
    public string Name { get; set; } = "新建宏";
    public List<MacroAction> Actions { get; set; } = new();
    public int RepeatCount { get; set; } = 1;
    public bool RepeatUntilReleased { get; set; }
}
