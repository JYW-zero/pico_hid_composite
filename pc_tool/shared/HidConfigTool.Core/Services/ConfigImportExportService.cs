using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.Json.Serialization.Metadata;
using HidConfigTool.Core.Models;
using HidConfigTool.Core.Utils;

namespace HidConfigTool.Core.Services;

/// <summary>
/// 配置导入/导出服务
/// 支持 JSON 和二进制格式，导入前进行版本校验和字段合法性校验
/// </summary>
public class ConfigImportExportService
{
    /// <summary>
    /// 当前支持的最高配置版本（统一从 ProtocolConstants.ConfigVersion 引用）
    /// </summary>
    public const int MaxSupportedConfigVersion = ProtocolConstants.ConfigVersion;

    /// <summary>
    /// 最低支持的配置版本
    /// </summary>
    public const int MinSupportedConfigVersion = 1;

    /// <summary>
    /// 配置文件魔数（二进制格式，统一从 ProtocolConstants.ConfigMagic 引用）
    /// </summary>
    private const uint ConfigMagic = ProtocolConstants.ConfigMagic;

    /// <summary>
    /// JSON 序列化安全选项
    /// </summary>
    private static readonly JsonSerializerOptions _jsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        TypeInfoResolver = new DefaultJsonTypeInfoResolver()
    };

    /// <summary>
    /// 导出配置为 JSON 文件
    /// </summary>
    /// <param name="config">设备配置</param>
    /// <param name="filePath">目标文件路径</param>
    /// <returns>是否成功</returns>
    public async Task<bool> ExportToJsonAsync(DeviceConfig config, string filePath)
    {
        try
        {
            if (config == null)
                return false;

            // 确保目录存在
            string? dir = Path.GetDirectoryName(filePath);
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }

            // 序列化配置
            string json = JsonSerializer.Serialize(config, _jsonOptions);

            // 写入文件
            await File.WriteAllTextAsync(filePath, json);

            return true;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"导出 JSON 配置失败: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 导出配置为二进制文件
    /// </summary>
    /// <param name="configData">配置二进制数据</param>
    /// <param name="filePath">目标文件路径</param>
    /// <returns>是否成功</returns>
    public async Task<bool> ExportToBinaryAsync(byte[] configData, string filePath)
    {
        try
        {
            if (configData == null || configData.Length == 0)
                return false;

            // 确保目录存在
            string? dir = Path.GetDirectoryName(filePath);
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }

            // 写入文件
            await File.WriteAllBytesAsync(filePath, configData);

            return true;
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"导出二进制配置失败: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 从 JSON 文件导入配置
    /// </summary>
    /// <param name="filePath">文件路径</param>
    /// <returns>导入结果（包含配置和校验信息）</returns>
    public async Task<ConfigImportResult?> ImportFromJsonAsync(string filePath)
    {
        try
        {
            if (!File.Exists(filePath))
                return new ConfigImportResult { Success = false, ErrorMessage = "文件不存在" };

            // 验证文件大小（最大 1MB）
            var fileInfo = new FileInfo(filePath);
            if (fileInfo.Length > 1024 * 1024)
                return new ConfigImportResult { Success = false, ErrorMessage = "文件过大（超过 1MB）" };

            // 读取文件
            string json = await File.ReadAllTextAsync(filePath);

            // 反序列化
            var config = JsonSerializer.Deserialize<DeviceConfig>(json, _jsonOptions);
            if (config == null)
                return new ConfigImportResult { Success = false, ErrorMessage = "JSON 解析失败" };

            // 校验配置
            var validation = ValidateConfig(config);
            if (!validation.IsValid)
                return new ConfigImportResult { Success = false, ErrorMessage = validation.ErrorMessage, Config = config };

            return new ConfigImportResult
            {
                Success = true,
                Config = config,
                Version = config.Version,
                HasVersionWarning = config.Version > MaxSupportedConfigVersion
            };
        }
        catch (JsonException ex)
        {
            return new ConfigImportResult { Success = false, ErrorMessage = $"JSON 格式错误: {ex.Message}" };
        }
        catch (Exception ex)
        {
            return new ConfigImportResult { Success = false, ErrorMessage = $"导入失败: {ex.Message}" };
        }
    }

    /// <summary>
    /// 从二进制文件导入配置
    /// </summary>
    /// <param name="filePath">文件路径</param>
    /// <param name="expectedSize">期望的配置大小</param>
    /// <returns>配置二进制数据</returns>
    public async Task<byte[]?> ImportFromBinaryAsync(string filePath, int expectedSize)
    {
        try
        {
            if (!File.Exists(filePath))
                return null;

            // 验证文件大小
            var fileInfo = new FileInfo(filePath);
            if (fileInfo.Length > 1024 * 1024)
                return null;

            // 读取文件
            byte[] data = await File.ReadAllBytesAsync(filePath);

            // 验证大小
            if (data.Length < expectedSize)
                return null;

            return data;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// 校验配置合法性
    /// </summary>
    /// <param name="config">设备配置</param>
    /// <returns>校验结果</returns>
    public ConfigValidationResult ValidateConfig(DeviceConfig config)
    {
        if (config == null)
            return new ConfigValidationResult { IsValid = false, ErrorMessage = "配置为空" };

        // 校验版本
        if (config.Version < MinSupportedConfigVersion)
            return new ConfigValidationResult { IsValid = false, ErrorMessage = $"配置版本过低（v{config.Version}），最低支持 v{MinSupportedConfigVersion}" };

        // 校验 DPI
        if (config.Dpi < 100 || config.Dpi > 6400)
            return new ConfigValidationResult { IsValid = false, ErrorMessage = $"DPI 值无效（{config.Dpi}），范围 100-6400" };

        // 校验摇杆死区
        if (config.JoystickDeadzone > 4095)
            return new ConfigValidationResult { IsValid = false, ErrorMessage = $"摇杆死区值无效（{config.JoystickDeadzone}），范围 0-4095" };

        // 校验摇杆灵敏度
        if (config.JoystickSensitivity < 0.1 || config.JoystickSensitivity > 10.0)
            return new ConfigValidationResult { IsValid = false, ErrorMessage = $"摇杆灵敏度无效（{config.JoystickSensitivity}），范围 0.1-10.0" };

        // 校验摇杆中心点
        if (config.JoystickCenterX > 4095 || config.JoystickCenterY > 4095)
            return new ConfigValidationResult { IsValid = false, ErrorMessage = "摇杆中心点值无效，范围 0-4095" };

        // 校验 DPI 档位
        if (config.DpiLevels != null)
        {
            foreach (int dpi in config.DpiLevels)
            {
                if (dpi < 100 || dpi > 6400)
                    return new ConfigValidationResult { IsValid = false, ErrorMessage = $"DPI 档位值无效（{dpi}），范围 100-6400" };
            }
        }

        // 校验按键映射数组长度
        if (config.Keymap != null && config.Keymap.Length != 64)
            return new ConfigValidationResult { IsValid = false, ErrorMessage = $"按键映射数组长度无效（{config.Keymap.Length}），应为 64" };

        if (config.FnKeymap != null && config.FnKeymap.Length != 64)
            return new ConfigValidationResult { IsValid = false, ErrorMessage = $"Fn 按键映射数组长度无效（{config.FnKeymap.Length}），应为 64" };

        return new ConfigValidationResult { IsValid = true };
    }

    /// <summary>
    /// 判断文件是否为 JSON 格式
    /// </summary>
    /// <param name="filePath">文件路径</param>
    /// <returns>是否为 JSON 格式</returns>
    public bool IsJsonFile(string filePath)
    {
        string ext = Path.GetExtension(filePath).ToLowerInvariant();
        return ext == ".json";
    }

    /// <summary>
    /// 判断文件是否为二进制格式
    /// </summary>
    /// <param name="filePath">文件路径</param>
    /// <returns>是否为二进制格式</returns>
    public bool IsBinaryFile(string filePath)
    {
        string ext = Path.GetExtension(filePath).ToLowerInvariant();
        return ext == ".bin" || ext == ".hidcfg";
    }

    /// <summary>
    /// 获取支持的文件扩展名过滤器（用于文件对话框）
    /// </summary>
    /// <returns>文件过滤器字符串</returns>
    public static string GetFileFilter()
    {
        return "配置文件|*.json;*.bin;*.hidcfg|JSON 配置文件|*.json|二进制配置文件|*.bin;*.hidcfg|所有文件|*.*";
    }
}

/// <summary>
/// 配置导入结果
/// </summary>
public class ConfigImportResult
{
    /// <summary>
    /// 是否成功
    /// </summary>
    public bool Success { get; set; }

    /// <summary>
    /// 错误消息
    /// </summary>
    public string? ErrorMessage { get; set; }

    /// <summary>
    /// 导入的配置
    /// </summary>
    public DeviceConfig? Config { get; set; }

    /// <summary>
    /// 配置版本
    /// </summary>
    public int Version { get; set; }

    /// <summary>
    /// 是否有版本警告（版本高于当前支持版本）
    /// </summary>
    public bool HasVersionWarning { get; set; }
}

/// <summary>
/// 配置校验结果
/// </summary>
public class ConfigValidationResult
{
    /// <summary>
    /// 是否有效
    /// </summary>
    public bool IsValid { get; set; }

    /// <summary>
    /// 错误消息
    /// </summary>
    public string? ErrorMessage { get; set; }
}
