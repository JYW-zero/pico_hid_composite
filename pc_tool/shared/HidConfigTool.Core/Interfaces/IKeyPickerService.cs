namespace HidConfigTool.Core.Interfaces;

/// <summary>
/// 键选择对话框服务抽象
/// </summary>
public interface IKeyPickerService
{
    /// <summary>
    /// 显示键选择对话框，返回选中的键码和键名，取消返回 null
    /// </summary>
    (byte KeyCode, string KeyName)? ShowKeyPicker(byte currentKeyCode);
}
