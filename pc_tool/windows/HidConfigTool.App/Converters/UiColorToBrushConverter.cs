using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;
using HidConfigTool.Core;

namespace HidConfigTool.App.Converters;

/// <summary>
/// 将 UiColor 转换为 WPF Brush
/// </summary>
public class UiColorToBrushConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is UiColor color)
        {
            return new SolidColorBrush(Color.FromArgb(color.A, color.R, color.G, color.B));
        }
        return Brushes.Transparent;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is SolidColorBrush brush)
        {
            return new UiColor(brush.Color.R, brush.Color.G, brush.Color.B, brush.Color.A);
        }
        return UiColor.Transparent;
    }
}
