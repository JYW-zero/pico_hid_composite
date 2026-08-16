using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.Json.Serialization.Metadata;
using HidConfigTool.Core.Models;

namespace HidConfigTool.App.Services;

/// <summary>
/// 配置文件管理器
/// </summary>
public class ConfigProfileManager
{
    private readonly string _profilesDirectory;

    /// <summary>
    /// JSON 序列化安全选项（禁止多态反序列化，防止类型混淆攻击）
    /// </summary>
    private static readonly JsonSerializerOptions _jsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        // 显式禁用多态类型推导，DeviceConfig 是简单 POCO 不需要多态
        TypeInfoResolver = new DefaultJsonTypeInfoResolver()
    };

    /// <summary>
    /// 配置文件列表
    /// </summary>
    public List<ConfigProfileInfo> Profiles { get; private set; } = new();

    /// <summary>
    /// 当前激活的配置文件名
    /// </summary>
    public string? CurrentProfileName { get; private set; }

    public ConfigProfileManager()
    {
        // 配置文件保存在用户 AppData 目录下
        string appDataPath = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        _profilesDirectory = Path.Combine(appDataPath, "HIDConfigTool", "Profiles");

        if (!Directory.Exists(_profilesDirectory))
        {
            Directory.CreateDirectory(_profilesDirectory);
        }

        LoadProfiles();
    }

    /// <summary>
    /// 加载所有配置文件
    /// </summary>
    public void LoadProfiles()
    {
        Profiles.Clear();

        if (!Directory.Exists(_profilesDirectory))
            return;

        var files = Directory.GetFiles(_profilesDirectory, "*.json");
        foreach (var file in files)
        {
            string name = Path.GetFileNameWithoutExtension(file);
            Profiles.Add(new ConfigProfileInfo
            {
                Name = name,
                FilePath = file,
                LastModified = File.GetLastWriteTime(file)
            });
        }

        Profiles = Profiles.OrderByDescending(p => p.LastModified).ToList();
    }

    /// <summary>
    /// 当前支持的最高配置版本
    /// </summary>
    private const int MAX_SUPPORTED_CONFIG_VERSION = 2;

    /// <summary>
    /// 保存配置为新的配置文件
    /// </summary>
    public bool SaveProfile(string name, DeviceConfig config)
    {
        try
        {
            string filePath = Path.Combine(_profilesDirectory, $"{name}.json");
            string json = JsonSerializer.Serialize(config, _jsonOptions);
            File.WriteAllText(filePath, json);

            LoadProfiles();
            return true;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"保存配置文件失败: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 加载配置文件
    /// </summary>
    public DeviceConfig? LoadProfile(string name)
    {
        try
        {
            string filePath = Path.Combine(_profilesDirectory, $"{name}.json");
            if (!File.Exists(filePath))
                return null;

            string json = File.ReadAllText(filePath);
            var config = JsonSerializer.Deserialize<DeviceConfig>(json, _jsonOptions);

            if (config != null)
            {
                // 版本兼容性检查
                if (config.Version > MAX_SUPPORTED_CONFIG_VERSION)
                {
                    config.HasVersionWarning = true;
                    System.Diagnostics.Debug.WriteLine($"配置文件版本 {config.Version} 高于当前支持版本 {MAX_SUPPORTED_CONFIG_VERSION}");
                }
                CurrentProfileName = name;
            }

            return config;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"加载配置文件失败: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 删除配置文件
    /// </summary>
    public bool DeleteProfile(string name)
    {
        try
        {
            string filePath = Path.Combine(_profilesDirectory, $"{name}.json");
            if (File.Exists(filePath))
            {
                File.Delete(filePath);
                LoadProfiles();
                return true;
            }
            return false;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"删除配置文件失败: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 重命名配置文件
    /// </summary>
    public bool RenameProfile(string oldName, string newName)
    {
        try
        {
            string oldPath = Path.Combine(_profilesDirectory, $"{oldName}.json");
            string newPath = Path.Combine(_profilesDirectory, $"{newName}.json");

            if (!File.Exists(oldPath))
                return false;

            File.Move(oldPath, newPath);
            LoadProfiles();
            return true;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"重命名配置文件失败: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 检查配置名是否已存在
    /// </summary>
    public bool ProfileExists(string name)
    {
        return Profiles.Any(p => p.Name == name);
    }
}

/// <summary>
/// 配置文件信息
/// </summary>
public class ConfigProfileInfo
{
    public string Name { get; set; } = string.Empty;
    public string FilePath { get; set; } = string.Empty;
    public DateTime LastModified { get; set; }
}
