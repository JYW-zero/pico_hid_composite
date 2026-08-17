namespace HidConfigTool.Core;

/// <summary>
/// 跨平台二维点，用于替代 WPF 的 Point/PointCollection
/// </summary>
public readonly struct UiPoint
{
    public double X { get; }
    public double Y { get; }

    public UiPoint(double x, double y)
    {
        X = x;
        Y = y;
    }

    public override string ToString() => $"({X}, {Y})";
}
