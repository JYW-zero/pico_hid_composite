using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 系统托盘服务（基础实现）
/// </summary>
public class TrayIconService : ITrayIconService
{
    public bool MinimizeToTray { get; set; } = true;
}
