namespace HidConfigTool.ViewModels;

using System.Collections.ObjectModel;
using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;
using HidConfigTool.Core.Services;

/// <summary>
/// 配置预设页面视图模型
/// </summary>
public partial class PresetPageViewModel : PageViewModelBase
{
    private readonly PresetService _presetService;
    private readonly IDialogService _dialogService;
    private readonly IInputDialogService _inputDialogService;

    /// <summary>
    /// 是否正在应用预设
    /// </summary>
    [ObservableProperty]
    private bool _isApplying;

    /// <summary>
    /// 当前选中的分类
    /// </summary>
    [ObservableProperty]
    private string _selectedCategory = "全部";

    /// <summary>
    /// 当前选中的预设
    /// </summary>
    [ObservableProperty]
    private ConfigPreset? _selectedPreset;

    /// <summary>
    /// 搜索关键词
    /// </summary>
    [ObservableProperty]
    private string _searchText = string.Empty;

    /// <summary>
    /// 所有预设
    /// </summary>
    public ObservableCollection<ConfigPreset> AllPresets => _presetService.AllPresets;

    /// <summary>
    /// 预设分类
    /// </summary>
    public ObservableCollection<string> Categories => _presetService.Categories;

    /// <summary>
    /// 筛选后的预设列表
    /// </summary>
    public ObservableCollection<ConfigPreset> FilteredPresets { get; } = new();

    public PresetPageViewModel(
        IDeviceService deviceService,
        PresetService presetService,
        IDialogService dialogService,
        IInputDialogService inputDialogService)
        : base(deviceService)
    {
        _presetService = presetService;
        _dialogService = dialogService;
        _inputDialogService = inputDialogService;
        StatusMessage = "就绪";

        // 初始化筛选列表
        RefreshFilteredPresets();
    }

    /// <summary>
    /// 设备已断开（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceDisconnectedAsync()
    {
        StatusMessage = "设备已断开";
        return Task.CompletedTask;
    }

    /// <summary>
    /// 刷新筛选后的预设列表
    /// </summary>
    private void RefreshFilteredPresets()
    {
        FilteredPresets.Clear();

        IEnumerable<ConfigPreset> presets = AllPresets;

        // 按分类筛选
        if (!string.IsNullOrEmpty(SelectedCategory) && SelectedCategory != "全部")
        {
            presets = presets.Where(p => p.Category == SelectedCategory);
        }

        // 按搜索关键词筛选
        if (!string.IsNullOrWhiteSpace(SearchText))
        {
            var search = SearchText.ToLowerInvariant();
            presets = presets.Where(p =>
                p.Name.ToLowerInvariant().Contains(search) ||
                p.Description.ToLowerInvariant().Contains(search) ||
                p.Category.ToLowerInvariant().Contains(search));
        }

        foreach (var preset in presets)
        {
            FilteredPresets.Add(preset);
        }
    }

    /// <summary>
    /// 选中分类变化时刷新筛选
    /// </summary>
    partial void OnSelectedCategoryChanged(string value)
    {
        RefreshFilteredPresets();
    }

    /// <summary>
    /// 搜索文本变化时刷新筛选
    /// </summary>
    partial void OnSearchTextChanged(string value)
    {
        RefreshFilteredPresets();
    }

    /// <summary>
    /// 应用预设命令
    /// </summary>
    [RelayCommand(CanExecute = nameof(CanApplyPreset))]
    private async Task ApplyPresetAsync()
    {
        if (SelectedPreset == null)
        {
            _dialogService.ShowWarning("请先选择一个预设", "未选择预设");
            return;
        }

        var result = _dialogService.ShowConfirm(
            $"确定要应用「{SelectedPreset.Name}」预设吗？\n\n{SelectedPreset.Summary}",
            "应用预设");

        if (!result)
            return;

        try
        {
            IsApplying = true;
            StatusMessage = $"正在应用「{SelectedPreset.Name}」...";

            var success = await _presetService.ApplyPresetAsync(SelectedPreset);

            if (success)
            {
                StatusMessage = $"预设「{SelectedPreset.Name}」应用成功";
                _dialogService.ShowInfo($"预设「{SelectedPreset.Name}」已成功应用到设备", "应用成功");
            }
            else
            {
                StatusMessage = $"预设「{SelectedPreset.Name}」应用失败";
                _dialogService.ShowError($"预设应用失败，请检查设备连接", "应用失败");
            }
        }
        catch (Exception ex)
        {
            StatusMessage = $"应用失败: {ex.Message}";
            _dialogService.ShowError($"应用失败: {ex.Message}", "错误");
        }
        finally
        {
            IsApplying = false;
        }
    }

