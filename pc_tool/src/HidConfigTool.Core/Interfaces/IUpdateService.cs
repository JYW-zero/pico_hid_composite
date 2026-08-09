namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 自动更新服务接口
/// </summary>
public interface IUpdateService
{
    /// <summary>
    /// 当前版本
    /// </summary>
    Version CurrentVersion { get; }

    /// <summary>
    /// 最新版本
    /// </summary>
    Version? LatestVersion { get; }

    /// <summary>
    /// 是否有更新
    /// </summary>
    bool HasUpdate { get; }

    /// <summary>
    /// 更新信息
    /// </summary>
    UpdateInfo? UpdateInfo { get; }

    /// <summary>
    /// 检查更新
    /// </summary>
    Task<bool> CheckForUpdateAsync();

    /// <summary>
    /// 下载更新
    /// </summary>
    Task<bool> DownloadUpdateAsync(IProgress<DownloadProgress>? progress = null);

    /// <summary>
    /// 安装更新
    /// </summary>
    Task<bool> InstallUpdateAsync();

    /// <summary>
    /// 检查更新完成事件
    /// </summary>
    event EventHandler<UpdateCheckEventArgs>? UpdateCheckCompleted;

    /// <summary>
    /// 下载进度变化事件
    /// </summary>
    event EventHandler<DownloadProgress>? DownloadProgressChanged;
}

/// <summary>
/// 更新信息
/// </summary>
public class UpdateInfo
{
    public Version Version { get; set; } = new(0, 0, 0);
    public DateTime ReleaseDate { get; set; }
    public List<string> ReleaseNotes { get; set; } = new();
    public string DownloadUrl { get; set; } = string.Empty;
    public long FileSize { get; set; }
    public string Sha256 { get; set; } = string.Empty;
    public bool IsMandatory { get; set; }
}

/// <summary>
/// 下载进度
/// </summary>
public class DownloadProgress
{
    public int Percentage { get; set; }
    public long BytesDownloaded { get; set; }
    public long TotalBytes { get; set; }
    public double SpeedBytesPerSecond { get; set; }
}

/// <summary>
/// 更新检查事件参数
/// </summary>
public class UpdateCheckEventArgs : EventArgs
{
    public bool HasUpdate { get; set; }
    public UpdateInfo? UpdateInfo { get; set; }
    public string? ErrorMessage { get; set; }
}
