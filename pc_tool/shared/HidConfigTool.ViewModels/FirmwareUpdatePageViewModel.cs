using System.Collections.ObjectModel;
using System.IO;
using System.Diagnostics;
using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Utils;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 固件升级页面 ViewModel
/// </summary>
public partial class FirmwareUpdatePageViewModel : ObservableObject, IDisposable
{
    private readonly IDeviceService _deviceService;
    private readonly IFileDialogService _fileDialogService;
    private readonly IDialogService _dialogService;
    private readonly IFirmwareUpdateService _firmwareUpdateService;
    private CancellationTokenSource? _cts;
    private FirmwareUpdateInfo? _updateInfo;

    [ObservableProperty]
    private string? _firmwarePath;

    [ObservableProperty]
    private int _progress;

    [ObservableProperty]
    private string _status = "就绪";

    [ObservableProperty]
    private bool _isUpdating;

    [ObservableProperty]
    private bool _isDeviceConnected;

    [ObservableProperty]
    private bool _isEnteringBootsel;

    /// <summary>设备是否编入自定义 in-app DFU bootloader（能力位 bit10 / CAP_DFU_UPGRADE）。
    /// false（当前固件未编入 bootloader）时隐藏“一键/在线 DFU”，仅保留芯片原生
    /// BOOTSEL UF2 手动升级，避免消费者点击必失败的在线升级。 */
    [ObservableProperty]
    private bool _isDfuUpgradeAvailable;

    [ObservableProperty]
    private string? _currentFirmwareVersion;

    /* ==================== 在线升级属性 ==================== */

    /// <summary>是否正在检查更新</summary>
    [ObservableProperty]
    private bool _isCheckingUpdate;

    /// <summary>是否正在下载固件</summary>
    [ObservableProperty]
    private bool _isDownloading;

    /// <summary>下载进度（0-100）</summary>
    [ObservableProperty]
    private int _downloadProgress;

    /// <summary>是否有可用更新</summary>
    [ObservableProperty]
    private bool _isUpdateAvailable;

    /// <summary>最新版本字符串（如 "v4.1.0"）</summary>
    [ObservableProperty]
    private string? _latestVersion;

    /// <summary>更新内容</summary>
    [ObservableProperty]
    private string? _releaseNotes;

    /// <summary>检查更新失败的错误信息（null 表示成功）</summary>
    [ObservableProperty]
    private string? _checkUpdateError;

    public ObservableCollection<string> LogMessages { get; } = new();

    public ICommand SelectFileCommand { get; }
    public ICommand StartUpdateCommand { get; }
    public ICommand CancelCommand { get; }
    public ICommand OpenBootselDriveCommand { get; }
    public ICommand CheckForUpdateCommand { get; }
    public ICommand OnlineUpdateCommand { get; }

    public FirmwareUpdatePageViewModel(IDeviceService deviceService, IFileDialogService fileDialogService, IDialogService dialogService, IFirmwareUpdateService firmwareUpdateService)
    {
        _deviceService = deviceService;
        _fileDialogService = fileDialogService;
        _dialogService = dialogService;
        _firmwareUpdateService = firmwareUpdateService;

        SelectFileCommand = new RelayCommand(SelectFile);
        StartUpdateCommand = new RelayCommand(StartUpdate, CanStartUpdate);
        CancelCommand = new RelayCommand(CancelUpdate);
        OpenBootselDriveCommand = new RelayCommand(OpenBootselDrive);
        CheckForUpdateCommand = new RelayCommand(CheckForUpdate);
        OnlineUpdateCommand = new RelayCommand(OnlineUpdate, CanOnlineUpdate);

        _deviceService.DeviceConnectionChanged += OnDeviceConnectionChanged;
        _deviceService.DfuStatusChanged += OnDfuStatusChanged;
        _deviceService.VersionCheckCompleted += OnVersionCheckCompleted;
        IsDeviceConnected = _deviceService.IsConnected;
        CurrentFirmwareVersion = _deviceService.FirmwareVersion;
        RefreshDfuAvailability();

        // 页面加载时自动检查更新（如果设备已连接）
        if (IsDeviceConnected)
        {
            _ = CheckForUpdateAsync();
        }
    }

