namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 开机自启动服务抽象
/// </summary>
public interface IAutoStartService
{
    bool IsEnabled();
    bool Toggle(bool enable);
}
