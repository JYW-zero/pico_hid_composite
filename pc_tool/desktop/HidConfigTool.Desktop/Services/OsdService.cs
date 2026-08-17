using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia OSD 屏幕显示服务（基础实现）
/// </summary>
public class OsdService : IOsdService
{
    public bool IsEnabled { get; set; } = true;

    public void ShowProfileChange(string profileName)
    {
        // 后续可替换为自定义通知窗口
    }
}
