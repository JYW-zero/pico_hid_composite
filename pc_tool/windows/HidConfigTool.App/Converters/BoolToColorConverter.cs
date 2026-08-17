using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;

namespace HidConfigTool.App.Converters;

/// <summary>
/// 布尔值转颜色转换器
/// 参数格式: "TrueColor|FalseColor"，支持颜色名
/// </summary>
public class BoolToColorConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        bool boolValue = value is bool b && b;
        string? param = parameter?.ToString();

        Color trueColor = Colors.LimeGreen;
        Color falseColor = Colors.IndianRed;

        if (!string.IsNullOrEmpty(param))
        {
            string[] parts = param.Split('|');
            if (parts.Length >= 2)
            {
                try
                {
                    trueColor = (Color)ColorConverter.ConvertFromString(parts[0]);
                    falseColor = (Color)ColorConverter.ConvertFromString(parts[1]);
                }
                catch
                {
                    // 解析失败用默认值
                }
            }
        }

        return new SolidColorBrush(boolValue ? trueColor : falseColor);
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}
