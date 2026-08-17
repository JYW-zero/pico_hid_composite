namespace HidConfigTool.Core.Models;

/// <summary>
/// 配置文件信息
/// </summary>
public class ConfigProfileInfo
{
    public string Name { get; set; } = string.Empty;
    public string FilePath { get; set; } = string.Empty;
    public DateTime LastModified { get; set; }
}
