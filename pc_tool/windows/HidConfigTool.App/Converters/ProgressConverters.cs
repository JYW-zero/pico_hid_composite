using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace HidConfigTool.App.Converters;

/// <summary>
/// ProgressBar 进度条宽度转换器
/// 输入：Value, Maximum, ActualWidth
/// 输出：indicator 的宽度
/// </summary>
public class ProgressToWidthConverter : IMultiValueConverter
{
    public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
    {
        if (values.Length >= 3 &&
            values[0] is double value &&
            values[1] is double maximum &&
            values[2] is double actualWidth &&
            maximum > 0)
        {
            return Math.Min(value / maximum * actualWidth, actualWidth);
        }
        return 0.0;
    }

    public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}

/// <summary>
/// Slider 已填充部分宽度转换器
/// 输入：Value, Minimum, Maximum, ActualWidth
/// </summary>
public class SliderFillConverter : IMultiValueConverter
{
    public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
    {
        if (values.Length >= 4 &&
            values[0] is double value &&
            values[1] is double minimum &&
            values[2] is double maximum &&
            values[3] is double actualWidth &&
            maximum > minimum)
        {
            return (value - minimum) / (maximum - minimum) * actualWidth;
        }
        return 0.0;
    }

    public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}

/// <summary>
/// Slider 滑块水平偏移转换器
/// 输入：Value, Minimum, Maximum, ActualWidth, ThumbWidth
/// </summary>
public class SliderThumbConverter : IMultiValueConverter
{
    public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
    {
        if (values.Length >= 5 &&
            values[0] is double value &&
            values[1] is double minimum &&
            values[2] is double maximum &&
            values[3] is double actualWidth &&
            values[4] is double thumbWidth &&
            maximum > minimum)
        {
            var ratio = (value - minimum) / (maximum - minimum);
            return ratio * (actualWidth - thumbWidth);
        }
        return 0.0;
    }

    public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}
