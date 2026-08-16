using System.Windows;
using System.Windows.Controls;
using HidConfigTool.App.ViewModels;

namespace HidConfigTool.App.Views;

/// <summary>
/// 按键管理页面：整合按键测试、按键设置、按键统计
/// </summary>
public partial class KeyManagementPage : UserControl, IDisposable
{
    private bool _disposed;
    private readonly KeyTestPage _keyTestPage;
    private readonly KeyPage _keyPage;
    private readonly StatsPage _statsPage;

    public KeyManagementPage(
        KeyTestPage keyTestPage,
        KeyPage keyPage,
        StatsPage statsPage)
    {
        InitializeComponent();

        _keyTestPage = keyTestPage;
        _keyPage = keyPage;
        _statsPage = statsPage;

        // 将注入的子页面设置到占位符
        KeyTestPlaceholder.Content = keyTestPage;
        KeyPagePlaceholder.Content = keyPage;
        StatsPagePlaceholder.Content = statsPage;

        // 监听 Tab 切换，控制按键测试页的轮询
        MainTabControl.SelectionChanged += OnTabSelectionChanged;

        // 初始状态：默认选中第一个 Tab（按键测试），确保轮询状态正确
        UpdateTestPageRefresh();
    }

    private void OnTabSelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        UpdateTestPageRefresh();
    }

    /// <summary>
    /// 根据当前选中的 Tab 控制按键测试页的自动刷新
    /// </summary>
    private void UpdateTestPageRefresh()
    {
        if (_keyTestPage?.DataContext is KeyTestPageViewModel vm)
        {
            // 只有选中"按键测试"Tab 时才启用轮询，节省 USB 带宽
            vm.AutoRefresh = MainTabControl.SelectedIndex == 0;
        }
    }

    /// <summary>
    /// 释放资源，清理子页面的 ViewModel，防止内存泄漏
    /// </summary>
    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;

        MainTabControl.SelectionChanged -= OnTabSelectionChanged;

        // 清理子页面的 ViewModel
        DisposeChildViewModel(_keyTestPage);
        DisposeChildViewModel(_keyPage);
        DisposeChildViewModel(_statsPage);
    }

    private static void DisposeChildViewModel(FrameworkElement? child)
    {
        if (child?.DataContext is IDisposable disposable)
        {
            disposable.Dispose();
        }
    }
}
