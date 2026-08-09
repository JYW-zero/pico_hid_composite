using HidConfigTool.Core.Models;

namespace HidConfigTool.Tests;

public class MacroTests
{
    [Fact]
    public void Macro_DefaultValues_AreCorrect()
    {
        var macro = new Macro();

        Assert.NotNull(macro.Id);
        Assert.NotEmpty(macro.Id);
        Assert.Equal("新建宏", macro.Name);
        Assert.NotNull(macro.Actions);
        Assert.Empty(macro.Actions);
        Assert.Equal(1, macro.RepeatCount);
        Assert.False(macro.RepeatUntilReleased);
    }

    [Fact]
    public void MacroAction_Type_WorksCorrectly()
    {
        var action = new MacroAction
        {
            Type = MacroActionType.KeyDown,
            KeyCode = 65,
            KeyName = "A",
            DelayMs = 100
        };

        Assert.Equal(MacroActionType.KeyDown, action.Type);
        Assert.Equal(65, action.KeyCode);
        Assert.Equal("A", action.KeyName);
        Assert.Equal(100, action.DelayMs);
    }

    [Fact]
    public void Macro_AddActions_WorksCorrectly()
    {
        var macro = new Macro();
        macro.Actions.Add(new MacroAction { Type = MacroActionType.KeyDown, KeyName = "Ctrl" });
        macro.Actions.Add(new MacroAction { Type = MacroActionType.KeyDown, KeyName = "C" });
        macro.Actions.Add(new MacroAction { Type = MacroActionType.KeyUp, KeyName = "C" });
        macro.Actions.Add(new MacroAction { Type = MacroActionType.KeyUp, KeyName = "Ctrl" });

        Assert.Equal(4, macro.Actions.Count);
        Assert.Equal("Ctrl", macro.Actions[0].KeyName);
        Assert.Equal(MacroActionType.KeyUp, macro.Actions[3].Type);
    }
}
