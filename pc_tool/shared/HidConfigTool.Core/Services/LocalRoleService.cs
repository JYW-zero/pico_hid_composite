/*
 * LocalRoleService.cs
 * 本地角色服务（功能角色分离，协议 v0.2）
 *
 * “本地角色”是与“设备能力位图”正交的纯本机 UI 模式：
 *   - Consumer（消费者标准模式）：只显示消费者功能页面；
 *   - Developer（开发者模式）：额外显示性能监控、错误日志、诊断等工程页面。
 *
 * 安全边界说明：本地角色只控制这台电脑上上位机界面的显隐，避免普通消费者被工程
 * 功能困扰；它不是安全机制——即使绕过界面直接发 raw HID，固件侧命令/报告闸门
 * （command_gate）仍会按能力位图与 manufacturing flag 强校验。
 *
 * 设计为静态服务 + 单文件持久化（%AppData%/HIDConfigTool/local_role.dat），
 * 不依赖 DI 容器，WPF 与 Avalonia 两端、各 ViewModel 均可直接订阅。
 */

using System;
using System.IO;

namespace HidConfigTool.Core.Services;

/// <summary>
/// 上位机本地角色
/// </summary>
public enum LocalRole
{
    /// <summary>消费者标准模式（默认）：仅消费者功能</summary>
    Consumer = 0,

    /// <summary>开发者模式：额外显示诊断/工程页面（仍受设备能力位图约束）</summary>
    Developer = 1
}

/// <summary>
/// 本地角色服务（静态、轻量、文件持久化）
/// </summary>
public static class LocalRoleService
{
    private static readonly string ConfigDirectory = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "HIDConfigTool");

    private static readonly string ConfigFile = Path.Combine(ConfigDirectory, "local_role.dat");

    private static LocalRole _current = Load();

    /// <summary>当前本地角色</summary>
    public static LocalRole Current => _current;

    /// <summary>当前是否为开发者模式</summary>
    public static bool IsDeveloper => _current == LocalRole.Developer;

    /// <summary>角色变化通知（UI 订阅以刷新菜单显隐）</summary>
    public static event Action? Changed;

    /// <summary>设置本地角色并持久化</summary>
    public static void SetRole(LocalRole role)
    {
        if (_current == role)
        {
            return;
        }

        _current = role;
        Save(role);
        Changed?.Invoke();
    }

    /// <summary>在消费者/开发者模式之间切换</summary>
    /// <returns>切换后是否处于开发者模式</returns>
    public static bool ToggleDeveloper()
    {
        SetRole(IsDeveloper ? LocalRole.Consumer : LocalRole.Developer);
        return IsDeveloper;
    }

    private static LocalRole Load()
    {
        try
        {
            if (File.Exists(ConfigFile) && File.ReadAllText(ConfigFile).Trim() == "1")
            {
                return LocalRole.Developer;
            }
        }
        catch
        {
            // 持久化文件不可读时安全回退到消费者模式
        }

        return LocalRole.Consumer;
    }

    private static void Save(LocalRole role)
    {
        try
        {
            Directory.CreateDirectory(ConfigDirectory);
            File.WriteAllText(ConfigFile, ((int)role).ToString());
        }
        catch
        {
            // 持久化失败不影响本次运行的内存状态
        }
    }
}
