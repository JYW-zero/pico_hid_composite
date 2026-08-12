using HidConfigTool.Core.Interfaces;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;

namespace HidConfigTool.App.Services;

/// <summary>
/// 自动更新服务实现
/// </summary>
public class UpdateService : IUpdateService, IDisposable
{
    private readonly HttpClient _httpClient;
    private bool _disposed;
    private string _updateUrl = "https://example.com/updates/version.json"; // 替换为实际的更新服务器地址
    private string? _lastDownloadPath;

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

            // 验证 SHA256
            if (!string.IsNullOrEmpty(UpdateInfo.Sha256))
            {
                string actualHash = await ComputeSha256Async(downloadPath);
                if (!string.Equals(actualHash, UpdateInfo.Sha256, StringComparison.OrdinalIgnoreCase))
                {
                    // 哈希不匹配，删除下载的文件
                    File.Delete(downloadPath);
                    return false;
                }
            }

            // 保存下载路径供安装使用
            _lastDownloadPath = downloadPath;

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
            if (string.IsNullOrEmpty(_lastDownloadPath) || !File.Exists(_lastDownloadPath))
                return false;

            string extension = Path.GetExtension(_lastDownloadPath).ToLowerInvariant();

            // .exe 安装包：直接运行
            if (extension == ".exe")
            {
                var startInfo = new ProcessStartInfo
                {
                    FileName = _lastDownloadPath,
                    UseShellExecute = true,
                    Verb = "runas" // 请求管理员权限
                };
                Process.Start(startInfo);
                // 安装程序会自动关闭当前程序，这里直接返回
                return true;
            }

            // .zip 压缩包：解压并替换文件（需要更新器程序协助）
            if (extension == ".zip")
            {
                // 创建批处理更新脚本
                string batchPath = Path.Combine(Path.GetTempPath(), "HIDConfigToolUpdate.bat");
                string appDir = AppContext.BaseDirectory;
                string tempExtractDir = Path.Combine(Path.GetTempPath(), "HIDConfigToolUpdateExtract");

                // 生成批处理脚本：等待程序退出 -> 解压 -> 复制文件 -> 重启程序
                string batchContent = $@"
@echo off
echo 正在更新 HID Config Tool...
timeout /t 2 /nobreak >nul
if exist ""{tempExtractDir}"" rmdir /s /q ""{tempExtractDir}""
powershell -Command ""Expand-Archive -Path '{_lastDownloadPath}' -DestinationPath '{tempExtractDir}' -Force""
xcopy /e /y /q ""{tempExtractDir}\*"" ""{appDir}""
rmdir /s /q ""{tempExtractDir}""
del ""{_lastDownloadPath}""
del ""%~f0""
start """" ""{Path.Combine(appDir, "HidConfigTool.App.exe")}""
";
                File.WriteAllText(batchPath, batchContent);

                // 启动批处理脚本（隐藏窗口）
                var startInfo = new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = $"/c \"{batchPath}\"",
                    UseShellExecute = false,
                    CreateNoWindow = true
                };
                Process.Start(startInfo);

                // 退出当前程序
                Environment.Exit(0);
                return true; // 不会执行到这里
            }

            // 其他格式：不支持
            return false;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 计算文件的 SHA256 哈希
    /// </summary>
    private static async Task<string> ComputeSha256Async(string filePath)
    {
        using var sha256 = SHA256.Create();
        using var stream = File.OpenRead(filePath);
        byte[] hash = await sha256.ComputeHashAsync(stream);
        return Convert.ToHexString(hash);
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

    public void Dispose()
    {
        if (!_disposed)
        {
            _httpClient.Dispose();
            _disposed = true;
        }
    }
}
