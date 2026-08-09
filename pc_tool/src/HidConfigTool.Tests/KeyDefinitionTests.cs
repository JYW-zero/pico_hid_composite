using HidConfigTool.Core.Models;

namespace HidConfigTool.Tests;

public class KeyDefinitionTests
{
    [Fact]
    public void KeyDefinition_DefaultValues_AreCorrect()
    {
        var key = new KeyDefinition();

        Assert.Equal(0, key.KeyCode);
        Assert.Equal(string.Empty, key.Name);
        Assert.Equal("其他", key.Category);
        Assert.Equal(string.Empty, key.Description);
    }

    [Fact]
    public void KeyDefinitions_GetAll_ReturnsNonEmptyList()
    {
        var keys = KeyDefinitions.GetAll();

        Assert.NotNull(keys);
        Assert.NotEmpty(keys);
        Assert.True(keys.Count > 50);
    }

    [Fact]
    public void KeyDefinitions_GetAll_ContainsLetterKeys()
    {
        var keys = KeyDefinitions.GetAll();

        var keyA = keys.FirstOrDefault(k => k.Name == "A");
        Assert.NotNull(keyA);
        Assert.Equal(0x04, keyA.KeyCode);
        Assert.Equal("字母", keyA.Category);
    }

    [Fact]
    public void KeyDefinitions_GetCategories_ReturnsDistinctCategories()
    {
        var categories = KeyDefinitions.GetCategories();

        Assert.NotNull(categories);
        Assert.NotEmpty(categories);
        Assert.Contains("字母", categories);
        Assert.Contains("数字", categories);
        Assert.Contains("功能键", categories);
    }

    [Fact]
    public void KeyDefinitions_GetAll_ContainsModifierKeys()
    {
        var keys = KeyDefinitions.GetAll();

        Assert.Contains(keys, k => k.Name == "Ctrl L");
        Assert.Contains(keys, k => k.Name == "Shift L");
        Assert.Contains(keys, k => k.Name == "Alt L");
    }
}
