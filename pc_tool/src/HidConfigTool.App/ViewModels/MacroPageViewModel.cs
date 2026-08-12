using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using HidConfigTool.Core.Models;
using HidConfigTool.Core.Interfaces;
using HidConfigTool.App.Services;

namespace HidConfigTool.App.ViewModels;

/// <summary>
/// 宏动作项视图模型
/// </summary>
public partial class MacroActionItemViewModel : ObservableObject
{
    [ObservableProperty]
    private MacroActionType _type;

    [ObservableProperty]
    private string _keyName = string.Empty;

    [ObservableProperty]
    private int _delayMs;

    [ObservableProperty]
    private byte _keyCode;

    [ObservableProperty]
    private int _index;

    /// <summary>
    /// 对应的按键定义（用于下拉框绑定）
    /// </summary>
    public KeyDefinition? KeyDefinition
    {
        get => KeyDefinitions.GetAll().FirstOrDefault(k => k.Name == KeyName);
        set
        {
            if (value != null)
            {
                KeyCode = value.KeyCode;
                KeyName = value.Name;
            }
        }
    }

    partial void OnKeyCodeChanged(byte value)
    {
        OnPropertyChanged(nameof(KeyDefinition));
    }

    partial void OnKeyNameChanged(string value)
    {
        // 根据按键名称同步更新 HID 用法码
        var keyDef = KeyDefinitions.GetAll().FirstOrDefault(k => k.Name == value);
        if (keyDef != null)
        {
            KeyCode = keyDef.KeyCode;
        }
        OnPropertyChanged(nameof(KeyDefinition));
    }

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
        MacroActionType.KeyDown or MacroActionType.KeyUp => KeyName,
        MacroActionType.Delay => $"{DelayMs} ms",
        MacroActionType.MouseMove => $"X:{KeyCode}, Y:{DelayMs}",
        _ => KeyName
    };
}

public partial class MacroPageViewModel : ObservableObject
{
    private readonly IDeviceService _deviceService;
    private readonly MacroRecorder _macroRecorder;
    private readonly DispatcherTimer _recordTimer;
    private DateTime _recordStartTime;

    /// <summary>
    /// 每个宏最大动作数（固件限制）
    /// </summary>
    private const int MaxActions = 32;

    /// <summary>
    /// 宏列表
    /// </summary>
    public ObservableCollection<Macro> Macros { get; } = new();

    /// <summary>
    /// 当前选中的宏
    /// </summary>
    [ObservableProperty]
    private Macro? _currentMacro;

    /// <summary>
    /// 当前选中的宏名称
    /// </summary>
    [ObservableProperty]
    private string _currentMacroName = "新建宏";

    partial void OnCurrentMacroChanged(Macro? value)
    {
        if (value != null)
        {
            CurrentMacroName = value.Name;
            LoadActionsFromMacro(value);
        }
    }

    /// <summary>
    /// 是否正在录制
    /// </summary>
    [ObservableProperty]
    private bool _isRecording;

    /// <summary>
    /// 是否正在播放宏
    /// </summary>
    [ObservableProperty]
    private bool _isPlaying;

    /// <summary>
    /// 状态消息
    /// </summary>
    [ObservableProperty]
    private string _statusMessage = string.Empty;

    /// <summary>
    /// 录制时长
    /// </summary>
    [ObservableProperty]
    private string _recordDuration = "00:00.0";

    /// <summary>
    /// 动作列表
    /// </summary>
    public ObservableCollection<MacroActionItemViewModel> Actions { get; } = new();

    /// <summary>
    /// 当前选中的动作
    /// </summary>
    [ObservableProperty]
    private MacroActionItemViewModel? _selectedAction;

    /// <summary>
    /// 总时长
    /// </summary>
    [ObservableProperty]
    private string _totalDuration = "0 ms";

    /// <summary>
    /// 是否正在加载
    /// </summary>
    [ObservableProperty]
    private bool _isLoading;

    /// <summary>
    /// 常用按键列表
    /// </summary>
    public List<KeyDefinition> CommonKeys { get; } = KeyDefinitions.GetAll();

    public MacroPageViewModel(IDeviceService deviceService, MacroRecorder macroRecorder)
    {
        _deviceService = deviceService;
        _macroRecorder = macroRecorder;
        _macroRecorder.ActionRecorded += OnActionRecorded;

        _recordTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(100)
        };
        _recordTimer.Tick += OnRecordTimerTick;

