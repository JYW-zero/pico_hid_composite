using System.Globalization;
using System.Windows.Data;

namespace HidConfigTool.App.Converters;

/// <summary>
/// 空值转布尔转换器
/// null -> false, 非 null -> true
/// </summary>
public class NullToBoolConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return value != null;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}