    private bool CanStartUpdate()
    {
        return IsDfuUpgradeAvailable && !IsUpdating && IsDeviceConnected
            && !string.IsNullOrEmpty(FirmwarePath) && File.Exists(FirmwarePath);
    }

    /// <summary>
    /// 判断是否可以在线升级
    /// 条件：设备编入 DFU bootloader、已连接、有可用更新、未在升级/下载中
    /// </summary>
    private bool CanOnlineUpdate()
    {
        return IsDfuUpgradeAvailable && IsDeviceConnected && IsUpdateAvailable && !IsUpdating && !IsDownloading;
    }

    /// <summary>
    /// 通知所有命令的可执行状态变化
    /// </summary>
    partial void OnIsUpdateAvailableChanged(bool value) => UpdateCommands();
    partial void OnIsUpdatingChanged(bool value) => UpdateCommands();
    partial void OnIsDownloadingChanged(bool value) => UpdateCommands();
    partial void OnIsDfuUpgradeAvailableChanged(bool value) => UpdateCommands();
    partial void OnIsDeviceConnectedChanged(bool value)
    {
        UpdateCommands();
        // 设备连接后自动检查更新
        if (value && !IsCheckingUpdate && !IsUpdateAvailable)
        {
            _ = CheckForUpdateAsync();
        }
    }

    private void UpdateCommands()
    {
        (StartUpdateCommand as IRelayCommand)?.NotifyCanExecuteChanged();
        (OnlineUpdateCommand as IRelayCommand)?.NotifyCanExecuteChanged();
    }

    /* ==================== 在线升级 ==================== */

    /// <summary>
    /// 检查更新（命令入口）
    /// </summary>
    private async void CheckForUpdate()
    {
        await CheckForUpdateAsync();
    }

    /// <summary>
    /// 异步检查固件更新
    /// 从 GitHub Releases 获取最新版本，与当前版本比较
    /// </summary>
    public async Task CheckForUpdateAsync()
    {
        if (IsCheckingUpdate || !IsDeviceConnected || !IsDfuUpgradeAvailable)
            return;

        try
        {
            IsCheckingUpdate = true;
            CheckUpdateError = null;
            IsUpdateAvailable = false;
            Status = "正在检查更新...";
            AddLog("正在检查固件更新...");

            // 解析当前固件版本（格式如 "v4.0.0" 或 "4.0.0"）
            uint currentVersion = ParseFirmwareVersion(CurrentFirmwareVersion);
            AddLog($"当前固件版本: {CurrentFirmwareVersion} (0x{currentVersion:X4})");

            // 从 GitHub 检查最新版本
            var updateInfo = await _firmwareUpdateService.CheckForUpdateAsync(currentVersion);

            if (updateInfo == null)
            {
                IsUpdateAvailable = false;
                LatestVersion = null;
                ReleaseNotes = null;
                Status = "已是最新版本";
                AddLog("✅ 已是最新版本，无需更新");
            }
            else
            {
                _updateInfo = updateInfo;
                IsUpdateAvailable = true;
                LatestVersion = updateInfo.VersionString;
                ReleaseNotes = updateInfo.ReleaseNotes;
                Status = $"发现新版本: {updateInfo.VersionString}";
                AddLog($"🔔 发现新版本: {updateInfo.VersionString} (发布于 {updateInfo.ReleaseDate:yyyy-MM-dd})");
                AddLog($"文件大小: {updateInfo.FileSize / 1024} KB");
            }
        }
        catch (Exception ex)
        {
            CheckUpdateError = ex.Message;
            Status = "检查更新失败";
            AddLog($"❌ 检查更新失败: {ex.Message}");
            AddLog("提示: 请检查网络连接，或使用本地固件文件升级");
        }
        finally
        {
            IsCheckingUpdate = false;
        }
    }

    /// <summary>
    /// 在线升级（命令入口）
    /// 流程：确认 → 下载固件 → DFU 升级 → 重启
    /// 用户只需点击一次，其余全自动
    /// </summary>
    private async void OnlineUpdate()
    {
        await OnlineUpdateAsync();
    }

