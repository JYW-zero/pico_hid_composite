using HidConfigTool.Core.Models;

namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 配置文件管理服务抽象
/// </summary>
public interface IConfigProfileService
{
    List<ConfigProfileInfo> Profiles { get; }
    DeviceConfig? LoadProfile(string name);
    bool SaveProfile(string name, DeviceConfig config);
    bool DeleteProfile(string name);
    bool RenameProfile(string oldName, string newName);
    bool ProfileExists(string name);
}
