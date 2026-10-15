using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Serialization;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Core.Services;

/// <summary>
/// 固件在线更新服务实现
/// 从 GitHub Releases 检查最新固件版本并下载
/// 参考 Logitech G HUB / Razer Synapse 的在线升级体验
/// 
/// GitHub Releases API:
/// - 最新版本: https://api.github.com/repos/{owner}/{repo}/releases/latest
/// - 下载资产: release.assets[].browser_download_url
/// </summary>
public class FirmwareUpdateService : IFirmwareUpdateService
{
    /* ==================== 配置 ==================== */

    /// <summary>GitHub 仓库所有者</summary>
    private const string GitHubOwner = "JYW-zero";

    /// <summary>GitHub 仓库名</summary>
    private const string GitHubRepo = "pico_hid_composite";

    /// <summary>GitHub API 基础地址</summary>
    private const string GitHubApiBase = "https://api.github.com";

    /// <summary>HTTP 超时时间（秒）</summary>
    private const int HttpTimeoutSeconds = 30;

    /// <summary>下载缓冲区大小（8KB）</summary>
    private const int DownloadBufferSize = 8192;

    /* ==================== 私有字段 ==================== */

    private readonly HttpClient _httpClient;
    private readonly string _tempDirectory;
    private readonly List<string> _tempFiles = new();

    /* ==================== 构造函数 ==================== */

    public FirmwareUpdateService()
    {
        _httpClient = new HttpClient
        {
            Timeout = TimeSpan.FromSeconds(HttpTimeoutSeconds)
        };
        // GitHub API 要求 User-Agent 头
        _httpClient.DefaultRequestHeaders.UserAgent.ParseAdd("HidConfigTool/1.0");
        _httpClient.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github.v3+json");

        _tempDirectory = Path.Combine(Path.GetTempPath(), "HidConfigTool_Firmware");
        Directory.CreateDirectory(_tempDirectory);
    }

    /* ==================== 公共方法 ==================== */

    /// <inheritdoc/>
    public async Task<FirmwareUpdateInfo?> CheckForUpdateAsync(uint currentVersion, bool includePrerelease = false)
    {
        try
        {
            string apiUrl = $"{GitHubApiBase}/repos/{GitHubOwner}/{GitHubRepo}/releases/latest";
            var release = await _httpClient.GetFromJsonAsync<GitHubRelease>(apiUrl);

            if (release == null)
                return null;

            // 如果不包含预发布版本且当前是预发布，则返回 null
            if (!includePrerelease && release.Prerelease)
                return null;

            // 解析版本号（tag_name 格式如 "v4.1.0"）
            uint latestVersion = ParseVersion(release.TagName);
            if (latestVersion == 0)
                return null;

            // 版本比较：只有最新版本 > 当前版本才返回
            if (latestVersion <= currentVersion)
                return null;

            // 查找 .uf2 固件文件
            var uf2Asset = release.Assets?
                .FirstOrDefault(a => a.Name.EndsWith(".uf2", StringComparison.OrdinalIgnoreCase));

            if (uf2Asset == null)
                return null;

            return new FirmwareUpdateInfo
            {
                Version = latestVersion,
                VersionString = release.TagName,
                ReleaseDate = release.PublishedAt,
                DownloadUrl = uf2Asset.BrowserDownloadUrl,
                FileSize = uf2Asset.Size,
                ReleaseNotes = release.Body ?? string.Empty,
                IsPrerelease = release.Prerelease
            };
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"CheckForUpdateAsync failed: {ex.Message}");
            return null;
        }
    }

    /// <inheritdoc/>
    public async Task<string> DownloadFirmwareAsync(FirmwareUpdateInfo updateInfo, IProgress<int>? progress = null, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrEmpty(updateInfo.DownloadUrl))
            throw new ArgumentException("下载 URL 不能为空", nameof(updateInfo));

        string fileName = Path.GetFileName(new Uri(updateInfo.DownloadUrl).AbsolutePath);
        string filePath = Path.Combine(_tempDirectory, fileName);

        // 如果文件已存在且大小匹配，直接返回（缓存）
        if (File.Exists(filePath) && new FileInfo(filePath).Length == updateInfo.FileSize)
        {
            progress?.Report(100);
            return filePath;
        }

        using var response = await _httpClient.GetAsync(updateInfo.DownloadUrl, HttpCompletionOption.ResponseHeadersRead, cancellationToken);
        response.EnsureSuccessStatusCode();

        long totalBytes = response.Content.Headers.ContentLength ?? updateInfo.FileSize;
        long downloadedBytes = 0;

        await using var contentStream = await response.Content.ReadAsStreamAsync(cancellationToken);
        await using var fileStream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None);

        var buffer = new byte[DownloadBufferSize];
        int bytesRead;
        int lastProgress = -1;

        while ((bytesRead = await contentStream.ReadAsync(buffer, cancellationToken)) > 0)
        {
            await fileStream.WriteAsync(buffer.AsMemory(0, bytesRead), cancellationToken);
            downloadedBytes += bytesRead;

            if (totalBytes > 0)
            {
                int currentProgress = (int)((double)downloadedBytes / totalBytes * 100);
                if (currentProgress != lastProgress)
                {
                    progress?.Report(currentProgress);
                    lastProgress = currentProgress;
                }
            }
        }

        await fileStream.FlushAsync(cancellationToken);
        progress?.Report(100);

        _tempFiles.Add(filePath);
        return filePath;
    }

    /// <inheritdoc/>
    public void CleanupTempFiles()
    {
        foreach (var file in _tempFiles)
        {
            try
            {
                if (File.Exists(file))
                    File.Delete(file);
            }
            catch
            {
                // 忽略删除错误
            }
        }
        _tempFiles.Clear();
    }

    /* ==================== 私有方法 ==================== */

    /// <summary>
    /// 解析版本字符串为 uint 格式（0xMMmm）
    /// 支持格式："v4.1.0"、"4.1"、"v4.1"
    /// </summary>
    private static uint ParseVersion(string? versionString)
    {
        if (string.IsNullOrWhiteSpace(versionString))
            return 0;

        try
        {
            // 移除前缀 'v' 或 'V'
            string cleaned = versionString.TrimStart('v', 'V');

            // 按 '.' 分割
            var parts = cleaned.Split('.');
            if (parts.Length < 2)
                return 0;

            // 解析主版本和次版本
            if (!uint.TryParse(parts[0], out uint major))
                return 0;
            if (!uint.TryParse(parts[1], out uint minor))
                return 0;

            // 组合为 0xMMmm 格式（与固件版本格式一致）
            return (major << 8) | (minor & 0xFF);
        }
        catch
        {
            return 0;
        }
    }

    /* ==================== GitHub API 数据模型 ==================== */

    private class GitHubRelease
    {
        [JsonPropertyName("tag_name")]
        public string TagName { get; set; } = string.Empty;

        [JsonPropertyName("name")]
        public string Name { get; set; } = string.Empty;

        [JsonPropertyName("body")]
        public string? Body { get; set; }

        [JsonPropertyName("prerelease")]
        public bool Prerelease { get; set; }

        [JsonPropertyName("published_at")]
        public DateTime? PublishedAt { get; set; }

        [JsonPropertyName("assets")]
        public List<GitHubAsset>? Assets { get; set; }
    }

    private class GitHubAsset
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = string.Empty;

        [JsonPropertyName("size")]
        public long Size { get; set; }

        [JsonPropertyName("browser_download_url")]
        public string BrowserDownloadUrl { get; set; } = string.Empty;
    }
}
