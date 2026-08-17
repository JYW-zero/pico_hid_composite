using System.Windows;
using HidConfigTool.App.Views;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.App.Services;

/// <summary>
/// 帮助窗口服务实现
/// </summary>
public class HelpWindowService : IHelpWindowService
{
    public void ShowHelp()
    {
        var helpWindow = new HelpWindow
        {
            Owner = Application.Current.MainWindow
        };
        helpWindow.ShowDialog();
    }
}
