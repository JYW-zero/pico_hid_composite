using Microsoft.Win32;
using System.Diagnostics;
using System.IO;

namespace HidConfigTool.App.Services;

/// <summary>
/// 开机自启动管理
/// </summary>
public class AutoStartManager
{
    private const string RegistryKeyPath = @"Software\Microsoft\Windows\CurrentVersion\Run";
    private const string AppName = "HIDConfigTool";

    /// <summary>
    /// 检查是否已启用自启动
    /// </summary>
    public static bool IsEnabled()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegistryKeyPath, true);
            if (key == null)
                return false;

            var value = key.GetValue(AppName);
            return value != null;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 启用自启动
    /// </summary>
    public static bool Enable()
    {
        try
        {
            string exePath = GetExecutablePath();

            using var key = Registry.CurrentUser.CreateSubKey(RegistryKeyPath, true);
            if (key == null)
                return false;

            key.SetValue(AppName, $"\"{exePath}\"");
            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 禁用自启动
    /// </summary>
    public static bool Disable()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RegistryKeyPath, true);
            if (key == null)
                return true;

            key.DeleteValue(AppName, false);
            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 切换自启动状态
    /// </summary>
    public static bool Toggle(bool enable)
    {
        return enable ? Enable() : Disable();
    }

    /// <summary>
    /// 获取可执行文件路径
    /// </summary>
    private static string GetExecutablePath()
    {
        // 优先使用当前进程路径
        var process = Process.GetCurrentProcess();
        string path = process.MainModule?.FileName ?? string.Empty;

        if (!string.IsNullOrEmpty(path) && File.Exists(path))
            return path;

        // 备用：使用 AppContext.BaseDirectory
        string baseDir = AppContext.BaseDirectory;
        string exeName = Path.GetFileNameWithoutExtension(baseDir.TrimEnd(Path.DirectorySeparatorChar)) + ".exe";
        string exePath = Path.Combine(baseDir, exeName);

        if (File.Exists(exePath))
            return exePath;

        // 如果是 dotnet run 模式，返回 dotnet 路径 + 项目 dll
        return path;
    }
}
