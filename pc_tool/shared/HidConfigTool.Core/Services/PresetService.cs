namespace HidConfigTool.Core.Services;

using System.Collections.ObjectModel;
using System.Text.Json;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

/// <summary>
/// 配置预设服务
/// 管理内置预设和用户预设，支持加载、保存、应用预设
/// </summary>
public class PresetService
{
    private readonly IDeviceService _deviceService;
    private readonly string _userPresetsPath;
    private readonly List<ConfigPreset> _builtInPresets = new();
    private readonly List<ConfigPreset> _userPresets = new();

    /// <summary>
    /// 所有预设（内置 + 用户）
    /// </summary>
    public ObservableCollection<ConfigPreset> AllPresets { get; } = new();

    /// <summary>
    /// 预设分类列表
    /// </summary>
    public ObservableCollection<string> Categories { get; } = new();

    public PresetService(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 用户预设存储路径
        var appDataPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "HIDConfigTool", "Presets");
        _userPresetsPath = appDataPath;

        // 确保目录存在
        Directory.CreateDirectory(_userPresetsPath);

        // 加载内置预设
        LoadBuiltInPresets();

        // 加载用户预设
        LoadUserPresets();

        // 刷新所有预设列表
        RefreshAllPresets();
    }

    /// <summary>
    /// 加载内置预设
    /// </summary>
    private void LoadBuiltInPresets()
    {
        _builtInPresets.Clear();

        // 办公预设
        _builtInPresets.Add(new ConfigPreset
        {
            Id = "builtin_office",
            Name = "办公模式",
            Description = "适合日常办公、文档编辑，DPI 适中，指针精确",
            Category = "办公",
            IsBuiltIn = true,
            Icon = "💼",
            Config = new DeviceConfig
            {
                Dpi = 1000,
                DpiIndex = 1,
                DpiLevels = new[] { 400, 800, 1600, 3200 },
                AccelerationEnabled = false,
                JoystickDeadzone = 150,
                JoystickSensitivity = 1.0,
                EncoderScrollSpeed = 3,
                LodLevel = 1,
                AngleSnapping = false
            }
        });

        // 游戏预设
        _builtInPresets.Add(new ConfigPreset
        {
            Id = "builtin_gaming",
            Name = "游戏模式",
            Description = "适合 FPS/MOBA 游戏，高 DPI，低延迟，角度捕捉开启",
            Category = "游戏",
            IsBuiltIn = true,
            Icon = "🎮",
            Config = new DeviceConfig
            {
                Dpi = 1600,
                DpiIndex = 2,
                DpiLevels = new[] { 400, 800, 1600, 3200 },
                AccelerationEnabled = false,
                JoystickDeadzone = 200,
                JoystickSensitivity = 1.2,
                EncoderScrollSpeed = 5,
                LodLevel = 0,
                AngleSnapping = true
            }
        });

        // 设计预设
        _builtInPresets.Add(new ConfigPreset
        {
            Id = "builtin_design",
            Name = "设计模式",
            Description = "适合图形设计、绘画，低 DPI，精确控制，指针加速",
            Category = "设计",
            IsBuiltIn = true,
            Icon = "🎨",
            Config = new DeviceConfig
            {
                Dpi = 600,
                DpiIndex = 0,
                DpiLevels = new[] { 400, 800, 1600, 3200 },
                AccelerationEnabled = true,
                AccelerationThreshold = 8,
                AccelerationRatio = 1.3,
                JoystickDeadzone = 100,
                JoystickSensitivity = 0.8,
                EncoderScrollSpeed = 2,
                LodLevel = 2,
                AngleSnapping = false
            }
        });

        // 编程预设
        _builtInPresets.Add(new ConfigPreset
        {
            Id = "builtin_coding",
            Name = "编程模式",
            Description = "适合代码编辑，中等 DPI，快速滚动，宏快捷键",
            Category = "编程",
            IsBuiltIn = true,
            Icon = "💻",
            Config = new DeviceConfig
            {
                Dpi = 1200,
                DpiIndex = 1,
                DpiLevels = new[] { 400, 800, 1600, 3200 },
                AccelerationEnabled = false,
                JoystickDeadzone = 120,
                JoystickSensitivity = 1.0,
                EncoderScrollSpeed = 4,
                LodLevel = 1,
                AngleSnapping = false
            }
        });

        // 省电预设
        _builtInPresets.Add(new ConfigPreset
        {
            Id = "builtin_powersave",
            Name = "省电模式",
            Description = "低 DPI，低回报率，延长设备续航",
            Category = "省电",
            IsBuiltIn = true,
            Icon = "🔋",
            Config = new DeviceConfig
            {
                Dpi = 800,
                DpiIndex = 1,
                DpiLevels = new[] { 400, 800, 1600, 3200 },
                AccelerationEnabled = false,
                JoystickDeadzone = 200,
                JoystickSensitivity = 1.0,
                EncoderScrollSpeed = 3,
                LodLevel = 1,
                AngleSnapping = false
            }
        });
    }

    /// <summary>
    /// 加载用户预设
    /// </summary>
    private void LoadUserPresets()
    {
        _userPresets.Clear();

        try
        {
            if (!Directory.Exists(_userPresetsPath))
                return;

            var files = Directory.GetFiles(_userPresetsPath, "*.json");
            foreach (var file in files)
            {
                try
                {
                    var json = File.ReadAllText(file);
                    var preset = JsonSerializer.Deserialize<ConfigPreset>(json, new JsonSerializerOptions
                    {
                        PropertyNameCaseInsensitive = true
                    });
                    if (preset != null && !preset.IsBuiltIn)
                    {
                        _userPresets.Add(preset);
                    }
                }
                catch
                {
                    // 忽略损坏的预设文件
                }
            }
        }
        catch
        {
            // 加载失败时使用空列表
        }
    }

    /// <summary>
    /// 保存用户预设
    /// </summary>
    private void SaveUserPreset(ConfigPreset preset)
    {
        try
        {
            var filePath = Path.Combine(_userPresetsPath, $"{preset.Id}.json");
            var json = JsonSerializer.Serialize(preset, new JsonSerializerOptions
            {
                WriteIndented = true
            });
            File.WriteAllText(filePath, json);
        }
        catch
        {
            // 保存失败时静默处理
        }
    }

    /// <summary>
    /// 删除用户预设文件
    /// </summary>
    private void DeleteUserPresetFile(ConfigPreset preset)
    {
        try
        {
            var filePath = Path.Combine(_userPresetsPath, $"{preset.Id}.json");
            if (File.Exists(filePath))
            {
                File.Delete(filePath);
            }
        }
        catch
        {
            // 删除失败时静默处理
        }
    }

    /// <summary>
    /// 刷新所有预设列表
    /// </summary>
    private void RefreshAllPresets()
    {
        AllPresets.Clear();
        Categories.Clear();

        var allCategories = new HashSet<string>();

        foreach (var preset in _builtInPresets)
        {
            AllPresets.Add(preset);
            allCategories.Add(preset.Category);
        }

        foreach (var preset in _userPresets)
        {
            AllPresets.Add(preset);
            allCategories.Add(preset.Category);
        }

        foreach (var category in allCategories.OrderBy(c => c))
        {
            Categories.Add(category);
        }
    }

    /// <summary>
    /// 应用预设到设备
    /// </summary>
    /// <param name="preset">要应用的预设</param>
    /// <returns>是否成功</returns>
    public async Task<bool> ApplyPresetAsync(ConfigPreset preset)
    {
        if (!_deviceService.IsConnected)
            return false;

        try
        {
            // 克隆配置以避免修改原始预设
            var config = preset.Config;
            var result = await _deviceService.SaveConfigAsync(config);
            return result;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 从当前设备配置创建用户预设
    /// </summary>
    /// <param name="name">预设名称</param>
    /// <param name="description">预设描述</param>
    /// <param name="category">预设分类</param>
    /// <param name="icon">预设图标</param>
    /// <returns>创建的预设</returns>
    public ConfigPreset? CreatePresetFromCurrent(string name, string description, string category, string icon = "📋")
    {
        var currentConfig = _deviceService.CurrentConfig;
        if (currentConfig == null)
            return null;

        var preset = new ConfigPreset
        {
            Name = name,
            Description = description,
            Category = category,
            IsBuiltIn = false,
            Icon = icon,
            Config = new DeviceConfig
            {
                Version = currentConfig.Version,
                Dpi = currentConfig.Dpi,
                DpiIndex = currentConfig.DpiIndex,
                DpiLevels = (int[])currentConfig.DpiLevels.Clone(),
                AccelerationEnabled = currentConfig.AccelerationEnabled,
                AccelerationThreshold = currentConfig.AccelerationThreshold,
                AccelerationRatio = currentConfig.AccelerationRatio,
                JoystickDeadzone = currentConfig.JoystickDeadzone,
                JoystickSensitivity = currentConfig.JoystickSensitivity,
                JoystickInvertX = currentConfig.JoystickInvertX,
                JoystickInvertY = currentConfig.JoystickInvertY,
                JoystickCenterX = currentConfig.JoystickCenterX,
                JoystickCenterY = currentConfig.JoystickCenterY,
                JoystickCalibrated = currentConfig.JoystickCalibrated,
                DpiSlots = (ushort[])currentConfig.DpiSlots.Clone(),
                CurrentDpiSlot = currentConfig.CurrentDpiSlot,
                LodLevel = currentConfig.LodLevel,
                AngleSnapping = currentConfig.AngleSnapping,
                DpiSwitchKey = currentConfig.DpiSwitchKey,
                Layer2Keymap = (byte[])currentConfig.Layer2Keymap.Clone(),
                Layer3Keymap = (byte[])currentConfig.Layer3Keymap.Clone(),
                EncoderReverse = currentConfig.EncoderReverse,
                EncoderStepsPerTick = currentConfig.EncoderStepsPerTick,
                EncoderScrollSpeed = currentConfig.EncoderScrollSpeed,
                Keymap = (byte[])currentConfig.Keymap.Clone(),
                FnKeymap = (byte[])currentConfig.FnKeymap.Clone()
            }
        };

        _userPresets.Add(preset);
        SaveUserPreset(preset);
        RefreshAllPresets();

        return preset;
    }

    /// <summary>
    /// 更新用户预设
    /// </summary>
    /// <param name="preset">要更新的预设</param>
    public void UpdatePreset(ConfigPreset preset)
    {
        if (preset.IsBuiltIn)
            return;

        var index = _userPresets.FindIndex(p => p.Id == preset.Id);
        if (index >= 0)
        {
            preset.ModifiedAt = DateTime.Now;
            _userPresets[index] = preset;
            SaveUserPreset(preset);
            RefreshAllPresets();
        }
    }

    /// <summary>
    /// 删除用户预设
    /// </summary>
    /// <param name="preset">要删除的预设</param>
    /// <returns>是否成功</returns>
    public bool DeletePreset(ConfigPreset preset)
    {
        if (preset.IsBuiltIn)
            return false;

        var index = _userPresets.FindIndex(p => p.Id == preset.Id);
        if (index >= 0)
        {
            _userPresets.RemoveAt(index);
            DeleteUserPresetFile(preset);
            RefreshAllPresets();
            return true;
        }

        return false;
    }

    /// <summary>
    /// 复制预设（内置预设复制为用户预设）
    /// </summary>
    /// <param name="preset">要复制的预设</param>
    /// <returns>复制后的预设</returns>
    public ConfigPreset DuplicatePreset(ConfigPreset preset)
    {
        var duplicate = preset.Clone();
        duplicate.Name = $"{preset.Name} (副本)";
        duplicate.IsBuiltIn = false;

        _userPresets.Add(duplicate);
        SaveUserPreset(duplicate);
        RefreshAllPresets();

        return duplicate;
    }

    /// <summary>
    /// 按分类筛选预设
    /// </summary>
    /// <param name="category">分类名称，为空或"全部"返回所有</param>
    /// <returns>筛选后的预设列表</returns>
    public IEnumerable<ConfigPreset> GetPresetsByCategory(string category)
    {
        if (string.IsNullOrEmpty(category) || category == "全部")
            return AllPresets;

        return AllPresets.Where(p => p.Category == category);
    }

    /// <summary>
    /// 重新加载所有预设
    /// </summary>
    public void ReloadPresets()
    {
        LoadBuiltInPresets();
        LoadUserPresets();
        RefreshAllPresets();
    }
}
