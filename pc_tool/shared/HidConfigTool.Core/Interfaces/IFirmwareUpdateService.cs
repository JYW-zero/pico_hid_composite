namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 固件更新信息
/// </summary>
public class FirmwareUpdateInfo
{
    /// <summary>版本号（如 0x0401 = v4.1）</summary>
    public uint Version { get; set; }

    /// <summary>版本字符串（如 "v4.1.0"）</summary>
    public string VersionString { get; set; } = string.Empty;

    /// <summary>发布日期</summary>
    public DateTime? ReleaseDate { get; set; }

    /// <summary>下载 URL（.uf2 文件）</summary>
    public string DownloadUrl { get; set; } = string.Empty;

    /// <summary>文件大小（字节）</summary>
    public long FileSize { get; set; }

    /// <summary>更新内容（Markdown 格式）</summary>
    public string ReleaseNotes { get; set; } = string.Empty;

    /// <summary>是否为预发布版本</summary>
    public bool IsPrerelease { get; set; }
}

/// <summary>
/// 固件在线更新服务接口
/// 从 GitHub Releases 检查最新固件版本并下载
/// 参考 Logitech G HUB / Razer Synapse 的在线升级体验
/// </summary>
public interface IFirmwareUpdateService
{
    /// <summary>
    /// 检查是否有可用的固件更新
    /// </summary>
    /// <param name="currentVersion">当前设备固件版本（如 0x0400）</param>
    /// <param name="includePrerelease">是否包含预发布版本</param>
    /// <returns>最新版本信息，如果没有更新则返回 null</returns>
    Task<FirmwareUpdateInfo?> CheckForUpdateAsync(uint currentVersion, bool includePrerelease = false);

    /// <summary>
    /// 下载固件文件到临时目录
    /// </summary>
    /// <param name="updateInfo">更新信息（包含下载 URL）</param>
    /// <param name="progress">下载进度回调（0-100）</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>下载后的本地文件路径</returns>
    Task<string> DownloadFirmwareAsync(FirmwareUpdateInfo updateInfo, IProgress<int>? progress = null, CancellationToken cancellationToken = default);

    /// <summary>
    /// 清理临时下载的固件文件
    /// </summary>
    void CleanupTempFiles();
}
