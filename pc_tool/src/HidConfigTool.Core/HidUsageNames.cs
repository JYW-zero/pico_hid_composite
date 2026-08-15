namespace HidConfigTool.Core;

/// <summary>
/// HID Usage 码显示名称（跨平台，不依赖 Win32）
/// </summary>
public static class HidUsageNames
{
    public static string ToName(byte hidUsage)
    {
        if (hidUsage == 0x00)
            return "None";

        if (hidUsage is >= 0x04 and <= 0x1D)
            return ((char)('A' + hidUsage - 0x04)).ToString();

        if (hidUsage is >= 0x1E and <= 0x26)
            return (hidUsage - 0x1E + 1).ToString();

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
            0xFE => "Fn",
            0xFF => "Modifier",
            _ => $"0x{hidUsage:X2}"
        };
    }
}
