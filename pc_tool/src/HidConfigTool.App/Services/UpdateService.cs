using HidConfigTool.Core.Interfaces;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Reflection;
using System.Text.Json;

namespace HidConfigTool.App.Services;

/// <summary>
/// 自动更新服务实现
/// </summary>
public class UpdateService : IUpdateService
{
    private readonly HttpClient _httpClient;
    private string _updateUrl = "https://example.com/updates/version.json"; // 替换为实际的更新服务器地址

    public Version CurrentVersion { get; }
    public Version? LatestVersion { get; private set; }
    public bool HasUpdate { get; private set; }
    public UpdateInfo? UpdateInfo { get; private set; }

    public event EventHandler<UpdateCheckEventArgs>? UpdateCheckCompleted;
    public event EventHandler<DownloadProgress>? DownloadProgressChanged;

    public UpdateService()
    {
        _httpClient = new HttpClient();
        _httpClient.Timeout = TimeSpan.FromSeconds(30);

        // 获取当前程序集版本
        var assembly = Assembly.GetExecutingAssembly();
        var version = assembly.GetName().Version;
        CurrentVersion = version ?? new Version(1, 0, 0);
    }

    /// <summary>
    /// 检查更新
    /// </summary>
    public async Task<bool> CheckForUpdateAsync()
    {
        try
        {
            // 从服务器获取版本信息
            var json = await _httpClient.GetStringAsync(_updateUrl);
            var versionInfo = JsonSerializer.Deserialize<VersionInfo>(json);

            if (versionInfo == null || string.IsNullOrEmpty(versionInfo.Version))
            {
                OnUpdateCheckCompleted(false, null, "无法获取版本信息");
                return false;
            }

            LatestVersion = new Version(versionInfo.Version);
            HasUpdate = LatestVersion > CurrentVersion;

            if (HasUpdate)
            {
                UpdateInfo = new UpdateInfo
                {
                    Version = LatestVersion,
                    ReleaseDate = versionInfo.ReleaseDate,
                    ReleaseNotes = versionInfo.ReleaseNotes ?? new List<string>(),
                    DownloadUrl = versionInfo.DownloadUrl,
                    FileSize = versionInfo.FileSize,
                    Sha256 = versionInfo.Sha256,
                    IsMandatory = versionInfo.IsMandatory
                };
            }

            OnUpdateCheckCompleted(HasUpdate, UpdateInfo, null);
            return HasUpdate;
        }
        catch (Exception ex)
        {
            OnUpdateCheckCompleted(false, null, ex.Message);
            return false;
        }
    }

    /// <summary>
    /// 下载更新
    /// </summary>
    public async Task<bool> DownloadUpdateAsync(IProgress<DownloadProgress>? progress = null)
    {
        try
        {
            if (UpdateInfo == null || string.IsNullOrEmpty(UpdateInfo.DownloadUrl))
                return false;

            // 下载目录
            string downloadDir = Path.Combine(Path.GetTempPath(), "HIDConfigToolUpdates");
            Directory.CreateDirectory(downloadDir);

            string fileName = Path.GetFileName(UpdateInfo.DownloadUrl);
            string downloadPath = Path.Combine(downloadDir, fileName);

            // 下载文件
            using var response = await _httpClient.GetAsync(UpdateInfo.DownloadUrl, HttpCompletionOption.ResponseHeadersRead);
            response.EnsureSuccessStatusCode();

            long totalBytes = response.Content.Headers.ContentLength ?? 0;
            long bytesDownloaded = 0;
            var stopwatch = Stopwatch.StartNew();

            using var contentStream = await response.Content.ReadAsStreamAsync();
            using var fileStream = new FileStream(downloadPath, FileMode.Create, FileAccess.Write, FileShare.None);

            var buffer = new byte[8192];
            int bytesRead;

            while ((bytesRead = await contentStream.ReadAsync(buffer)) > 0)
            {
                await fileStream.WriteAsync(buffer.AsMemory(0, bytesRead));
                bytesDownloaded += bytesRead;

                // 报告进度
                if (totalBytes > 0)
                {
                    var downloadProgress = new DownloadProgress
                    {
                        Percentage = (int)(bytesDownloaded * 100 / totalBytes),
                        BytesDownloaded = bytesDownloaded,
                        TotalBytes = totalBytes,
                        SpeedBytesPerSecond = bytesDownloaded / stopwatch.Elapsed.TotalSeconds
                    };

                    progress?.Report(downloadProgress);
                    OnDownloadProgressChanged(downloadProgress);
                }
            }

            stopwatch.Stop();

            // 验证 SHA256（可选）
            if (!string.IsNullOrEmpty(UpdateInfo.Sha256))
            {
                // TODO: 验证文件哈希
            }

            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 安装更新
    /// </summary>
    public async Task<bool> InstallUpdateAsync()
    {
        try
        {
            // TODO: 实现更新安装逻辑
            // 1. 关闭当前程序
            // 2. 解压更新包
            // 3. 替换文件
            // 4. 重启程序

            await Task.Delay(100); // 模拟
            return true;
        }
        catch
        {
            return false;
        }
    }

    private void OnUpdateCheckCompleted(bool hasUpdate, UpdateInfo? updateInfo, string? errorMessage)
    {
        UpdateCheckCompleted?.Invoke(this, new UpdateCheckEventArgs
        {
            HasUpdate = hasUpdate,
            UpdateInfo = updateInfo,
            ErrorMessage = errorMessage
        });
    }

    private void OnDownloadProgressChanged(DownloadProgress progress)
    {
        DownloadProgressChanged?.Invoke(this, progress);
    }

    // 版本信息数据模型
    private class VersionInfo
    {
        public string Version { get; set; } = string.Empty;
        public DateTime ReleaseDate { get; set; }
        public List<string>? ReleaseNotes { get; set; }
        public string DownloadUrl { get; set; } = string.Empty;
        public long FileSize { get; set; }
        public string Sha256 { get; set; } = string.Empty;
        public bool IsMandatory { get; set; }
    }
}
