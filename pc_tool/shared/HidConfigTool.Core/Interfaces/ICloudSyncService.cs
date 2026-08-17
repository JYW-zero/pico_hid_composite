using HidConfigTool.Core.Models;

namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 云同步服务接口
/// </summary>
public interface ICloudSyncService
{
    /// <summary>
    /// 是否已登录
    /// </summary>
    bool IsLoggedIn { get; }

    /// <summary>
    /// 用户名
    /// </summary>
    string? UserName { get; }

    /// <summary>
    /// 自动同步是否开启
    /// </summary>
    bool AutoSyncEnabled { get; set; }

    /// <summary>
    /// 登录
    /// </summary>
    Task<bool> LoginAsync(string username, string password);

    /// <summary>
    /// 登出
    /// </summary>
    Task LogoutAsync();

    /// <summary>
    /// 上传配置到云端
    /// </summary>
    Task<bool> UploadConfigAsync(string configName, DeviceConfig config);

    /// <summary>
    /// 从云端下载配置
    /// </summary>
    Task<DeviceConfig?> DownloadConfigAsync(string configName);

    /// <summary>
    /// 获取云端配置列表
    /// </summary>
    Task<List<CloudConfigInfo>> GetConfigListAsync();

    /// <summary>
    /// 删除云端配置
    /// </summary>
    Task<bool> DeleteConfigAsync(string configName);

    /// <summary>
    /// 同步所有配置（双向同步）
    /// </summary>
    Task<SyncResult> SyncAllAsync();

    /// <summary>
    /// 同步状态变化事件
    /// </summary>
    event EventHandler<SyncStatusEventArgs>? SyncStatusChanged;
}

/// <summary>
/// 云端配置信息
/// </summary>
public class CloudConfigInfo
{
    public string Name { get; set; } = string.Empty;
    public DateTime LastModified { get; set; }
    public long Size { get; set; }
    public string Version { get; set; } = string.Empty;
}

/// <summary>
/// 同步结果
/// </summary>
public class SyncResult
{
    public bool Success { get; set; }
    public int UploadedCount { get; set; }
    public int DownloadedCount { get; set; }
    public int SkippedCount { get; set; }
    public string? ErrorMessage { get; set; }
}

/// <summary>
/// 同步状态事件参数
/// </summary>
public class SyncStatusEventArgs : EventArgs
{
    public string Status { get; set; } = string.Empty;
    public int Progress { get; set; }
    public bool IsSyncing { get; set; }
}
