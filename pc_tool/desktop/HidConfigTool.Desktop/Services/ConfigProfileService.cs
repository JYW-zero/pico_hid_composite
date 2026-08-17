using System.IO;
using System.Text.Json;
using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// 配置文件管理服务实现（跨平台，基于文件系统）
/// </summary>
public class ConfigProfileService : IConfigProfileService
{
    private readonly string _profilesDirectory;
    private static readonly JsonSerializerOptions _jsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true
    };

    public List<ConfigProfileInfo> Profiles { get; private set; } = new();

    public ConfigProfileService()
    {
        _profilesDirectory = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "HIDConfigTool", "Profiles");
        Directory.CreateDirectory(_profilesDirectory);
        LoadProfiles();
    }

    public void LoadProfiles()
    {
        Profiles.Clear();
        if (Directory.Exists(_profilesDirectory))
        {
            foreach (var file in Directory.GetFiles(_profilesDirectory, "*.json"))
            {
                Profiles.Add(new ConfigProfileInfo
                {
                    Name = Path.GetFileNameWithoutExtension(file),
                    FilePath = file,
                    LastModified = File.GetLastWriteTime(file)
                });
            }
        }
    }

    public DeviceConfig? LoadProfile(string name)
    {
        var filePath = Path.Combine(_profilesDirectory, $"{name}.json");
        if (!File.Exists(filePath)) return null;
        try
        {
            var json = File.ReadAllText(filePath);
            return JsonSerializer.Deserialize<DeviceConfig>(json, _jsonOptions);
        }
        catch { return null; }
    }

    public bool SaveProfile(string name, DeviceConfig config)
    {
        try
        {
            var filePath = Path.Combine(_profilesDirectory, $"{name}.json");
            var json = JsonSerializer.Serialize(config, _jsonOptions);
            File.WriteAllText(filePath, json);
            LoadProfiles();
            return true;
        }
        catch { return false; }
    }

    public bool DeleteProfile(string name)
    {
        try
        {
            var filePath = Path.Combine(_profilesDirectory, $"{name}.json");
            if (File.Exists(filePath))
            {
                File.Delete(filePath);
                LoadProfiles();
            }
            return true;
        }
        catch { return false; }
    }

    public bool RenameProfile(string oldName, string newName)
    {
        try
        {
            var oldPath = Path.Combine(_profilesDirectory, $"{oldName}.json");
            var newPath = Path.Combine(_profilesDirectory, $"{newName}.json");
            if (File.Exists(oldPath))
            {
                File.Move(oldPath, newPath);
                LoadProfiles();
            }
            return true;
        }
        catch { return false; }
    }

    public bool ProfileExists(string name)
    {
        return Profiles.Any(p => p.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
    }
}
