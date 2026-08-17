using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;
using System.IO;
using System.Text.Json;

namespace HidConfigTool.App.Services;

/// <summary>
/// 本地云同步服务实现
/// 用本地文件模拟云端存储，用于测试和演示
/// 实际使用时可以替换为真实的云端服务
/// </summary>
public class LocalCloudSyncService : ICloudSyncService
{
    private readonly string _cloudStoragePath;
    private readonly string _localProfilesPath;
    private bool _isLoggedIn;
    private string? _userName;
    private bool _autoSyncEnabled;

    public bool IsLoggedIn => _isLoggedIn;
    public string? UserName => _userName;

    public bool AutoSyncEnabled
    {
        get => _autoSyncEnabled;
        set => _autoSyncEnabled = value;
    }

    public event EventHandler<SyncStatusEventArgs>? SyncStatusChanged;

    public LocalCloudSyncService()
    {
        // 云端存储路径（模拟）
        string appDataPath = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        _cloudStoragePath = Path.Combine(appDataPath, "HIDConfigTool", "CloudStorage");
        _localProfilesPath = Path.Combine(appDataPath, "HIDConfigTool", "Profiles");

        Directory.CreateDirectory(_cloudStoragePath);
    }

    /// <summary>
    /// 登录（模拟，任意用户名密码都可以登录）
    /// </summary>
    public Task<bool> LoginAsync(string username, string password)
    {
        // 模拟登录
        if (!string.IsNullOrEmpty(username) && !string.IsNullOrEmpty(password))
        {
            _isLoggedIn = true;
            _userName = username;

            // 创建用户目录
            string userPath = Path.Combine(_cloudStoragePath, username);
            Directory.CreateDirectory(userPath);

            return Task.FromResult(true);
        }

        return Task.FromResult(false);
    }

    /// <summary>
    /// 登出
    /// </summary>
    public Task LogoutAsync()
    {
        _isLoggedIn = false;
        _userName = null;
        return Task.CompletedTask;
    }

    /// <summary>
    /// 上传配置到云端
    /// </summary>
    public async Task<bool> UploadConfigAsync(string configName, DeviceConfig config)
    {
        try
        {
            if (!_isLoggedIn || string.IsNullOrEmpty(_userName))
                return false;

            OnSyncStatusChanged($"正在上传 {configName}...", 50, true);

            string userPath = Path.Combine(_cloudStoragePath, _userName);
            string configPath = Path.Combine(userPath, $"{configName}.json");

            var json = JsonSerializer.Serialize(config, new JsonSerializerOptions
            {
                WriteIndented = true
            });

            await File.WriteAllTextAsync(configPath, json);

            OnSyncStatusChanged("上传完成", 100, false);
            return true;
        }
        catch
        {
            OnSyncStatusChanged("上传失败", 0, false);
            return false;
        }
    }

    /// <summary>
    /// 从云端下载配置
    /// </summary>
    public async Task<DeviceConfig?> DownloadConfigAsync(string configName)
    {
        try
        {
            if (!_isLoggedIn || string.IsNullOrEmpty(_userName))
                return null;

            OnSyncStatusChanged($"正在下载 {configName}...", 50, true);

            string userPath = Path.Combine(_cloudStoragePath, _userName);
            string configPath = Path.Combine(userPath, $"{configName}.json");

            if (!File.Exists(configPath))
                return null;

            var json = await File.ReadAllTextAsync(configPath);
            var config = JsonSerializer.Deserialize<DeviceConfig>(json);

            OnSyncStatusChanged("下载完成", 100, false);
            return config;
        }
        catch
        {
            OnSyncStatusChanged("下载失败", 0, false);
            return null;
        }
    }

    /// <summary>
    /// 获取云端配置列表
    /// </summary>
    public Task<List<CloudConfigInfo>> GetConfigListAsync()
    {
        var list = new List<CloudConfigInfo>();

        try
        {
            if (!_isLoggedIn || string.IsNullOrEmpty(_userName))
                return Task.FromResult(list);

            string userPath = Path.Combine(_cloudStoragePath, _userName);

            if (!Directory.Exists(userPath))
                return Task.FromResult(list);

            var files = Directory.GetFiles(userPath, "*.json");

            foreach (var file in files)
            {
                var fileInfo = new FileInfo(file);
                list.Add(new CloudConfigInfo
                {
                    Name = Path.GetFileNameWithoutExtension(file),
                    LastModified = fileInfo.LastWriteTime,
                    Size = fileInfo.Length,
                    Version = "1.0"
                });
            }
        }
        catch
        {
            // 忽略错误
        }

        return Task.FromResult(list);
    }