    /// <summary>
    /// 异步在线升级
    /// 参考 Logitech G HUB / Razer Synapse 的一键升级体验
    /// </summary>
    public async Task OnlineUpdateAsync()
    {
        if (_updateInfo == null || !CanOnlineUpdate())
            return;

        // UX层面确认（参考 Razer Synapse）
        bool confirmed = _dialogService.ShowConfirm(
            $"确定要升级固件吗？\n\n" +
            $"当前版本: {CurrentFirmwareVersion}\n" +
            $"最新版本: {_updateInfo.VersionString}\n\n" +
            $"升级过程中请勿断开设备连接！\n" +
            $"升级完成后设备将自动重启。",
            "固件升级确认");
        if (!confirmed)
            return;

        try
        {
            IsUpdating = true;
            _cts = new CancellationTokenSource();
            Progress = 0;

            // 步骤1：下载固件
            AddLog($"正在下载固件 {_updateInfo.VersionString}...");
            Status = "正在下载固件...";
            IsDownloading = true;

            var downloadProgress = new Progress<int>(p =>
            {
                DownloadProgress = p;
                // 下载阶段占总进度的 0-30%
                Progress = (int)(p * 0.3);
            });

            string firmwarePath = await _firmwareUpdateService.DownloadFirmwareAsync(
                _updateInfo, downloadProgress, _cts.Token);

            IsDownloading = false;
            AddLog($"✅ 固件下载完成: {Path.GetFileName(firmwarePath)}");
            FirmwarePath = firmwarePath;

            // 步骤2：DFU 升级（占总进度的 30-100%）
            AddLog("正在进入 DFU 模式并升级...");
            Status = "正在升级固件...";

            var upgradeProgress = new Progress<int>(p =>
            {
                // DFU 阶段占总进度的 30-100%
                Progress = 30 + (int)(p * 0.7);
            });

            bool success = await _deviceService.DfuUpgradeAsync(firmwarePath, upgradeProgress, _cts.Token);

            if (success)
            {
                Progress = 100;
                Status = "✅ 升级完成，设备正在重启...";
                AddLog("✅ 固件升级完成！设备正在重启...");
                _dialogService.ShowInfo(
                    $"固件升级完成！\n\n版本: {CurrentFirmwareVersion} → {_updateInfo.VersionString}\n设备正在重启，请稍候...",
                    "升级成功");

                // 升级完成后清除更新状态
                IsUpdateAvailable = false;
                _updateInfo = null;
            }
            else
            {
                Status = "❌ 升级失败";
                AddLog("❌ 固件升级失败，请查看日志了解详情");
                _dialogService.ShowError(
                    "固件升级失败！\n\n建议:\n1. 检查设备连接\n2. 重试升级\n3. 如持续失败，使用 BOOTSEL UF2 模式手动升级",
                    "升级失败");
            }
        }
        catch (OperationCanceledException)
        {
            Status = "升级已取消";
            AddLog("⚠️ 升级已取消");
        }
        catch (Exception ex)
        {
            Status = "❌ 升级异常";
            AddLog($"❌ 升级异常: {ex.Message}");
            _dialogService.ShowError($"升级过程中发生异常: {ex.Message}", "升级异常");
        }
        finally
        {
            IsUpdating = false;
            IsDownloading = false;
            _cts = null;
        }
    }

    /// <summary>
    /// 解析固件版本字符串为 uint（0xMMmm 格式）
    /// </summary>
    private static uint ParseFirmwareVersion(string? versionString)
    {
        if (string.IsNullOrWhiteSpace(versionString))
            return 0;

        try
        {
            string cleaned = versionString.TrimStart('v', 'V');
            var parts = cleaned.Split('.');
            if (parts.Length < 2)
                return 0;

            if (!uint.TryParse(parts[0], out uint major))
                return 0;
            if (!uint.TryParse(parts[1], out uint minor))
                return 0;

            return (major << 8) | (minor & 0xFF);
        }
        catch
        {
            return 0;
        }
    }

