namespace HidConfigTool.Core.Models;

/// <summary>
/// 应用感知规则
/// </summary>
public class AppAwarenessRule
{
    public string ProcessName { get; set; } = string.Empty;
    public string AppName { get; set; } = string.Empty;
    public string ProfileName { get; set; } = string.Empty;
    public bool IsEnabled { get; set; } = true;
}
