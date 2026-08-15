using System.IO;
using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.App.Services;
using HidConfigTool.App.Views;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;
using System.Collections.ObjectModel;
using Microsoft.Win32;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 设置页面视图模型
/// </summary>
/// <summary>
/// 应用感知规则项视图模型
/// </summary>
public partial class AppAwarenessRuleItemViewModel : ObservableObject
{
    [ObservableProperty]
    private string _processName = string.Empty;

    [ObservableProperty]
    private string _appName = string.Empty;

    [ObservableProperty]
    private string _profileName = string.Empty;

    [ObservableProperty]
    private bool _isEnabled = true;
}

public partial class SettingsPageViewModel : ObservableObject
{
    private readonly TrayIconManager _trayIconManager;
    private readonly IDeviceService _deviceService;

    [ObservableProperty]
    private bool _minimizeToTray;

    [ObservableProperty]
    private bool _autoStart;

    [ObservableProperty]
    private bool _checkUpdateOnStart = true;

    [ObservableProperty]
    private bool _osdEnabled = true;
    /// <summary>
    /// 应用感知总开关
    /// </summary>
    [ObservableProperty]
    private bool _appAwarenessEnabled;

    /// <summary>
    /// 当前前台应用
    /// </summary>
    [ObservableProperty]
    private string _currentForegroundApp = "未知";

    /// <summary>
    /// 应用感知规则列表
    /// </summary>
    public ObservableCollection<AppAwarenessRuleItemViewModel> AppAwarenessRules { get; } = new();

    [ObservableProperty]
    private string _language = "简体中文";

    [ObservableProperty]
    private string _theme = "深色";

    [ObservableProperty]
    private string _statusMessage = string.Empty;
    /// <summary>
    /// 配置文件列表
    /// </summary>
    public ObservableCollection<string> Profiles { get; } = new();

    /// <summary>
    /// 当前选中的配置文件
    /// </summary>
    [ObservableProperty]
    private string? _currentProfile;
    [ObservableProperty]
    private string _currentFirmwareVersion = "v1.0.0";

    [ObservableProperty]
    private string _latestFirmwareVersion = "未知";

    [ObservableProperty]
    private string _firmwareFilePath = string.Empty;

    [ObservableProperty]
    private double _updateProgress;

    [ObservableProperty]
    private bool _isUpdating;

    [ObservableProperty]
    private string _updateStatus = "就绪";

    private readonly ConfigProfileManager _profileManager;
    private readonly OsdManager _osdManager;
    private readonly AppAwarenessManager _appAwarenessManager;
    private readonly ThemeManager _themeManager;
    private readonly LanguageManager _languageManager;

    public SettingsPageViewModel(TrayIconManager trayIconManager, IDeviceService deviceService, ConfigProfileManager profileManager, OsdManager osdManager, AppAwarenessManager appAwarenessManager, ThemeManager themeManager, LanguageManager languageManager)
    {
        _trayIconManager = trayIconManager;
        _deviceService = deviceService;
        _profileManager = profileManager;
        _osdManager = osdManager;
        _appAwarenessManager = appAwarenessManager;
        _themeManager = themeManager;
        _languageManager = languageManager;

        // 加载当前主题
        Theme = _themeManager.CurrentTheme == ThemeManager.LightTheme ? "浅色" : "深色";

        // 加载当前语言
        Language = _languageManager.CurrentLanguage == LanguageManager.English ? "English" : "简体中文";

        // 加载应用感知规则
        foreach (var rule in _appAwarenessManager.Rules)
        {
            AppAwarenessRules.Add(new AppAwarenessRuleItemViewModel
            {
                ProcessName = rule.ProcessName,
                AppName = rule.AppName,
                ProfileName = rule.ProfileName,
                IsEnabled = rule.IsEnabled
            });
        }

        // 加载配置文件列表
        foreach (var p in _profileManager.Profiles)
        {
            Profiles.Add(p.Name);
        }

        // 如果没有配置文件，添加几个示例
        if (Profiles.Count == 0)
        {
            Profiles.Add("默认配置");
            Profiles.Add("游戏配置");
            Profiles.Add("办公配置");
        }

        CurrentProfile = Profiles[0];

        // 加载当前设置
        MinimizeToTray = _trayIconManager.MinimizeToTray;
        AutoStart = AutoStartManager.IsEnabled();
    }

    partial void OnMinimizeToTrayChanged(bool value)
    {
        _trayIconManager.MinimizeToTray = value;
    }

    partial void OnOsdEnabledChanged(bool value)
    {
        _osdManager.IsEnabled = value;
    }