    /// <summary>
    /// 进入官方 BOOTSEL UF2 模式并打开盘符
    /// 流程：1. 发送 CMD_ENTER_BOOTSEL 命令让设备进入 BOOTSEL 模式（软件模拟按住 BOOTSEL 键上电）
    ///       2. 等待设备枚举为 RPI-RP2 U 盘（最多等待10秒）
    ///       3. 自动用资源管理器打开盘符，方便用户拖拽 .uf2 文件
    /// </summary>
    private async void OpenBootselDrive()
    {
        if (IsEnteringBootsel || IsUpdating)
            return;

        try
        {
            IsEnteringBootsel = true;

            // UX层面二次确认（参考 Razer Synapse / Logitech G HUB 成熟方案）
            // 协议层面仍保留3次命令确认（防止恶意软件），UX层面再确认一次（防止用户误触）
            if (IsDeviceConnected)
            {
                bool confirmed = _dialogService.ShowConfirm(
                    "确定要让设备进入 BOOTSEL UF2 模式吗？\n\n" +
                    "• 设备将自动重启并进入 BOOTSEL 模式\n" +
                    "• 电脑会出现一个名为 RPI-RP2 的 U 盘\n" +
                    "• 将 .uf2 固件文件拖入 U 盘即可完成升级\n\n" +
                    "注意：升级过程中请勿断开设备连接！",
                    "进入 BOOTSEL UF2 模式");
                if (!confirmed)
                {
                    IsEnteringBootsel = false;
                    return;
                }
            }

            // 步骤1：如果设备已连接，先发送命令让设备进入 BOOTSEL 模式
            if (IsDeviceConnected)
            {
                AddLog("正在发送命令，让设备进入 BOOTSEL UF2 模式...");
                Status = "正在进入 BOOTSEL 模式...";

                bool cmdSent = await _deviceService.EnterBootselModeAsync();
                if (!cmdSent)
                {
                    AddLog("进入 BOOTSEL 模式命令发送失败");
                    Status = "进入 BOOTSEL 模式失败";
                    _dialogService.ShowError("进入 BOOTSEL 模式命令发送失败，请检查设备连接", "错误");
                    return;
                }

                AddLog("命令已发送，设备正在重启进入 BOOTSEL 模式...");
                Status = "等待 U 盘出现...";
            }
            else
            {
                AddLog("设备未连接，直接检测是否已有 BOOTSEL U 盘...");
            }

            // 步骤2：等待 RPI-RP2 U 盘出现（最多等待10秒，每秒检查一次）
            const int maxWaitSeconds = 10;
            for (int i = 0; i < maxWaitSeconds; i++)
            {
                await Task.Delay(1000);

                string? drivePath = FindBootselDrive();
                if (drivePath != null)
                {
                    // 步骤3：找到 BOOTSEL U 盘，用资源管理器打开
                    System.Diagnostics.Process.Start("explorer.exe", drivePath);
                    AddLog($"检测到 BOOTSEL U 盘，已打开盘符: {drivePath}");
                    Status = "请将 .uf2 固件文件拖入 U 盘";
                    return;
                }

                AddLog($"等待 U 盘出现... ({i + 1}/{maxWaitSeconds}秒)");
            }

            // 超时未检测到 BOOTSEL U 盘
            AddLog("超时：未检测到 BOOTSEL U 盘 (RPI-RP2)");
            Status = "未检测到 BOOTSEL U 盘";

            _dialogService.ShowInfo(
                "未检测到 BOOTSEL U 盘。\n\n请按以下步骤手动操作：\n1. 按住板子上的 BOOTSEL 键\n2. 按复位键上电\n3. 电脑会出现名为 RPI-RP2 的 U 盘\n4. 再次点击此按钮打开盘符",
                "BOOTSEL UF2 升级");
        }
        catch (TaskCanceledException)
        {
            AddLog("操作已取消");
            Status = "已取消";
        }
        catch (Exception ex)
        {
            AddLog($"进入 BOOTSEL 模式失败: {ex.Message}");
            Status = "进入 BOOTSEL 模式失败";
        }
        finally
        {
            IsEnteringBootsel = false;
        }
    }

    /// <summary>
    /// 查找卷标为 "RPI-RP2" 的可移动磁盘（RP2350 BOOTSEL 模式的 U 盘）
    /// </summary>
    /// <returns>U 盘根目录路径，未找到返回 null</returns>
    private static string? FindBootselDrive()
    {
        try
        {
            var drives = DriveInfo.GetDrives()
                .Where(d => d.DriveType == DriveType.Removable)
                .ToList();

            foreach (var drive in drives)
            {
                try
                {
                    if (drive.IsReady &&
                        !string.IsNullOrEmpty(drive.VolumeLabel) &&
                        drive.VolumeLabel.Equals("RPI-RP2", StringComparison.OrdinalIgnoreCase))
                    {
                        return drive.RootDirectory.FullName;
                    }
                }
                catch
                {
                    // 忽略单个驱动器的访问错误
                }
            }
        }
        catch
        {
            // 忽略枚举驱动器的错误
        }
        return null;
    }

