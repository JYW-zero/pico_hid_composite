namespace HidConfigTool.Core.Models;

/// <summary>
/// 按键定义
/// </summary>
public class KeyDefinition : IEquatable<KeyDefinition>
{
    /// <summary>
    /// HID 用法码
    /// </summary>
    public byte KeyCode { get; set; }

    /// <summary>
    /// 键名（显示用）
    /// </summary>
    public string Name { get; set; } = string.Empty;

    /// <summary>
    /// 分类
    /// </summary>
    public string Category { get; set; } = "其他";

    /// <summary>
    /// 描述
    /// </summary>
    public string Description { get; set; } = string.Empty;

    public bool Equals(KeyDefinition? other)
    {
        if (other == null) return false;
        return KeyCode == other.KeyCode;
    }

    public override bool Equals(object? obj)
    {
        return Equals(obj as KeyDefinition);
    }

    public override int GetHashCode()
    {
        return KeyCode.GetHashCode();
    }

    public override string ToString()
    {
        return Name;
    }
}

/// <summary>
/// 常用按键定义列表
/// </summary>
public static class KeyDefinitions
{
    /// <summary>
    /// 获取所有按键定义
    /// </summary>
    public static List<KeyDefinition> GetAll()
    {
        var keys = new List<KeyDefinition>();

        // 字母键
        for (byte i = 0x04; i <= 0x1D; i++)
        {
            keys.Add(new KeyDefinition
            {
                KeyCode = i,
                Name = ((char)('A' + (i - 0x04))).ToString(),
                Category = "字母",
                Description = $"字母 {((char)('A' + (i - 0x04)))}"
            });
        }

        // 数字键
        for (byte i = 0x1E; i <= 0x27; i++)
        {
            keys.Add(new KeyDefinition
            {
                KeyCode = i,
                Name = (i - 0x1E + 1).ToString(),
                Category = "数字",
                Description = $"数字 {i - 0x1E + 1}"
            });
        }

        // 常用功能键
        keys.AddRange(new[]
        {
            new KeyDefinition { KeyCode = 0x28, Name = "Enter", Category = "编辑", Description = "回车键" },
            new KeyDefinition { KeyCode = 0x29, Name = "Esc", Category = "功能", Description = "退出键" },
            new KeyDefinition { KeyCode = 0x2A, Name = "Backspace", Category = "编辑", Description = "退格键" },
            new KeyDefinition { KeyCode = 0x2B, Name = "Tab", Category = "编辑", Description = "制表键" },
            new KeyDefinition { KeyCode = 0x2C, Name = "Space", Category = "编辑", Description = "空格键" },
            new KeyDefinition { KeyCode = 0x2D, Name = "-", Category = "符号", Description = "减号/下划线" },
            new KeyDefinition { KeyCode = 0x2E, Name = "=", Category = "符号", Description = "等号/加号" },
            new KeyDefinition { KeyCode = 0x2F, Name = "[", Category = "符号", Description = "左方括号" },
            new KeyDefinition { KeyCode = 0x30, Name = "]", Category = "符号", Description = "右方括号" },
            new KeyDefinition { KeyCode = 0x31, Name = "\\", Category = "符号", Description = "反斜杠" },
            new KeyDefinition { KeyCode = 0x33, Name = ";", Category = "符号", Description = "分号/冒号" },
            new KeyDefinition { KeyCode = 0x34, Name = "'", Category = "符号", Description = "单引号/双引号" },
            new KeyDefinition { KeyCode = 0x35, Name = "`", Category = "符号", Description = "反引号/波浪号" },
            new KeyDefinition { KeyCode = 0x36, Name = ",", Category = "符号", Description = "逗号/小于号" },
            new KeyDefinition { KeyCode = 0x37, Name = ".", Category = "符号", Description = "句号/大于号" },
            new KeyDefinition { KeyCode = 0x38, Name = "/", Category = "符号", Description = "斜杠/问号" },
            new KeyDefinition { KeyCode = 0x39, Name = "Caps Lock", Category = "功能", Description = "大写锁定" },
        });

        // F 功能键
        for (byte i = 0x3A; i <= 0x45; i++)
        {
            keys.Add(new KeyDefinition
            {
                KeyCode = i,
                Name = $"F{i - 0x3A + 1}",
                Category = "功能键",
                Description = $"功能键 F{i - 0x3A + 1}"
            });
        }

        // 编辑键
        keys.AddRange(new[]
        {
            new KeyDefinition { KeyCode = 0x49, Name = "Insert", Category = "编辑", Description = "插入键" },
            new KeyDefinition { KeyCode = 0x4A, Name = "Home", Category = "编辑", Description = "行首键" },
            new KeyDefinition { KeyCode = 0x4B, Name = "Page Up", Category = "编辑", Description = "上翻页" },
            new KeyDefinition { KeyCode = 0x4C, Name = "Delete", Category = "编辑", Description = "删除键" },
            new KeyDefinition { KeyCode = 0x4D, Name = "End", Category = "编辑", Description = "行尾键" },
            new KeyDefinition { KeyCode = 0x4E, Name = "Page Down", Category = "编辑", Description = "下翻页" },
            new KeyDefinition { KeyCode = 0x4F, Name = "→ Right", Category = "方向", Description = "右方向键" },
            new KeyDefinition { KeyCode = 0x50, Name = "← Left", Category = "方向", Description = "左方向键" },
            new KeyDefinition { KeyCode = 0x51, Name = "↓ Down", Category = "方向", Description = "下方向键" },
            new KeyDefinition { KeyCode = 0x52, Name = "↑ Up", Category = "方向", Description = "上方向键" },
        });

        // 修饰键
        keys.AddRange(new[]
        {
            new KeyDefinition { KeyCode = 0xE0, Name = "Ctrl L", Category = "修饰键", Description = "左 Ctrl" },
            new KeyDefinition { KeyCode = 0xE1, Name = "Shift L", Category = "修饰键", Description = "左 Shift" },
            new KeyDefinition { KeyCode = 0xE2, Name = "Alt L", Category = "修饰键", Description = "左 Alt" },
            new KeyDefinition { KeyCode = 0xE3, Name = "GUI L", Category = "修饰键", Description = "左 Win/Cmd" },
            new KeyDefinition { KeyCode = 0xE4, Name = "Ctrl R", Category = "修饰键", Description = "右 Ctrl" },
            new KeyDefinition { KeyCode = 0xE5, Name = "Shift R", Category = "修饰键", Description = "右 Shift" },
            new KeyDefinition { KeyCode = 0xE6, Name = "Alt R", Category = "修饰键", Description = "右 Alt" },
            new KeyDefinition { KeyCode = 0xE7, Name = "GUI R", Category = "修饰键", Description = "右 Win/Cmd" },
        });

        // 特殊
        keys.Add(new KeyDefinition
        {
            KeyCode = 0x00,
            Name = "None",
            Category = "特殊",
            Description = "无功能"
        });

        return keys;
    }

    /// <summary>
    /// 获取所有分类
    /// </summary>
    public static List<string> GetCategories()
    {
        return GetAll().Select(k => k.Category).Distinct().ToList();
    }
}