        // 监听动作列表变化，更新索引
        Actions.CollectionChanged += (s, e) => UpdateActionIndexes();

        // 初始化8个固定宏槽位（固件限制 MACRO_MAX_COUNT=8）
        for (int i = 0; i < 8; i++)
        {
            Macros.Add(new Macro { Name = "未配置" });
        }
        CurrentMacro = Macros[0];
        CurrentMacroName = CurrentMacro.Name;

        // 从设备加载宏
        _ = LoadMacrosFromDeviceAsync();
    }

    /// <summary>
    /// 更新所有动作的索引
    /// </summary>
    private void UpdateActionIndexes()
    {
        for (int i = 0; i < Actions.Count; i++)
        {
            Actions[i].Index = i + 1; // 从1开始
        }
    }

    [RelayCommand]
    private void NewMacro()
    {
        CurrentMacroName = "新建宏";
        Actions.Clear();
        UpdateTotalDuration();
    }

    [RelayCommand]
    private void ClearMacro()
    {
        if (CurrentMacro != null)
        {
            var result = MessageBox.Show("确定要清空当前宏的所有动作吗？", "确认清空", MessageBoxButton.YesNo, MessageBoxImage.Question);
            if (result == MessageBoxResult.Yes)
            {
                CurrentMacro.Actions.Clear();
                CurrentMacro.Name = "未配置";
                CurrentMacroName = "未配置";
                Actions.Clear();
                SelectedAction = null;
                UpdateTotalDuration();
            }
        }
    }

    [RelayCommand]
    private void AddKeyAction()
    {
        if (Actions.Count >= MaxActions)
        {
            MessageBox.Show($"最多只能添加 {MaxActions} 个动作", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        Actions.Add(new MacroActionItemViewModel
        {
            Type = MacroActionType.KeyDown,
            KeyCode = 0x04,  // HID Usage 码：字母 A
            KeyName = "A",
            DelayMs = 0
        });
        UpdateTotalDuration();
    }

    [RelayCommand]
    private void AddDelayAction()
    {
        if (Actions.Count >= MaxActions)
        {
            MessageBox.Show($"最多只能添加 {MaxActions} 个动作", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        Actions.Add(new MacroActionItemViewModel
        {
            Type = MacroActionType.Delay,
            DelayMs = 100
        });
        UpdateTotalDuration();
    }

    [RelayCommand]
    private void DeleteAction(MacroActionItemViewModel? action)
    {
        if (action != null)
        {
            int index = Actions.IndexOf(action);
            Actions.Remove(action);

            // 自动选中下一个或上一个动作
            if (Actions.Count > 0)
            {
                if (index < Actions.Count)
                {
                    SelectedAction = Actions[index]; // 选中下一个
                }
                else
                {
                    SelectedAction = Actions[Actions.Count - 1]; // 选中上一个（最后一个）
                }
            }
            else
            {
                SelectedAction = null;
            }

            UpdateTotalDuration();
        }
    }

    [RelayCommand]
    private void ClearActions()
    {
        if (Actions.Count == 0) return;

        var result = MessageBox.Show("确定要清空所有动作吗？", "确认", MessageBoxButton.YesNo, MessageBoxImage.Question);
        if (result == MessageBoxResult.Yes)
        {
            Actions.Clear();
            SelectedAction = null;
            UpdateTotalDuration();
        }
    }

    [RelayCommand]
    private void MoveUpAction(MacroActionItemViewModel? action)
    {
        if (action == null) return;
        int index = Actions.IndexOf(action);
        if (index > 0)
        {
            Actions.Move(index, index - 1);
        }
    }

    [RelayCommand]
    private void MoveDownAction(MacroActionItemViewModel? action)
    {
        if (action == null) return;
        int index = Actions.IndexOf(action);
        if (index < Actions.Count - 1)
        {
            Actions.Move(index, index + 1);
        }
    }

    [RelayCommand]
    private void StartRecording()
    {
        if (_macroRecorder.StartRecording())
        {
            IsRecording = true;
            _recordStartTime = DateTime.Now;
            _recordTimer.Start();
        }
    }

    [RelayCommand]
    private void StopRecording()
    {
        _macroRecorder.StopRecording();
        IsRecording = false;
        _recordTimer.Stop();

        // 注意：动作已在 OnActionRecorded 事件中实时添加，这里不需要重复添加
        UpdateTotalDuration();
    }

    private void OnActionRecorded(object? sender, MacroAction e)
    {
        // 录制过程中实时更新 UI
        Application.Current?.Dispatcher.Invoke(() =>
        {
            if (Actions.Count >= MaxActions)
            {
                // 达到上限，自动停止录制
                StopRecording();
                MessageBox.Show($"已达到最大动作数 {MaxActions}，录制已自动停止", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            Actions.Add(new MacroActionItemViewModel
            {
                Type = e.Type,
                KeyCode = (byte)e.KeyCode,
                KeyName = e.KeyName,
                DelayMs = e.DelayMs
            });
            UpdateTotalDuration();
        });
    }

    private void OnRecordTimerTick(object? sender, EventArgs e)
    {
        var duration = DateTime.Now - _recordStartTime;
        RecordDuration = duration.ToString(@"mm\:ss\.f");
    }

    private void UpdateTotalDuration()
    {
        int total = 0;
        foreach (var action in Actions)
        {
            total += action.DelayMs;
        }
        TotalDuration = $"{total} ms";
    }

    [RelayCommand]
    private async Task PlayMacro()
    {
        if (CurrentMacro == null || !_deviceService.IsConnected)
        {
            MessageBox.Show("设备未连接或未选择宏", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        byte macroId = (byte)Macros.IndexOf(CurrentMacro);
        if (macroId >= 8)
        {
            MessageBox.Show("无效的宏ID", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            return;
        }

        // 先保存当前宏到设备
        await SaveCurrentMacroAsync();

        // 发送播放命令
        bool result = await _deviceService.PlayMacroAsync(macroId);
        if (result)
        {
            IsPlaying = true;
            StatusMessage = $"正在播放宏: {CurrentMacro.Name}";
        }
        else
        {
            MessageBox.Show("播放宏失败", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    [RelayCommand]
    private async Task StopPlayback()
    {
        if (!_deviceService.IsConnected)
            return;

        byte macroId = CurrentMacro != null ? (byte)Macros.IndexOf(CurrentMacro) : (byte)0xFF;
        bool result = await _deviceService.StopMacroAsync(macroId);
        if (result)
        {
            IsPlaying = false;
            StatusMessage = "已停止播放";
        }
    }

    #region 设备对接

    /// <summary>
    /// 从设备加载所有宏
    /// </summary>
    private async Task LoadMacrosFromDeviceAsync()
    {
        if (!_deviceService.IsConnected)
        {
            return;
        }

        IsLoading = true;
        try
        {
            Macros.Clear();

            for (byte i = 0; i < 8; i++)
            {
                byte[]? data = await _deviceService.GetMacroAsync(i);
                if (data != null && data.Length >= 20)
                {
                    var macro = ParseMacroData(i, data);
                    Macros.Add(macro);
                }
                else
                {
                    Macros.Add(new Macro { Name = $"宏 {i + 1}" });
                }
            }

            if (Macros.Count > 0)
            {
                CurrentMacro = Macros[0];
                CurrentMacroName = CurrentMacro.Name;
                LoadActionsFromMacro(CurrentMacro);
            }
        }
        catch (Exception ex)
        {
            MessageBox.Show($"加载宏失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
        }
        finally
        {
            IsLoading = false;
        }
    }

    /// <summary>
    /// 解析宏数据（固件格式）
    /// </summary>
    private Macro ParseMacroData(byte id, byte[] data)
    {
        var macro = new Macro { Id = id.ToString() };

        try
        {
            // 固件格式:
            // 偏移0: id (uint8)
            // 偏移1: trigger_key (uint8)
            // 偏移2: repeat_count (uint8)
            // 偏移3: action_count (uint8)
            // 偏移4-19: name (char[16])
            // 偏移20-147: actions (32个 * 4字节)

            if (data.Length >= 20)
            {
                macro.RepeatCount = data[2];
                int actionCount = data[3];

                // 读取名称（16字节，UTF8）
                int nameLen = 0;
                for (int i = 0; i < 16; i++)
                {
                    if (data[4 + i] == 0) break;
                    nameLen++;
                }
                macro.Name = System.Text.Encoding.UTF8.GetString(data, 4, nameLen);
                if (string.IsNullOrEmpty(macro.Name))
                {
                    macro.Name = $"宏 {id + 1}";
                }

                // 读取动作
                macro.Actions.Clear();
                for (int i = 0; i < actionCount && i < 32; i++)
                {
                    int offset = 20 + i * 4;
                    if (offset + 4 > data.Length) break;

                    var action = new MacroAction
                    {
                        Type = (MacroActionType)data[offset],
                        KeyCode = data[offset + 1],
                        DelayMs = data[offset + 2] | (data[offset + 3] << 8)
                    };
                    action.KeyName = GetKeyName((byte)action.KeyCode);
                    macro.Actions.Add(action);
                }
            }
        }
        catch
        {
            // 解析失败，使用默认值
        }

        return macro;
    }

    /// <summary>
    /// 把宏的动作加载到UI
    /// </summary>
    private void LoadActionsFromMacro(Macro macro)
    {
        Actions.Clear();
        foreach (var action in macro.Actions)
        {
            Actions.Add(new MacroActionItemViewModel
            {
                Type = action.Type,
                KeyCode = (byte)action.KeyCode,
                KeyName = action.KeyName,
                DelayMs = action.DelayMs
            });
        }
        UpdateTotalDuration();
    }

    /// <summary>
    /// 保存当前宏到设备
    /// </summary>
    [RelayCommand]
    private async Task SaveCurrentMacroAsync()
    {
        if (CurrentMacro == null || !_deviceService.IsConnected)
        {
            MessageBox.Show("设备未连接", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        try
        {
            // 更新当前宏的名称和动作
            CurrentMacro.Name = CurrentMacroName;
            CurrentMacro.Actions.Clear();
            foreach (var actionVm in Actions)
            {
                CurrentMacro.Actions.Add(new MacroAction
                {
                    Type = actionVm.Type,
                    KeyName = actionVm.KeyName,
                    KeyCode = actionVm.KeyCode,
                    DelayMs = actionVm.DelayMs
                });
            }

            // 序列化为固件格式
            byte[] data = SerializeMacroData(CurrentMacro);

            // 获取宏ID
            byte macroId = (byte)Macros.IndexOf(CurrentMacro);
            if (macroId >= 8) macroId = 0;

            // 写入设备
            bool ok = await _deviceService.SetMacroAsync(macroId, data);
            if (ok)
            {
                MessageBox.Show("保存成功！", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            else
            {
                MessageBox.Show("保存失败", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
        catch (Exception ex)
        {
            MessageBox.Show($"保存失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    /// <summary>
    /// 把宏序列化为固件格式
    /// </summary>
    private byte[] SerializeMacroData(Macro macro)
    {
        byte[] data = new byte[148];

        try
        {
            // 偏移0: id (uint8) - 固件端会处理
            data[0] = 0;
            // 偏移1: trigger_key (uint8) - 暂不支持
            data[1] = 0;
            // 偏移2: repeat_count (uint8)
            data[2] = (byte)Math.Min(macro.RepeatCount, 255);
            // 偏移3: action_count (uint8)
            int actionCount = Math.Min(macro.Actions.Count, 32);
            data[3] = (byte)actionCount;

            // 偏移4-19: name (char[16])
            byte[] nameBytes = System.Text.Encoding.UTF8.GetBytes(macro.Name);
            int nameLen = Math.Min(nameBytes.Length, 15);
            Array.Copy(nameBytes, 0, data, 4, nameLen);
            data[4 + nameLen] = 0;

            // 偏移20-147: actions (32个 * 4字节)
            for (int i = 0; i < actionCount; i++)
            {
                int offset = 20 + i * 4;
                var action = macro.Actions[i];
                data[offset] = (byte)action.Type;
                data[offset + 1] = (byte)action.KeyCode;
                data[offset + 2] = (byte)(action.DelayMs & 0xFF);
                data[offset + 3] = (byte)((action.DelayMs >> 8) & 0xFF);
            }
        }
        catch
        {
            // 序列化失败，返回空数据
        }

        return data;
    }

    /// <summary>
    /// 刷新宏列表（重新从设备加载）
    /// </summary>
    [RelayCommand]
    private async Task RefreshMacrosAsync()
    {
        await LoadMacrosFromDeviceAsync();
    }

    /// <summary>
    /// 获取键名（HID键码转名称）
    /// </summary>
    private static string GetKeyName(byte keyCode)
    {
        if (keyCode == 0x00)
            return "None";
        return HidKeyConverter.HidUsageToName(keyCode);
    }

    #endregion
}
