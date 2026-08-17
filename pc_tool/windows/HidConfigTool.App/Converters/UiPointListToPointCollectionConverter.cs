using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;
using HidConfigTool.Core;

namespace HidConfigTool.App.Converters;

/// <summary>
/// 将 List&lt;UiPoint&gt; 转换为 WPF PointCollection
/// </summary>
public class UiPointListToPointCollectionConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is IEnumerable<UiPoint> points)
        {
            var collection = new PointCollection();
            foreach (var p in points)
            {
                collection.Add(new Point(p.X, p.Y));
            }
            return collection;
        }
        return new PointCollection();
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is PointCollection collection)
        {
            var list = new List<UiPoint>();
            foreach (var p in collection)
            {
                list.Add(new UiPoint(p.X, p.Y));
            }
            return list;
        }
        return new List<UiPoint>();
    }
}
