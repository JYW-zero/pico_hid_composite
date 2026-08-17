namespace HidConfigTool.Core;

/// <summary>
/// 跨平台 RGB 颜色表示，用于替代 WPF 的 Brush/Color
/// </summary>
public readonly struct UiColor
{
    public byte R { get; }
    public byte G { get; }
    public byte B { get; }
    public byte A { get; }

    public UiColor(byte r, byte g, byte b, byte a = 255)
    {
        R = r;
        G = g;
        B = b;
        A = a;
    }

    /// <summary>透明色</summary>
    public static UiColor Transparent => new(0, 0, 0, 0);

    /// <summary>转换为十六进制字符串，如 #FF1A283B</summary>
    public string ToHex() => $"#{A:X2}{R:X2}{G:X2}{B:X2}";

    public override string ToString() => ToHex();
}