    /// <summary>
    /// 是否可以应用预设
    /// </summary>
    private bool CanApplyPreset() => IsDeviceConnected && !IsApplying && SelectedPreset != null;

    /// <summary>
    /// 从当前设备配置创建预设命令
    /// </summary>
    [RelayCommand(CanExecute = nameof(CanCreatePreset))]
    private void CreatePresetFromCurrent()
    {
        if (!DeviceService.IsConnected || DeviceService.CurrentConfig == null)
        {
            _dialogService.ShowWarning("请先连接设备并读取配置", "无法创建预设");
            return;
        }

        var name = _inputDialogService.ShowInput("请输入预设名称", "创建预设", "我的预设");
        if (string.IsNullOrWhiteSpace(name))
            return;

        var description = _inputDialogService.ShowInput("请输入预设描述（可选）", "创建预设", "");
        var category = _inputDialogService.ShowInput("请输入预设分类", "创建预设", "自定义");

        var preset = _presetService.CreatePresetFromCurrent(name, description ?? string.Empty, category ?? "自定义");
        if (preset != null)
        {
            StatusMessage = $"预设「{name}」已创建";
            _dialogService.ShowInfo($"预设「{name}」已从当前设备配置创建", "创建成功");
            RefreshFilteredPresets();
        }
        else
        {
            _dialogService.ShowError("创建预设失败，请检查设备连接", "创建失败");
        }
    }

    /// <summary>
    /// 是否可以创建预设
    /// </summary>
    private bool CanCreatePreset() => IsDeviceConnected && DeviceService.CurrentConfig != null;

    /// <summary>
    /// 删除预设命令
    /// </summary>
    [RelayCommand(CanExecute = nameof(CanDeletePreset))]
    private void DeletePreset()
    {
        if (SelectedPreset == null)
            return;

        if (SelectedPreset.IsBuiltIn)
        {
            _dialogService.ShowWarning("内置预设不可删除", "无法删除");
            return;
        }

        var result = _dialogService.ShowConfirm(
            $"确定要删除预设「{SelectedPreset.Name}」吗？此操作不可撤销。",
            "删除预设");

        if (!result)
            return;

        var success = _presetService.DeletePreset(SelectedPreset);
        if (success)
        {
            StatusMessage = $"预设「{SelectedPreset.Name}」已删除";
            SelectedPreset = null;
            RefreshFilteredPresets();
        }
    }

    /// <summary>
    /// 是否可以删除预设
    /// </summary>
    private bool CanDeletePreset() => SelectedPreset != null && !SelectedPreset.IsBuiltIn;

    /// <summary>
    /// 复制预设命令
    /// </summary>
    [RelayCommand(CanExecute = nameof(CanDuplicatePreset))]
    private void DuplicatePreset()
    {
        if (SelectedPreset == null)
            return;

        var duplicate = _presetService.DuplicatePreset(SelectedPreset);
        StatusMessage = $"预设「{SelectedPreset.Name}」已复制为「{duplicate.Name}」";
        RefreshFilteredPresets();
    }

    /// <summary>
    /// 是否可以复制预设
    /// </summary>
    private bool CanDuplicatePreset() => SelectedPreset != null;

    /// <summary>
    /// 刷新预设列表命令
    /// </summary>
    [RelayCommand]
    private void RefreshPresets()
    {
        _presetService.ReloadPresets();
        RefreshFilteredPresets();
        StatusMessage = "预设列表已刷新";
    }

}
