using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Desktop.ViewModels;

public partial class MacroActionItemViewModel : ObservableObject
{
    [ObservableProperty] private MacroActionType _type;
    [ObservableProperty] private string _keyName = string.Empty;
    [ObservableProperty] private int _delayMs;
    [ObservableProperty] private byte _keyCode;
    [ObservableProperty] private int _index;

    public string TypeText => Type switch
    {
        MacroActionType.KeyDown => "按下",
        MacroActionType.KeyUp => "抬起",
        MacroActionType.Delay => "延时",
        MacroActionType.MouseMove => "移动",
        MacroActionType.MouseClick => "点击",
        MacroActionType.MouseScroll => "滚轮",
        _ => "未知"
    };

    public string Description => Type switch
    {
        MacroActionType.Delay => $"{DelayMs} ms",
        _ => KeyName
    };
}

public partial class MacroPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;

    public ObservableCollection<Macro> Macros { get; } = new();
    public ObservableCollection<MacroActionItemViewModel> Actions { get; } = new();
    public List<KeyDefinition> CommonKeys { get; } = KeyDefinitions.GetAll();

    [ObservableProperty] private Macro? _currentMacro;
    [ObservableProperty] private string _currentMacroName = "宏 1";
    [ObservableProperty] private string _statusMessage = string.Empty;
    [ObservableProperty] private bool _isLoading;
    [ObservableProperty] private MacroActionItemViewModel? _selectedAction;

    public MacroPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        for (int i = 0; i < 8; i++)
            Macros.Add(new Macro { Name = $"宏 {i + 1}" });
        CurrentMacro = Macros[0];
        _ = LoadMacrosFromDeviceAsync();
    }

    partial void OnCurrentMacroChanged(Macro? value)
    {
        if (value == null)
            return;
        CurrentMacroName = value.Name;
        LoadActions(value);
    }

    [RelayCommand]
    private void AddAction()
    {
        if (Actions.Count >= MacroCodec.MaxActions)
            return;
        Actions.Add(new MacroActionItemViewModel
        {
            Type = MacroActionType.KeyDown,
            KeyName = "A",
            KeyCode = 0x04,
            Index = Actions.Count + 1
        });
    }

    [RelayCommand]
    private void RemoveAction()
    {
        if (SelectedAction != null)
            Actions.Remove(SelectedAction);
    }

    [RelayCommand]
    private async Task SaveCurrentMacroAsync()
    {
        if (CurrentMacro == null || !_deviceService.IsConnected)
        {
            StatusMessage = "设备未连接";
            return;
        }

        CurrentMacro.Name = CurrentMacroName;
        CurrentMacro.Actions.Clear();
        foreach (var item in Actions)
        {
            CurrentMacro.Actions.Add(new MacroAction
            {
                Type = item.Type,
                KeyName = item.KeyName,
                KeyCode = item.KeyCode,
                DelayMs = item.DelayMs
            });
        }

        byte id = (byte)Math.Max(0, Macros.IndexOf(CurrentMacro));
        bool ok = await _deviceService.SetMacroAsync(id, MacroCodec.Serialize(id, CurrentMacro));
        StatusMessage = ok ? "宏已保存" : "宏保存失败";
    }

    [RelayCommand]
    private async Task PlayMacroAsync()
    {
        if (CurrentMacro == null || !_deviceService.IsConnected)
            return;
        byte id = (byte)Math.Max(0, Macros.IndexOf(CurrentMacro));
        bool ok = await _deviceService.PlayMacroAsync(id);
        StatusMessage = ok ? "已发送播放命令" : "播放失败";
    }

    [RelayCommand]
    private async Task StopMacroAsync()
    {
        await _deviceService.StopMacroAsync(0xFF);
        StatusMessage = "已停止宏";
    }

    [RelayCommand]
    private async Task RefreshMacrosAsync() => await LoadMacrosFromDeviceAsync();

    private async Task LoadMacrosFromDeviceAsync()
    {
        if (!_deviceService.IsConnected)
            return;

        IsLoading = true;
        try
        {
            var loaded = new List<Macro>();
            for (byte i = 0; i < 8; i++)
            {
                byte[]? data = await _deviceService.GetMacroAsync(i);
                loaded.Add(data != null ? MacroCodec.Parse(i, data) : new Macro { Name = $"宏 {i + 1}" });
            }

            Macros.Clear();
            foreach (var macro in loaded)
                Macros.Add(macro);
            CurrentMacro = Macros[0];
        }
        catch (Exception ex)
        {
            StatusMessage = $"加载宏失败: {ex.Message}";
        }
        finally
        {
            IsLoading = false;
        }
    }

    private void LoadActions(Macro macro)
    {
        Actions.Clear();
        int i = 1;
        foreach (var action in macro.Actions)
        {
            Actions.Add(new MacroActionItemViewModel
            {
                Type = action.Type,
                KeyCode = (byte)action.KeyCode,
                KeyName = action.KeyName,
                DelayMs = action.DelayMs,
                Index = i++
            });
        }
    }
}
