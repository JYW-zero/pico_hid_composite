using System.Windows.Input;

namespace HidConfigTool.App.Services;

/// <summary>
/// Win32 虚拟键码(VK) → USB HID Usage 码 转换器
/// 固件使用 HID Usage 码，而 Windows 键盘钩子提供 VK 码
/// </summary>
public static class HidKeyConverter
{
    /// <summary>
    /// 将 Win32 虚拟键码转换为 USB HID Usage 码
    /// </summary>
    /// <param name="vkCode">Win32 虚拟键码</param>
    /// <returns>HID Usage 码，未知键返回 0</returns>
    public static byte VirtualKeyToHidUsage(int vkCode)
    {
        // 字母 A-Z: VK 0x41-0x5A → HID 0x04-0x1D
        if (vkCode is >= 0x41 and <= 0x5A)
        {
            return (byte)(0x04 + (vkCode - 0x41));
        }

        // 数字 1-9: VK 0x31-0x39 → HID 0x1E-0x26
        if (vkCode is >= 0x31 and <= 0x39)
        {
            return (byte)(0x1E + (vkCode - 0x31));
        }

        // 数字 0: VK 0x30 → HID 0x27
        if (vkCode == 0x30)
        {
            return 0x27;
        }

        // 功能键 F1-F12: VK 0x70-0x7B → HID 0x3A-0x45
        if (vkCode is >= 0x70 and <= 0x7B)
        {
            return (byte)(0x3A + (vkCode - 0x70));
        }

        // 小键盘数字 0-9: VK 0x60-0x69 → HID 0x62-0x6B
        if (vkCode is >= 0x60 and <= 0x69)
        {
            return (byte)(0x62 + (vkCode - 0x60));
        }

        return vkCode switch
        {
            // 编辑键
            0x0D => 0x28, // Enter
            0x1B => 0x29, // Escape
            0x08 => 0x2A, // Backspace
            0x09 => 0x2B, // Tab
            0x20 => 0x2C, // Space
            0xBD => 0x2D, // OemMinus (-)
            0xBB => 0x2E, // OemPlus (=)
            0xDB => 0x2F, // OemOpenBrackets ([)
            0xDD => 0x30, // OemCloseBrackets (])
            0xDC => 0x31, // OemBackslash (\)
            0xBA => 0x33, // OemSemicolon (;)
            0xDE => 0x34, // OemQuotes (')
            0xC0 => 0x35, // OemTilde (`)
            0xBC => 0x36, // OemComma (,)
            0xBE => 0x37, // OemPeriod (.)
            0xBF => 0x38, // OemQuestion (/)
            0x14 => 0x39, // CapsLock

            // 方向键
            0x26 => 0x52, // Up
            0x28 => 0x51, // Down
            0x25 => 0x50, // Left
            0x27 => 0x4F, // Right

            // 导航键
            0x2D => 0x49, // Insert
            0x24 => 0x4A, // Home
            0x21 => 0x4B, // PageUp
            0x2E => 0x4C, // Delete
            0x23 => 0x4D, // End
            0x22 => 0x4E, // PageDown

            // 修饰键
            0xA2 => 0xE0, // LeftControl
            0xA4 => 0xE2, // LeftAlt
            0xA0 => 0xE1, // LeftShift
            0x5B => 0xE3, // LWin (Left GUI)
            0xA3 => 0xE4, // RightControl
            0xA5 => 0xE6, // RightAlt
            0xA1 => 0xE5, // RightShift
            0x5C => 0xE7, // RWin (Right GUI)

            // 小键盘
            0x6E => 0x6C, // NumPad .
            0x6F => 0x6D, // NumPad /
            0x6A => 0x6E, // NumPad *
            0x6D => 0x6F, // NumPad -
            0x6B => 0x70, // NumPad +
            0x6C => 0x71, // NumPad Enter

            // 其他
            0x13 => 0x47, // ScrollLock
            0x10 => 0xE1, // Shift (默认左Shift)
            0x11 => 0xE0, // Control (默认左Control)
            0x12 => 0xE2, // Alt (默认左Alt)

            _ => 0 // 未知键
        };
    }

    /// <summary>
    /// 将 WPF Key 枚举转换为 USB HID Usage 码
    /// </summary>
    public static byte KeyToHidUsage(Key key)
    {
        int vkCode = KeyInterop.VirtualKeyFromKey(key);
        return VirtualKeyToHidUsage(vkCode);
    }

    /// <summary>
    /// 获取 HID Usage 码对应的按键名称（用于显示）
    /// </summary>
    public static string HidUsageToName(byte hidUsage)
    {
        // 字母 A-Z
        if (hidUsage is >= 0x04 and <= 0x1D)
        {
            return ((char)('A' + hidUsage - 0x04)).ToString();
        }

        // 数字 1-9
        if (hidUsage is >= 0x1E and <= 0x26)
        {
            return (hidUsage - 0x1E + 1).ToString();
        }

        return hidUsage switch
        {
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
            0x47 => "Scroll Lock",
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
            0xE0 => "Left Ctrl",
            0xE1 => "Left Shift",
            0xE2 => "Left Alt",
            0xE3 => "Left GUI",
            0xE4 => "Right Ctrl",
            0xE5 => "Right Shift",
            0xE6 => "Right Alt",
            0xE7 => "Right GUI",
            _ => $"0x{hidUsage:X2}"
        };
    }
}