    /// <summary>
    /// 删除云端配置
    /// </summary>
    public Task<bool> DeleteConfigAsync(string configName)
    {
        try
        {
            if (!_isLoggedIn || string.IsNullOrEmpty(_userName))
                return Task.FromResult(false);

            string userPath = Path.Combine(_cloudStoragePath, _userName);
            string configPath = Path.Combine(userPath, $"{configName}.json");

            if (File.Exists(configPath))
            {
                File.Delete(configPath);
                return Task.FromResult(true);
            }

            return Task.FromResult(false);
        }
        catch
        {
            return Task.FromResult(false);
        }
    }

    /// <summary>
    /// 同步所有配置（双向同步）
    /// </summary>
    public async Task<SyncResult> SyncAllAsync()
    {
        var result = new SyncResult();

        try
        {
            if (!_isLoggedIn)
            {
                result.Success = false;
                result.ErrorMessage = "未登录";
                return result;
            }

            OnSyncStatusChanged("正在同步...", 10, true);

            // 获取云端列表
            var cloudList = await GetConfigListAsync();

            // 获取本地列表
            var localList = new List<string>();
            if (Directory.Exists(_localProfilesPath))
            {
                var files = Directory.GetFiles(_localProfilesPath, "*.json");
                foreach (var file in files)
                {
                    localList.Add(Path.GetFileNameWithoutExtension(file));
                }
            }

            OnSyncStatusChanged("正在比较配置...", 30, true);

            // 上传本地有但云端没有的，或者本地更新的
            foreach (var localName in localList)
            {
                string localPath = Path.Combine(_localProfilesPath, $"{localName}.json");
                var localFileInfo = new FileInfo(localPath);

                var cloudConfig = cloudList.FirstOrDefault(c => c.Name == localName);
                if (cloudConfig == null || localFileInfo.LastWriteTime > cloudConfig.LastModified)
                {
                    // 上传
                    try
                    {
                        var json = await File.ReadAllTextAsync(localPath);
                        var config = JsonSerializer.Deserialize<DeviceConfig>(json);
                        if (config != null)
                        {
                            await UploadConfigAsync(localName, config);
                            result.UploadedCount++;
                        }
                        else
                        {
                            result.SkippedCount++;
                        }
                    }
                    catch
                    {
                        result.SkippedCount++;
                    }
                }
                else
                {
                    result.SkippedCount++;
                }
            }

            OnSyncStatusChanged("正在下载云端配置...", 70, true);

            // 下载云端有但本地没有的，或者云端更新的
            foreach (var cloudConfig in cloudList)
            {
                if (!localList.Contains(cloudConfig.Name))
                {
                    // 下载
                    try
                    {
                        var config = await DownloadConfigAsync(cloudConfig.Name);
                        if (config != null)
                        {
                            string localPath = Path.Combine(_localProfilesPath, $"{cloudConfig.Name}.json");
                            var json = JsonSerializer.Serialize(config, new JsonSerializerOptions { WriteIndented = true });
                            await File.WriteAllTextAsync(localPath, json);
                            result.DownloadedCount++;
                        }
                        else
                        {
                            result.SkippedCount++;
                        }
                    }
                    catch
                    {
                        result.SkippedCount++;
                    }
                }
            }

            result.Success = true;
            OnSyncStatusChanged("同步完成", 100, false);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            OnSyncStatusChanged("同步失败", 0, false);
        }

        return result;
    }

    private void OnSyncStatusChanged(string status, int progress, bool isSyncing)
    {
        SyncStatusChanged?.Invoke(this, new SyncStatusEventArgs
        {
            Status = status,
            Progress = progress,
            IsSyncing = isSyncing
        });
    }
}