    partial void OnAutoStartChanged(bool value)
    {
        AutoStartManager.Toggle(value);
    }

    partial void OnThemeChanged(string value)
    {
        string theme = value == "浅色" ? ThemeManager.LightTheme : ThemeManager.DarkTheme;
        _themeManager.SetTheme(theme);
    }

    partial void OnLanguageChanged(string value)
    {
        string language = value == "English" ? LanguageManager.English : LanguageManager.Chinese;
        _languageManager.SetLanguage(language);
    }

    [RelayCommand]
    private async Task ExportConfigAsync()
    {
        try
        {
            var dialog = new SaveFileDialog
            {
                Filter = "配置文件 (*.json)|*.json|所有文件 (*.*)|*.*",
                DefaultExt = "json",
                FileName = "hid_config.json"
            };

            if (dialog.ShowDialog() == true)
            {
                bool result = await _deviceService.ExportConfigAsync(dialog.FileName);
                if (result)
                {
                    StatusMessage = "配置导出成功";
                    MessageBox.Show("配置导出成功！", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                else
                {
                    StatusMessage = "配置导出失败";
                    MessageBox.Show("配置导出失败！", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"导出失败: {ex.Message}";
            MessageBox.Show($"导出失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    [RelayCommand]
    private async Task ImportConfigAsync()
    {
        try
        {
            var dialog = new OpenFileDialog
            {
                Filter = "配置文件 (*.json)|*.json|所有文件 (*.*)|*.*",
                DefaultExt = "json"
            };

            if (dialog.ShowDialog() == true)
            {
                var result = MessageBox.Show("导入配置将覆盖当前设置，确定要继续吗？", "确认导入",
                    MessageBoxButton.YesNo, MessageBoxImage.Question);

                if (result == MessageBoxResult.Yes)
                {
                    bool success = await _deviceService.ImportConfigAsync(dialog.FileName);
                    if (success)
                    {
                        StatusMessage = "配置导入成功";
                        MessageBox.Show("配置导入成功！", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
                    }
                    else
                    {
                        StatusMessage = "配置导入失败";
                        MessageBox.Show("配置导入失败！", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"导入失败: {ex.Message}";
            MessageBox.Show($"导入失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    [RelayCommand]
    private async Task ResetConfigAsync()
    {
        try
        {
            var result = MessageBox.Show("恢复默认配置将清除所有自定义设置，确定要继续吗？", "确认恢复",
                MessageBoxButton.YesNo, MessageBoxImage.Warning);

            if (result == MessageBoxResult.Yes)
            {
                bool success = await _deviceService.ResetConfigAsync();
                if (success)
                {
                    StatusMessage = "恢复默认配置成功";
                    MessageBox.Show("恢复默认配置成功！", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                else
                {
                    StatusMessage = "恢复默认配置失败";
                    MessageBox.Show("恢复默认配置失败！", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"恢复默认配置失败: {ex.Message}";
            MessageBox.Show($"恢复默认配置失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }


    [RelayCommand]
    private async Task CheckUpdateAsync()
    {
        UpdateStatus = "正在检查更新...";
        await Task.Delay(1000);
        LatestFirmwareVersion = "v1.0.0";
        UpdateStatus = "当前已是最新版本";
    }

    [RelayCommand]
    private void SelectFirmwareFile()
    {
        var dialog = new OpenFileDialog
        {
            Filter = "固件文件 (*.uf2;*.bin;*.hex)|*.uf2;*.bin;*.hex|所有文件 (*.*)|*.*",
            Title = "选择固件文件"
        };

        if (dialog.ShowDialog() == true)
        {
            FirmwareFilePath = dialog.FileName;
            UpdateStatus = $"已选择: {System.IO.Path.GetFileName(dialog.FileName)}";
        }
    }

    [RelayCommand]
    private async Task StartUpdateAsync()
    {
        if (string.IsNullOrEmpty(FirmwareFilePath))
        {
            MessageBox.Show("请先选择固件文件", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        var result = MessageBox.Show("确定要开始固件升级吗？升级过程中请勿断开设备。", "确认升级",
            MessageBoxButton.YesNo, MessageBoxImage.Question);

        if (result != MessageBoxResult.Yes)
            return;

        IsUpdating = true;
        UpdateStatus = "正在升级...";
        UpdateProgress = 0;

        // 模拟升级进度
        for (int i = 0; i <= 100; i += 10)
        {
            await Task.Delay(200);
            UpdateProgress = i;
        }

        UpdateProgress = 100;
        UpdateStatus = "升级完成！";
        IsUpdating = false;

        MessageBox.Show("固件升级完成！", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    [RelayCommand]
    private async Task SwitchProfile(string? profileName)
    {
        if (string.IsNullOrEmpty(profileName))
            return;

        CurrentProfile = profileName;
        StatusMessage = $"已切换到配置: {profileName}";
        _osdManager.ShowProfileChange(profileName);

        // 从文件加载配置并应用到设备
        try
        {
            var config = _profileManager.LoadProfile(profileName);
            if (config != null)
            {
                await _deviceService.SaveConfigAsync(config);
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"配置应用失败: {ex.Message}";
        }
    }

    [RelayCommand]
    private void NewProfile()
    {
        string newName = $"新配置 {Profiles.Count + 1}";

        // 确保名称不重复
        int suffix = 1;
        while (_profileManager.ProfileExists(newName))
        {
            newName = $"新配置 {Profiles.Count + suffix}";
            suffix++;
        }

        // 保存默认配置到文件
        var defaultConfig = new DeviceConfig();
        if (_profileManager.SaveProfile(newName, defaultConfig))
        {
            Profiles.Add(newName);
            CurrentProfile = newName;
            StatusMessage = $"已创建新配置: {newName}";
        }
        else
        {
            StatusMessage = $"创建配置失败: {newName}";
        }
    }

    [RelayCommand]
    private void DeleteProfile()
    {
        if (string.IsNullOrEmpty(CurrentProfile))
            return;

        var result = MessageBox.Show($"确定要删除配置 \"{CurrentProfile}\" 吗？", "确认删除",
            MessageBoxButton.YesNo, MessageBoxImage.Question);

        if (result == MessageBoxResult.Yes)
        {
            string toDelete = CurrentProfile;

            // 从文件删除
            if (_profileManager.DeleteProfile(toDelete))
            {
                Profiles.Remove(toDelete);

                if (Profiles.Count > 0)
                {
                    CurrentProfile = Profiles[0];
                }
                else
                {
                    CurrentProfile = null;
                }

                StatusMessage = $"已删除配置: {toDelete}";
            }
            else
            {
                StatusMessage = $"删除配置失败: {toDelete}";
            }
        }
    }


    partial void OnAppAwarenessEnabledChanged(bool value)
    {
        if (value)
        {
            _appAwarenessManager.Start();
            StatusMessage = "应用感知已开启";
        }
        else
        {
            _appAwarenessManager.Stop();
            StatusMessage = "应用感知已关闭";
        }
    }

    [RelayCommand]
    private void AddAppRule()
    {
        // 简单实现，添加一个示例规则
        var newRule = new AppAwarenessRuleItemViewModel
        {
            ProcessName = "newapp",
            AppName = "新应用",
            ProfileName = "默认配置",
            IsEnabled = true
        };
        AppAwarenessRules.Add(newRule);
        _appAwarenessManager.AddRule(newRule.ProcessName, newRule.AppName, newRule.ProfileName);
        StatusMessage = "已添加新规则";
    }

    [RelayCommand]
    private void RemoveAppRule(AppAwarenessRuleItemViewModel? rule)
    {
        if (rule == null)
            return;

        AppAwarenessRules.Remove(rule);
        _appAwarenessManager.RemoveRule(rule.ProcessName);
        StatusMessage = $"已删除规则: {rule.AppName}";
    }
    [RelayCommand]
    private void RenameProfile()
    {
        if (string.IsNullOrEmpty(CurrentProfile))
            return;

        // 弹出输入框让用户输入新名字
        string? newName = Views.InputDialog.Show(
            Application.Current.MainWindow,
            "请输入新的配置名称：",
            "重命名配置",
            CurrentProfile);

        if (string.IsNullOrWhiteSpace(newName))
            return;

        if (newName == CurrentProfile)
            return;

        // 检查是否重名
        if (_profileManager.ProfileExists(newName))
        {
            MessageBox.Show("该名称已存在，请换一个名称。", "提示", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        // 实际重命名文件
        if (_profileManager.RenameProfile(CurrentProfile, newName))
        {
            int index = Profiles.IndexOf(CurrentProfile);
            if (index >= 0)
            {
                Profiles[index] = newName;
                CurrentProfile = newName;
                StatusMessage = $"已重命名为: {newName}";
            }
        }
        else
        {
            StatusMessage = $"重命名失败: {CurrentProfile}";
        }
    }
    [RelayCommand]
    private void OpenLogFolder()
    {
        // TODO: 打开日志文件夹
        MessageBox.Show("功能开发中...", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    [RelayCommand]
    private void OpenHelp()
    {
        var helpWindow = new HelpWindow
        {
            Owner = System.Windows.Application.Current.MainWindow
        };
        helpWindow.ShowDialog();
    }
}






