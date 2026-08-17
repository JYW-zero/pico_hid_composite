using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace HidConfigTool.App.Converters;

/// <summary>
/// 布尔值取反转换器
/// 支持绑定到 bool（如 IsEnabled）和 Visibility
/// </summary>
public class InvertBoolConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is bool b)
        {
            bool result = !b;
            if (targetType == typeof(Visibility))
                return result ? Visibility.Visible : Visibility.Collapsed;
            return result;
        }
        if (targetType == typeof(Visibility))
            return Visibility.Visible;
        return true;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is bool b)
            return !b;
        if (value is Visibility vis)
            return vis != Visibility.Visible;
        return true;
    }
}
