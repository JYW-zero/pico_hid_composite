using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Desktop.ViewModels;

public partial class KeyItemViewModel : ObservableObject
{
    public int Index { get; set; }
    [ObservableProperty] private byte _keyCode;
    [ObservableProperty] private string _keyName = "None";
}

public partial class KeyPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private readonly byte[] _normalKeymap = new byte[64];
    private readonly byte[] _fnKeymap = new byte[64];

    public ObservableCollection<KeyItemViewModel> Keys { get; } = new();
    public IReadOnlyList<KeyDefinition> AllKeys { get; } = KeyDefinitions.GetAll();

    [ObservableProperty] private int _currentLayer;
    [ObservableProperty] private KeyItemViewModel? _selectedKey;
    [ObservableProperty] private string _statusMessage = string.Empty;
    [ObservableProperty] private KeyDefinition? _pickedKey;

    public KeyPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;
        for (int i = 0; i < 64; i++)
            Keys.Add(new KeyItemViewModel { Index = i, KeyName = "None" });

        LoadFromDevice();
        RefreshKeys();
    }

    partial void OnCurrentLayerChanged(int value) => RefreshKeys();

    partial void OnPickedKeyChanged(KeyDefinition? value)
    {
        if (SelectedKey == null || value == null)
            return;
        SelectedKey.KeyCode = value.KeyCode;
        SelectedKey.KeyName = value.Name;
        byte[] map = CurrentLayer == 0 ? _normalKeymap : _fnKeymap;
        if (SelectedKey.Index < map.Length)
            map[SelectedKey.Index] = value.KeyCode;
    }

    [RelayCommand]
    private void SwitchLayer(string layer)
    {
        if (int.TryParse(layer, out int index))
            CurrentLayer = index;
    }

    [RelayCommand]
    private void SelectKey(KeyItemViewModel? key) => SelectedKey = key;

    [RelayCommand]
    private async Task SaveToDeviceAsync()
    {
        if (!_deviceService.IsConnected || _deviceService.CurrentConfig == null)
        {
            StatusMessage = "设备未连接";
            return;
        }

        var config = _deviceService.CurrentConfig;
        Array.Copy(_normalKeymap, config.Keymap, 64);
        Array.Copy(_fnKeymap, config.FnKeymap, 64);
        bool ok = await _deviceService.SaveConfigAsync(config);
        StatusMessage = ok ? "按键映射已保存" : "保存失败";
    }

    private void LoadFromDevice()
    {
        var config = _deviceService.CurrentConfig;
        if (config == null)
            return;
        Array.Copy(config.Keymap, _normalKeymap, Math.Min(64, config.Keymap.Length));
        Array.Copy(config.FnKeymap, _fnKeymap, Math.Min(64, config.FnKeymap.Length));
    }

    private void RefreshKeys()
    {
        byte[] map = CurrentLayer == 0 ? _normalKeymap : _fnKeymap;
        for (int i = 0; i < 64; i++)
        {
            Keys[i].KeyCode = map[i];
            Keys[i].KeyName = HidUsageNames.ToName(map[i]);
        }
    }
}
