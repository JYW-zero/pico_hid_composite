using System.Windows;
using System.Windows.Media.Animation;

namespace HidConfigTool.App.Views;

/// <summary>
/// OSD 悬浮提示窗口
/// </summary>
public partial class OsdWindow : Window
{
    private readonly int _displayDurationMs;

    public OsdWindow(string icon, string title, string message, int displayDurationMs = 2000)
    {
        InitializeComponent();

        _displayDurationMs = displayDurationMs;

        // 设置数据
        DataContext = new
        {
            Icon = icon,
            Title = title,
            Message = message
        };

        // 定位到屏幕右下角
        PositionToBottomRight();

        // 加载完成后开始淡入
        Loaded += OnLoaded;
    }

    private void PositionToBottomRight()
    {
        // 获取主显示器工作区
        var workingArea = SystemParameters.WorkArea;

        // 右下角，留出边距
        Left = workingArea.Right - Width - 24;
        Top = workingArea.Bottom - Height - 24;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        // 播放淡入动画
        var fadeIn = (Storyboard)FindResource("FadeInStoryboard");
        fadeIn.Completed += FadeIn_Completed;
        fadeIn.Begin();
    }

    private void FadeIn_Completed(object? sender, EventArgs e)
    {
        // 等待指定时间后淡出
        var timer = new System.Windows.Threading.DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(_displayDurationMs)
        };
        timer.Tick += (s, args) =>
        {
            timer.Stop();
            StartFadeOut();
        };
        timer.Start();
    }

    private void StartFadeOut()
    {
        var fadeOut = (Storyboard)FindResource("FadeOutStoryboard");
        fadeOut.Completed += FadeOut_Completed;
        fadeOut.Begin();
    }

    private void FadeOut_Completed(object? sender, EventArgs e)
    {
        Close();
    }
}
