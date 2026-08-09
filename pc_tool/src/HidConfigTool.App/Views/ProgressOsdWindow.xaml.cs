using System.Windows;
using System.Windows.Media.Animation;

namespace HidConfigTool.App.Views;

/// <summary>
/// 带进度条的 OSD 悬浮提示窗口
/// 用于音量、亮度、DPI 等数值调节提示
/// </summary>
public partial class ProgressOsdWindow : Window
{
    private readonly int _displayDurationMs;
    private readonly double _percentage;

    public ProgressOsdWindow(string icon, string title, string message, double percentage, int displayDurationMs = 1500)
    {
        InitializeComponent();

        _displayDurationMs = displayDurationMs;
        _percentage = Math.Clamp(percentage, 0, 100);

        // 设置数据
        DataContext = new
        {
            Icon = icon,
            Title = title,
            Message = message,
            PercentageText = $"{_percentage:F0}%"
        };

        // 定位到屏幕右下角
        PositionToBottomRight();

        // 加载完成后开始淡入和进度条动画
        Loaded += OnLoaded;
    }

    private void PositionToBottomRight()
    {
        var workingArea = SystemParameters.WorkArea;
        Left = workingArea.Right - Width - 24;
        Top = workingArea.Bottom - Height - 24;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        // 计算进度条最大宽度（内容区域宽度 = 总宽度 - 左右边距）
        double maxWidth = 272; // 320 - 24*2
        double targetWidth = maxWidth * (_percentage / 100.0);

        // 进度条动画
        var progressAnim = new DoubleAnimation
        {
            From = 0,
            To = targetWidth,
            Duration = TimeSpan.FromMilliseconds(300),
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
        };
        ProgressBar.BeginAnimation(FrameworkElement.WidthProperty, progressAnim);

        // 淡入动画
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