    private void SelectFile()
    {
        var path = _fileDialogService.OpenFile("选择固件文件", "固件文件|*.uf2;*.bin|UF2 文件|*.uf2|BIN 文件|*.bin");
        if (!string.IsNullOrEmpty(path))
        {
            FirmwarePath = path;
            var fileInfo = new FileInfo(path);
            AddLog($"已选择固件: {Path.GetFileName(path)} ({fileInfo.Length} 字节)");
            UpdateCommands();
        }
    }

    private async void StartUpdate()
    {
        try
        {
            if (string.IsNullOrEmpty(FirmwarePath) || !File.Exists(FirmwarePath))
                return;

            _cts = new CancellationTokenSource();
            IsUpdating = true;
            Progress = 0;
            LogMessages.Clear();
            UpdateCommands();

            var progress = new Progress<int>(p =>
            {
                Progress = p;
            });

            AddLog("开始固件升级...");
            bool success = await _deviceService.DfuUpgradeAsync(FirmwarePath, progress, _cts.Token);

            IsUpdating = false;
            UpdateCommands();

            if (success)
            {
                Status = "升级成功";
                AddLog("=== 固件升级完成 ===");
                AddLog("设备已自动重启，当前固件版本: " + (_deviceService.FirmwareVersion ?? "未知"));
            }
            else
            {
                Status = "升级失败，请重试";
                AddLog("=== 固件升级失败 ===");
                AddLog("故障排查建议:");
                AddLog("  1. 检查设备是否正确连接（USB 数据线，非充电线）");
                AddLog("  2. 确认设备已进入 DFU 模式（Bootloader VID=0x1209 PID=0x4005）");
                AddLog("  3. 尝试重新插拔设备后再次升级");
                AddLog("  4. 如多次失败，请查看上方日志了解具体失败阶段");
                AddLog("提示: 已选择的固件文件保留，可直接点击\"开始升级\"重试");
            }
        }
        catch (Exception ex)
        {
            // async void 方法必须捕获所有异常，防止未观察异常导致应用崩溃
            IsUpdating = false;
            UpdateCommands();
            Status = "升级异常";
            AddLog($"升级过程中发生异常: {ex.Message}");
        }
    }

    private void CancelUpdate()
    {
        _cts?.Cancel();
        AddLog("正在取消升级...");
    }

    private void OnDeviceConnectionChanged(object? sender, bool isConnected)
    {
        IsDeviceConnected = isConnected;
        CurrentFirmwareVersion = isConnected ? _deviceService.FirmwareVersion : null;
        // 连接瞬间版本/能力可能尚未解析完成，先刷新一次；权威值由 VersionCheckCompleted 再刷
        if (isConnected)
        {
            RefreshDfuAvailability();
        }
        else
        {
            IsDfuUpgradeAvailable = false;
        }
        UpdateCommands();
    }

    /// <summary>设备信息与能力位图解析完成后，按 bit10 确定一键/在线 DFU 是否可用。</summary>
    private void OnVersionCheckCompleted(object? sender, CompatibilityResult result) => RefreshDfuAvailability();

    private void RefreshDfuAvailability()
    {
        IsDfuUpgradeAvailable = _deviceService.IsConnected
            && _deviceService.IsCapabilityAvailable(CapabilityId.DfuUpgrade);
    }

    private void OnDfuStatusChanged(object? sender, string message)
    {
        AddLog(message);
    }

    private void AddLog(string message)
    {
        LogMessages.Add($"[{DateTime.Now:HH:mm:ss}] {message}");
        Status = message;
    }

    public void Dispose()
    {
        _deviceService.DeviceConnectionChanged -= OnDeviceConnectionChanged;
        _deviceService.DfuStatusChanged -= OnDfuStatusChanged;
        _deviceService.VersionCheckCompleted -= OnVersionCheckCompleted;
        _cts?.Cancel();
        _cts?.Dispose();
    }
}
