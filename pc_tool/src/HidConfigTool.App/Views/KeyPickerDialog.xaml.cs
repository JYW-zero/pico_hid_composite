using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using HidConfigTool.Core.Models;

namespace HidConfigTool.App.Views;

/// <summary>
/// 按键选择对话框
/// </summary>
public partial class KeyPickerDialog : Window
{
    /// <summary>
    /// 选中的键码
    /// </summary>
    public byte SelectedKeyCode { get; private set; }

    /// <summary>
    /// 选中的键名
    /// </summary>
    public string SelectedKeyName { get; private set; } = string.Empty;

    private readonly List<KeyDefinition> _allKeys;
    private string _currentCategory = "全部";

    public KeyPickerDialog(byte currentKeyCode = 0)
    {
        InitializeComponent();

        _allKeys = KeyDefinitions.GetAll();
        KeyListBox.ItemsSource = _allKeys;

        // 选中当前键
        var currentKey = _allKeys.FirstOrDefault(k => k.KeyCode == currentKeyCode);
        if (currentKey != null)
        {
            KeyListBox.SelectedItem = currentKey;
            KeyListBox.ScrollIntoView(currentKey);
        }
    }

    private void SearchBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        FilterKeys();
    }

    private void CategoryButton_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button button && button.Tag is string category)
        {
            _currentCategory = category;

            // 更新按钮样式
            UpdateCategoryButtonStyles();

            FilterKeys();
        }
    }

    private void UpdateCategoryButtonStyles()
    {
        // 简单处理：全部按钮特殊处理，其他按钮都用 Secondary
        // 实际项目可以用数据绑定，这里简化处理
        AllCategoryButton.Style = _currentCategory == "全部"
            ? (Style)FindResource("PrimaryButtonStyle")
            : (Style)FindResource("SecondaryButtonStyle");
    }

    private void FilterKeys()
    {
        string searchText = SearchBox.Text?.ToLower() ?? string.Empty;

        IEnumerable<KeyDefinition> filtered = _allKeys;

        // 分类过滤
        if (_currentCategory != "全部")
        {
            filtered = filtered.Where(k => k.Category == _currentCategory);
        }

        // 搜索过滤
        if (!string.IsNullOrEmpty(searchText))
        {
            filtered = filtered.Where(k =>
                k.Name.ToLower().Contains(searchText) ||
                k.Description.ToLower().Contains(searchText) ||
                k.KeyCode.ToString("X2").ToLower().Contains(searchText));
        }

        KeyListBox.ItemsSource = filtered.ToList();
    }

    private void KeyListBox_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
    {
        if (KeyListBox.SelectedItem is KeyDefinition key)
        {
            SelectKey(key);
        }
    }

    private void OkButton_Click(object sender, RoutedEventArgs e)
    {
        if (KeyListBox.SelectedItem is KeyDefinition key)
        {
            SelectKey(key);
        }
        else
        {
            MessageBox.Show("请选择一个按键", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
        }
    }

    private void CancelButton_Click(object sender, RoutedEventArgs e)
    {
        DialogResult = false;
        Close();
    }

    private void SelectKey(KeyDefinition key)
    {
        SelectedKeyCode = key.KeyCode;
        SelectedKeyName = key.Name;
        DialogResult = true;
        Close();
    }
}
