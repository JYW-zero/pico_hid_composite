using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Desktop.Services;

/// <summary>
/// Avalonia 键选择对话框服务（基础实现，后续可替换为自定义窗口）
/// </summary>
public class KeyPickerService : IKeyPickerService
{
    public (byte KeyCode, string KeyName)? ShowKeyPicker(byte currentKeyCode)
    {
        // 简单实现：返回当前值，后续可替换为自定义对话框窗口
        return (currentKeyCode, currentKeyCode.ToString());
    }
}
