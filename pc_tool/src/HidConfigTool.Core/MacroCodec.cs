using System.Text;
using HidConfigTool.Core.Models;

namespace HidConfigTool.Core;

/// <summary>
/// 固件宏二进制格式编解码（148 字节）
/// </summary>
public static class MacroCodec
{
    public const int MacroSize = 148;
    public const int MaxActions = 32;

    public static Macro Parse(byte id, byte[] data)
    {
        var macro = new Macro { Id = id.ToString(), Name = $"宏 {id + 1}" };
        if (data.Length < 20)
            return macro;

        macro.RepeatCount = data[2];
        int actionCount = data[3];

        int nameLen = 0;
        for (int i = 0; i < 16; i++)
        {
            if (data[4 + i] == 0)
                break;
            nameLen++;
        }

        string name = Encoding.UTF8.GetString(data, 4, nameLen);
        if (!string.IsNullOrEmpty(name))
            macro.Name = name;

        macro.Actions.Clear();
        for (int i = 0; i < actionCount && i < MaxActions; i++)
        {
            int offset = 20 + i * 4;
            if (offset + 4 > data.Length)
                break;

            var action = new MacroAction
            {
                Type = (MacroActionType)data[offset],
                KeyCode = data[offset + 1],
                DelayMs = data[offset + 2] | (data[offset + 3] << 8)
            };
            action.KeyName = HidUsageNames.ToName((byte)action.KeyCode);
            macro.Actions.Add(action);
        }

        return macro;
    }

    public static byte[] Serialize(byte id, Macro macro)
    {
        byte[] data = new byte[MacroSize];
        data[0] = id;
        data[1] = 0;
        data[2] = (byte)Math.Min(macro.RepeatCount, 255);
        int actionCount = Math.Min(macro.Actions.Count, MaxActions);
        data[3] = (byte)actionCount;

        byte[] nameBytes = Encoding.UTF8.GetBytes(macro.Name ?? string.Empty);
        int nameLen = Math.Min(nameBytes.Length, 15);
        Array.Copy(nameBytes, 0, data, 4, nameLen);

        for (int i = 0; i < actionCount; i++)
        {
            int offset = 20 + i * 4;
            var action = macro.Actions[i];
            data[offset] = (byte)action.Type;
            data[offset + 1] = (byte)action.KeyCode;
            data[offset + 2] = (byte)(action.DelayMs & 0xFF);
            data[offset + 3] = (byte)((action.DelayMs >> 8) & 0xFF);
        }

        return data;
    }
}
