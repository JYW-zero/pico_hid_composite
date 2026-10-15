using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.ViewModels;

/// <summary>
/// 按键设置页面视图模型
/// </summary>
public partial class KeyPageViewModel : PageViewModelBase
{
    private readonly IDialogService _dialogService;
    private readonly IKeyPickerService _keyPickerService;

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
    /// 当前编辑层：0=普通层，1=Fn层（仅用于UI编辑显示）
    /// </summary>
    [ObservableProperty]
    private int _currentLayer;

    /// <summary>
    /// 当前设备激活层（0-3），通过 CMD_SWITCH_LAYER 实际切换设备当前生效的层
    /// 与编辑层（CurrentLayer）分离：编辑层用于修改不同层的映射，激活层控制设备当前使用哪层
    /// </summary>
    [ObservableProperty]
    private int _currentActiveLayer;

    /// <summary>
    /// 是否正在切换激活层
    /// </summary>
    [ObservableProperty]
    private bool _isApplyingLayer;

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

    public KeyPageViewModel(IDeviceService deviceService, IDialogService dialogService, IKeyPickerService keyPickerService)
        : base(deviceService)
    {
        _dialogService = dialogService;
        _keyPickerService = keyPickerService;

        // 初始化 8x8 网格的 64 个键
        InitializeKeys();

        // 初始化双手手指布局
        InitializeFingers();

        // 加载配置
        LoadConfigFromDevice();

        // 刷新显示
        RefreshKeys();
    }

    /// <summary>
    /// 设备已连接（重写基类虚方法）
    /// </summary>
    protected override Task OnDeviceConnectedAsync()
    {
        LoadConfigFromDevice();
        RefreshKeys();
        return Task.CompletedTask;
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
        if (!DeviceService.IsConnected || DeviceService.CurrentConfig == null)
            return;

        var config = DeviceService.CurrentConfig;

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

    /// <summary>
    /// 切换设备当前激活层（实际发送 CMD_SWITCH_LAYER 到固件）
    /// 与编辑层 SwitchLayer 分离：编辑层只切换UI显示，本命令实际切换设备生效层
    /// </summary>
    [RelayCommand]
    private async Task ApplyActiveLayerAsync(string layer)
    {
        if (!DeviceService.IsConnected || IsApplyingLayer)
            return;

        if (!int.TryParse(layer, out int layerIndex) || layerIndex < 0 || layerIndex > 3)
            return;

        // 已是当前层，无需切换
        if (layerIndex == CurrentActiveLayer)
            return;

        // 添加确认对话框，防止误触
        if (!_dialogService.ShowConfirm($"确定要切换到 Layer {layerIndex} 吗？\n切换后设备将立即使用该层的按键映射。", "确认切换层"))
            return;

        try
        {
            IsApplyingLayer = true;

            bool result = await DeviceService.SwitchLayerAsync((byte)layerIndex);
            if (result)
            {
                CurrentActiveLayer = layerIndex;
            }
            else
            {
                _dialogService.ShowError($"切换到层 {layerIndex} 失败", "错误");
            }
        }
        catch (Exception ex)
        {
            _dialogService.ShowError($"切换层异常: {ex.Message}", "错误");
        }
        finally
        {
            IsApplyingLayer = false;
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

        var result = _keyPickerService.ShowKeyPicker(key.KeyCode);
        if (result.HasValue)
        {
            key.KeyCode = result.Value.KeyCode;
            key.KeyName = result.Value.KeyName;

            // 同时更新本地映射数组
            byte[] keymap = CurrentLayer == 0 ? _normalKeymap : _fnKeymap;
            if (key.Index < keymap.Length)
            {
                keymap[key.Index] = result.Value.KeyCode;
            }
        }
    }

    [RelayCommand]
    private async Task SaveToDeviceAsync()
    {
        if (!DeviceService.IsConnected || DeviceService.CurrentConfig == null)
        {
            return;
        }

        try
        {
            var config = DeviceService.CurrentConfig;

            // 把本地映射写回配置
            for (int i = 0; i < 64 && i < config.Keymap.Length; i++)
                config.Keymap[i] = _normalKeymap[i];
            for (int i = 0; i < 64 && i < config.FnKeymap.Length; i++)
                config.FnKeymap[i] = _fnKeymap[i];

            await DeviceService.SaveConfigAsync(config);
        }
        catch (Exception)
        {
            // 错误状态已经通过状态栏显示了
        }
    }

    [RelayCommand]
    private async Task ResetDefaultAsync()
    {
        if (!DeviceService.IsConnected)
        {
            _dialogService.ShowInfo("请先连接设备", "提示");
            return;
        }

        if (_dialogService.ShowConfirm("确定要恢复默认按键映射吗？\n此操作将重置所有配置（包括按键映射、DPI、死区等）。", "确认重置"))
        {
            bool result = await DeviceService.ResetConfigAsync();
            if (result)
            {
                LoadConfigFromDevice();
                RefreshKeys();
                _dialogService.ShowInfo("已恢复默认配置", "成功");
            }
            else
            {
                _dialogService.ShowInfo("恢复默认配置失败，请重试", "错误");
            }
        }
    }

    /// <summary>
    /// 获取键名（简化版，常用键）
    /// </summary>
    private static string GetKeyName(byte keyCode)
    {
        if (keyCode == 0x00)
            return "None";
        return HidKeyConverter.HidUsageToName(keyCode);
    }

}
