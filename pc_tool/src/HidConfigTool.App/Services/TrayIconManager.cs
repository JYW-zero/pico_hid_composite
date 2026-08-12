using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace HidConfigTool.App.Services;

/// <summary>
/// 系统托盘图标管理
/// </summary>
public class TrayIconManager : IDisposable
{
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    private static extern bool DestroyIcon(IntPtr handle);

    private readonly NotifyIcon _notifyIcon;
    private System.Windows.Window? _mainWindow;

    /// <summary>
    /// 是否最小化到托盘
    /// </summary>
    public bool MinimizeToTray { get; set; } = true;

    public TrayIconManager()
    {
        _notifyIcon = new NotifyIcon
        {
            Text = "HID 配置工具",
            Visible = true,
            Icon = GenerateDefaultIcon()
        };

        // 创建右键菜单
        var contextMenu = new ContextMenuStrip();

        var showMenuItem = new ToolStripMenuItem("显示主窗口");
        showMenuItem.Click += (s, e) => ShowMainWindow();
        contextMenu.Items.Add(showMenuItem);

        contextMenu.Items.Add(new ToolStripSeparator());

        var exitMenuItem = new ToolStripMenuItem("退出");
        exitMenuItem.Click += (s, e) => ExitApplication();
        contextMenu.Items.Add(exitMenuItem);

        _notifyIcon.ContextMenuStrip = contextMenu;

        // 双击托盘图标显示主窗口
        _notifyIcon.MouseDoubleClick += (s, e) =>
        {
            if (e.Button == MouseButtons.Left)
                ShowMainWindow();
        };
    }

    /// <summary>
    /// 设置主窗口引用
    /// </summary>
    public void SetMainWindow(System.Windows.Window window)
    {
        _mainWindow = window;
        _mainWindow.StateChanged += MainWindow_StateChanged;
        _mainWindow.Closing += MainWindow_Closing;
    }

    private void MainWindow_StateChanged(object? sender, EventArgs e)
    {
        if (_mainWindow == null)
            return;

        if (MinimizeToTray && _mainWindow.WindowState == System.Windows.WindowState.Minimized)
        {
            _mainWindow.Hide();
            ShowBalloonTip("已最小化到托盘", "双击托盘图标显示主窗口");
        }
    }

    private void MainWindow_Closing(object? sender, System.ComponentModel.CancelEventArgs e)
    {
        // 如果设置了最小化到托盘，则关闭时最小化而不是退出
        if (MinimizeToTray)
        {
            e.Cancel = true;
            _mainWindow?.Hide();
            ShowBalloonTip("已最小化到托盘", "程序继续在后台运行");
        }
    }

    /// <summary>
    /// 显示主窗口
    /// </summary>
    public void ShowMainWindow()
    {
        if (_mainWindow == null)
            return;

        _mainWindow.Show();
        _mainWindow.WindowState = System.Windows.WindowState.Normal;
        _mainWindow.Activate();
    }

    /// <summary>
    /// 退出应用程序
    /// </summary>
    public void ExitApplication()
    {
        _notifyIcon.Visible = false;
        System.Windows.Application.Current.Shutdown();
    }

    /// <summary>
    /// 显示气泡提示
    /// </summary>
    public void ShowBalloonTip(string title, string message, int timeoutMs = 3000)
    {
        _notifyIcon.ShowBalloonTip(timeoutMs, title, message, ToolTipIcon.Info);
    }

    /// <summary>
    /// 生成默认图标（简单的蓝色方块）
    /// </summary>
    private static Icon GenerateDefaultIcon()
    {
        // 创建一个 32x32 的位图
        using var bitmap = new Bitmap(32, 32);
        using var g = Graphics.FromImage(bitmap);

        // 蓝色背景
        g.Clear(Color.FromArgb(122, 162, 247)); // #7AA2F7

        // 白色 H 字母
        using var font = new Font("Arial", 16, FontStyle.Bold);
        var textSize = g.MeasureString("H", font);
        g.DrawString("H", font, Brushes.White,
            (32 - textSize.Width) / 2,
            (32 - textSize.Height) / 2);

        // 转换为 Icon
        IntPtr hIcon = bitmap.GetHicon();
        try
        {
            // FromHandle 不拥有句柄所有权，需要 Clone 创建独立副本后销毁原始句柄
            return (Icon)Icon.FromHandle(hIcon).Clone();
        }
        finally
        {
            DestroyIcon(hIcon);
        }
    }

    public void Dispose()
    {
        _notifyIcon.Dispose();
        GC.SuppressFinalize(this);
    }
}
