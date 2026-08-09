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
    private int _keyCode;

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

    /// <summary>
    /// 是否正在录制
    /// </summary>
    [ObservableProperty]
    private bool _isRecording;

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

        // 初始化宏列表（先添加占位符，后面从设备加载）
        for (int i = 0; i < 8; i++)
        {
            Macros.Add(new Macro { Name = $"宏 {i + 1}" });
        }
        CurrentMacro = Macros[0];
        CurrentMacroName = CurrentMacro.Name;

        // 从设备加载宏
        _ = LoadMacrosFromDeviceAsync();
    }

    [RelayCommand]
    private void NewMacro()
    {
        CurrentMacroName = "新建宏";
        Actions.Clear();
        UpdateTotalDuration();
    }

    [RelayCommand]
    private void DeleteMacro()
    {
        if (CurrentMacro != null && Macros.Contains(CurrentMacro))
        {
            Macros.Remove(CurrentMacro);
            if (Macros.Count > 0)
            {
                CurrentMacro = Macros[0];
                CurrentMacroName = CurrentMacro.Name;
                LoadActionsFromMacro(CurrentMacro);
            }
        }
    }

    [RelayCommand]
    private void AddKeyAction()
    {
        Actions.Add(new MacroActionItemViewModel
        {
            Type = MacroActionType.KeyDown,
            KeyName = "A",
            KeyCode = 65,
            DelayMs = 0
        });
        UpdateTotalDuration();
    }

    [RelayCommand]
    private void AddDelayAction()
    {
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
            Actions.Remove(action);
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
        var actions = _macroRecorder.StopRecording();
        IsRecording = false;
        _recordTimer.Stop();

        // 把录制的动作添加到当前宏
        foreach (var action in actions)
        {
            Actions.Add(new MacroActionItemViewModel
            {
                Type = action.Type,
                KeyName = action.KeyName,
                DelayMs = action.DelayMs,
                KeyCode = action.KeyCode
            });
        }

        UpdateTotalDuration();
    }

    private void OnActionRecorded(object? sender, MacroAction e)
    {
        // 录制过程中实时更新 UI
        Application.Current?.Dispatcher.Invoke(() =>
        {
            Actions.Add(new MacroActionItemViewModel
            {
                Type = e.Type,
                KeyName = e.KeyName,
                DelayMs = e.DelayMs,
                KeyCode = e.KeyCode
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
    private void PlayMacro()
    {
        // 播放宏的逻辑
    }

    [RelayCommand]
    private void StopPlayback()
    {
        // 停止播放的逻辑
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
                KeyName = action.KeyName,
                KeyCode = action.KeyCode,
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

    #endregion
}
