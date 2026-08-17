using System.Globalization;
using System.Windows.Data;

namespace HidConfigTool.App.Converters;

/// <summary>
/// 枚举值转可见性转换器
/// 当绑定值等于参数时返回 Visible，否则返回 Collapsed
/// </summary>
public class EnumToVisConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value == null || parameter == null)
            return System.Windows.Visibility.Collapsed;

        string? paramStr = parameter?.ToString();
        if (string.IsNullOrEmpty(paramStr))
            return System.Windows.Visibility.Collapsed;

        // 尝试将参数转换为枚举类型
        if (value.GetType().IsEnum)
        {
            try
            {
                object? paramEnum = Enum.Parse(value.GetType(), paramStr);
                return value.Equals(paramEnum) ? System.Windows.Visibility.Visible : System.Windows.Visibility.Collapsed;
            }
            catch
            {
                return System.Windows.Visibility.Collapsed;
            }
        }

        return value.ToString() == paramStr ? System.Windows.Visibility.Visible : System.Windows.Visibility.Collapsed;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}
