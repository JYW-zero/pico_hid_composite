using System.Windows;
using HidConfigTool.App.Views;

namespace HidConfigTool.App.Services;

/// <summary>
/// OSD 悬浮提示管理器
/// </summary>
public class OsdManager
{
    private OsdWindow? _currentOsd;
    private ProgressOsdWindow? _currentProgressOsd;
    private readonly object _lock = new();

    /// <summary>
    /// 是否启用 OSD 提示
    /// </summary>
    public bool IsEnabled { get; set; } = true;

    /// <summary>
    /// 显示普通 OSD 提示
    /// </summary>
    public void Show(string icon, string title, string message, int durationMs = 2000)
    {
        if (!IsEnabled)
            return;

        if (Application.Current?.Dispatcher == null)
            return;

        Application.Current.Dispatcher.Invoke(() =>
        {
            lock (_lock)
            {
                HideAllInternal();

                var osd = new OsdWindow(icon, title, message, durationMs);
                osd.Closed += (s, e) =>
                {
                    lock (_lock)
                    {
                        if (_currentOsd == osd)
                        {
                            _currentOsd = null;
                        }
                    }
                };

                _currentOsd = osd;
                osd.Show();
            }
        });
    }

    /// <summary>
    /// 显示带进度条的 OSD 提示
    /// </summary>
    public void ShowProgress(string icon, string title, string message, double percentage, int durationMs = 1500)
    {
        if (!IsEnabled)
            return;

        if (Application.Current?.Dispatcher == null)
            return;

        Application.Current.Dispatcher.Invoke(() =>
        {
            lock (_lock)
            {
                HideAllInternal();

                var osd = new ProgressOsdWindow(icon, title, message, percentage, durationMs);
                osd.Closed += (s, e) =>
                {
                    lock (_lock)
                    {
                        if (_currentProgressOsd == osd)
                        {
                            _currentProgressOsd = null;
                        }
                    }
                };

                _currentProgressOsd = osd;
                osd.Show();
            }
        });
    }

    private void HideAllInternal()
    {
        if (_currentOsd != null)
        {
            try { _currentOsd.Close(); } catch { }
            _currentOsd = null;
        }
        if (_currentProgressOsd != null)
        {
            try { _currentProgressOsd.Close(); } catch { }
            _currentProgressOsd = null;
        }
    }

    /// <summary>
    /// 显示音量调节提示
    /// </summary>
    public void ShowVolume(int volumePercent, bool muted = false)
    {
        string icon = muted ? "🔇" : volumePercent > 50 ? "🔊" : volumePercent > 0 ? "🔉" : "🔈";
        string title = muted ? "已静音" : "音量";
        string message = muted ? "" : "使用滚轮调节";
        ShowProgress(icon, title, message, muted ? 0 : volumePercent, 1200);
    }

    /// <summary>
    /// 显示亮度调节提示
    /// </summary>
    public void ShowBrightness(int brightnessPercent)
    {
        string icon = brightnessPercent > 50 ? "☀️" : "🌤️";
        ShowProgress(icon, "屏幕亮度", "使用滚轮调节", brightnessPercent, 1200);
    }

    /// <summary>
    /// 显示 DPI 切换提示（带进度条）
    /// </summary>
    public void ShowDpiProgress(int dpi, int minDpi, int maxDpi)
    {
        double percentage = (double)(dpi - minDpi) / (maxDpi - minDpi) * 100;
        ShowProgress("🎯", "DPI", dpi + " DPI", percentage, 1500);
    }

    /// <summary>
    /// 显示电量提示
    /// </summary>
    public void ShowBattery(int batteryPercent, bool charging = false)
    {
        string icon = charging ? "⚡" : batteryPercent > 50 ? "🔋" : batteryPercent > 20 ? "🪫" : "⚠️";
        string title = charging ? "充电中" : "电量";
        string message = batteryPercent <= 20 ? "电量低，请充电" : "";
        ShowProgress(icon, title, message, batteryPercent, 2000);
    }

    /// <summary>
    /// 显示 DPI 切换提示
    /// </summary>
    public void ShowDpiChange(int dpi)
    {
        Show("🎯", "DPI 已切换", dpi + " DPI", 1500);
    }

    /// <summary>
    /// 显示配置切换提示
    /// </summary>
    public void ShowProfileChange(string profileName)
    {
        Show("📋", "配置已切换", profileName, 1500);
    }

    /// <summary>
    /// 显示层切换提示
    /// </summary>
    public void ShowLayerChange(string layerName)
    {
        Show("⌨️", "层已切换", layerName, 1500);
    }

    /// <summary>
    /// 显示成功提示
    /// </summary>
    public void ShowSuccess(string message)
    {
        Show("✅", "成功", message, 2000);
    }

    /// <summary>
    /// 显示错误提示
    /// </summary>
    public void ShowError(string message)
    {
        Show("❌", "错误", message, 3000);
    }

    /// <summary>
    /// 显示警告提示
    /// </summary>
    public void ShowWarning(string message)
    {
        Show("⚠️", "警告", message, 2500);
    }

    /// <summary>
    /// 隐藏所有 OSD
    /// </summary>
    public void Hide()
    {
        if (Application.Current?.Dispatcher == null)
            return;

        Application.Current.Dispatcher.Invoke(() =>
        {
            lock (_lock)
            {
                HideAllInternal();
            }
        });
    }
}
