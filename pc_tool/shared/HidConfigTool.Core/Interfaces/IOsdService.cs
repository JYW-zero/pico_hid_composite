namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// OSD 屏幕显示服务抽象
/// </summary>
public interface IOsdService
{
    bool IsEnabled { get; set; }
    void ShowProfileChange(string profileName);
}
