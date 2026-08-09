using System.Collections.ObjectModel;
using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.App.Views;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 按键设置页面视图模型
/// </summary>
public partial class KeyPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;

    /// <summary>
    /// 所有按键
    /// </summary>
    public ObservableCollection<KeyItemViewModel> Keys { get; } = new();

    /// <summary>
    /// 左手手指
    /// </summary>
    public ObservableCollection<FingerViewModel> LeftHandFingers { get; } = new();

    /// <summary>
    /// 右手手指
    /// </summary>
    public ObservableCollection<FingerViewModel> RightHandFingers { get; } = new();

    /// <summary>
    /// 功能键
    /// </summary>
    public ObservableCollection<KeyItemViewModel> FunctionKeys { get; } = new();

    /// <summary>
    /// 当前选中的手：Left / Right
    /// </summary>
    [ObservableProperty]
    private string _selectedHand = "Left";

    /// <summary>
    /// 当前层：0=普通层，1=Fn层
    /// </summary>
    [ObservableProperty]
    private int _currentLayer;

    /// <summary>
    /// 当前选中的键
    /// </summary>
    [ObservableProperty]
    private KeyItemViewModel? _selectedKey;

    /// <summary>
    /// 普通层按键映射（本地维护）
    /// </summary>
    private byte[] _normalKeymap = new byte[64];

    /// <summary>
    /// Fn层按键映射（本地维护）
    /// </summary>
    private byte[] _fnKeymap = new byte[64];

    public KeyPageViewModel(IDeviceService deviceService)
    {
        _deviceService = deviceService;

        // 初始化 8x8 网格的 64 个键
        InitializeKeys();

        // 初始化双手手指布局
        InitializeFingers();

        // 加载配置
        LoadConfigFromDevice();

        // 刷新显示
        RefreshKeys();
    }

    private void InitializeKeys()
    {
        Keys.Clear();

        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                int index = row * 8 + col;
                var key = new KeyItemViewModel
                {
                    Index = index,
                    Row = row,
                    Column = col,
                    KeyCode = 0,
                    KeyName = GetKeyName(0)
                };
                Keys.Add(key);
            }
        }
    }

    /// <summary>
    /// 初始化双手手指布局
    /// 注意：键位索引映射需要根据实际硬件调整
    /// </summary>
    private void InitializeFingers()
    {
        LeftHandFingers.Clear();
        RightHandFingers.Clear();
        FunctionKeys.Clear();

        // 手指名称（从上到下：拇指、食指、中指、无名指、小指）
        string[] fingerNames = { "拇指", "食指", "中指", "无名指", "小指" };

        // 初始化左手（顺序反过来：拇指在上）
        for (int i = 0; i < 5; i++)
        {
            int baseIndex = (4 - i) * 6; // 拇指对应索引24-29，小指对应0-5
            var finger = new FingerViewModel
            {
                Name = fingerNames[i],
                Hand = "Left",
                FingerIndex = i,
                Up = Keys[baseIndex + 0],    // +Y 上
                Down = Keys[baseIndex + 1],  // -Y 下
                Left = Keys[baseIndex + 2],  // -X 左
                Right = Keys[baseIndex + 3], // +X 右
                Front = Keys[baseIndex + 4], // -Z 前
                Back = Keys[baseIndex + 5]   // +Z 后
            };
            LeftHandFingers.Add(finger);
        }

        // 初始化右手（顺序反过来：拇指在上）
        for (int i = 0; i < 5; i++)
        {
            int baseIndex = 30 + (4 - i) * 6; // 拇指对应54-59，小指对应30-35
            var finger = new FingerViewModel
            {
                Name = fingerNames[i],
                Hand = "Right",
                FingerIndex = i,
                Up = Keys[baseIndex + 0],    // +Y 上
                Down = Keys[baseIndex + 1],  // -Y 下
                Left = Keys[baseIndex + 2],  // -X 左
                Right = Keys[baseIndex + 3], // +X 右
                Front = Keys[baseIndex + 4], // -Z 前
                Back = Keys[baseIndex + 5]   // +Z 后
            };
            RightHandFingers.Add(finger);
        }

        // 初始化功能键（索引 60-63）
        string[] funcKeyNames = { "功能键1", "功能键2", "功能键3", "功能键4" };
        for (int i = 0; i < 4; i++)
        {
            Keys[60 + i].KeyName = funcKeyNames[i];
            FunctionKeys.Add(Keys[60 + i]);
        }
    }

    /// <summary>
    /// 选中手
    /// </summary>
    [RelayCommand]
    private void SelectHand(string hand)
    {
        SelectedHand = hand;
    }

    /// <summary>
    /// 从设备加载配置到本地映射数组
    /// </summary>
    private void LoadConfigFromDevice()
    {
        if (!_deviceService.IsConnected || _deviceService.CurrentConfig == null)
            return;

        var config = _deviceService.CurrentConfig;

        // 复制普通层
        for (int i = 0; i < 64 && i < config.Keymap.Length; i++)
            _normalKeymap[i] = config.Keymap[i];

        // 复制Fn层
        for (int i = 0; i < 64 && i < config.FnKeymap.Length; i++)
            _fnKeymap[i] = config.FnKeymap[i];
    }

    /// <summary>
    /// 根据当前层刷新按键显示
    /// </summary>
    private void RefreshKeys()
    {
        byte[] keymap = CurrentLayer == 0 ? _normalKeymap : _fnKeymap;

        for (int i = 0; i < 64 && i < Keys.Count; i++)
        {
            Keys[i].KeyCode = keymap[i];
            Keys[i].KeyName = GetKeyName(keymap[i]);
            Keys[i].IsFnLayer = CurrentLayer == 1;
        }
    }

    partial void OnCurrentLayerChanged(int value)
    {
        RefreshKeys();
    }


    [RelayCommand]
    private void SwitchLayer(string layer)
    {
        if (int.TryParse(layer, out int layerIndex))
        {
            CurrentLayer = layerIndex;
        }
    }
    [RelayCommand]
    private void SelectKey(KeyItemViewModel? key)
    {
        SelectedKey = key;
    }

    [RelayCommand]
    private void EditKey(KeyItemViewModel? key)
    {
        if (key == null)
            return;

        var dialog = new KeyPickerDialog(key.KeyCode)
        {
            Owner = System.Windows.Application.Current.MainWindow
        };

        if (dialog.ShowDialog() == true)
        {
            key.KeyCode = dialog.SelectedKeyCode;
            key.KeyName = dialog.SelectedKeyName;

            // 同时更新本地映射数组
            byte[] keymap = CurrentLayer == 0 ? _normalKeymap : _fnKeymap;
            if (key.Index < keymap.Length)
            {
                keymap[key.Index] = dialog.SelectedKeyCode;
            }
        }
    }

    [RelayCommand]
    private async Task SaveToDeviceAsync()
    {
        if (!_deviceService.IsConnected || _deviceService.CurrentConfig == null)
        {
            return;
        }

        try
        {
            var config = _deviceService.CurrentConfig;

            // 把本地映射写回配置
            for (int i = 0; i < 64 && i < config.Keymap.Length; i++)
                config.Keymap[i] = _normalKeymap[i];
            for (int i = 0; i < 64 && i < config.FnKeymap.Length; i++)
                config.FnKeymap[i] = _fnKeymap[i];

            await _deviceService.SaveConfigAsync(config);
        }
        catch (Exception)
        {
            // 错误状态已经通过状态栏显示了
        }
    }

    [RelayCommand]
    private void ResetDefault()
    {
        var result = MessageBox.Show("确定要恢复默认按键映射吗？", "确认重置",
            MessageBoxButton.YesNo, MessageBoxImage.Question);

        if (result == MessageBoxResult.Yes)
        {
            // TODO: 加载默认映射
            MessageBox.Show("功能开发中...", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
        }
    }

    /// <summary>
    /// 获取键名（简化版，常用键）
    /// </summary>
    private static string GetKeyName(byte keyCode)
    {
        return keyCode switch
        {
            0x00 => "None",
            0x04 => "A",
            0x05 => "B",
            0x06 => "C",
            0x07 => "D",
            0x08 => "E",
            0x09 => "F",
            0x0A => "G",
            0x0B => "H",
            0x0C => "I",
            0x0D => "J",
            0x0E => "K",
            0x0F => "L",
            0x10 => "M",
            0x11 => "N",
            0x12 => "O",
            0x13 => "P",
            0x14 => "Q",
            0x15 => "R",
            0x16 => "S",
            0x17 => "T",
            0x18 => "U",
            0x19 => "V",
            0x1A => "W",
            0x1B => "X",
            0x1C => "Y",
            0x1D => "Z",
            0x1E => "1",
            0x1F => "2",
            0x20 => "3",
            0x21 => "4",
            0x22 => "5",
            0x23 => "6",
            0x24 => "7",
            0x25 => "8",
            0x26 => "9",
            0x27 => "0",
            0x28 => "Enter",
            0x29 => "Esc",
            0x2A => "Backspace",
            0x2B => "Tab",
            0x2C => "Space",
            0x2D => "-",
            0x2E => "=",
            0x2F => "[",
            0x30 => "]",
            0x31 => "\\",
            0x33 => ";",
            0x34 => "'",
            0x35 => "`",
            0x36 => ",",
            0x37 => ".",
            0x38 => "/",
            0x39 => "Caps Lock",
            0x3A => "F1",
            0x3B => "F2",
            0x3C => "F3",
            0x3D => "F4",
            0x3E => "F5",
            0x3F => "F6",
            0x40 => "F7",
            0x41 => "F8",
            0x42 => "F9",
            0x43 => "F10",
            0x44 => "F11",
            0x45 => "F12",
            0x49 => "Insert",
            0x4A => "Home",
            0x4B => "Page Up",
            0x4C => "Delete",
            0x4D => "End",
            0x4E => "Page Down",
            0x4F => "Right",
            0x50 => "Left",
            0x51 => "Down",
            0x52 => "Up",
            0xE0 => "Ctrl L",
            0xE1 => "Shift L",
            0xE2 => "Alt L",
            0xE3 => "GUI L",
            0xE4 => "Ctrl R",
            0xE5 => "Shift R",
            0xE6 => "Alt R",
            0xE7 => "GUI R",
            _ => $"0x{keyCode:X2}"
        };
    }
}
