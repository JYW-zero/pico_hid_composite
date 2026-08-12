using HidConfigTool.App.Services;
using System.Windows.Input;

namespace HidConfigTool.Tests;

/// <summary>
/// HidKeyConverter 单元测试
/// 测试 HID Usage 码与名称、虚拟键码之间的转换
/// </summary>
public class HidKeyConverterTests
{
    [Theory]
    [InlineData(0x04, "A")]
    [InlineData(0x05, "B")]
    [InlineData(0x06, "C")]
    [InlineData(0x1D, "Z")]
    [InlineData(0x1E, "1")]
    [InlineData(0x27, "0")]
    [InlineData(0x28, "Enter")]
    [InlineData(0x29, "Esc")]
    [InlineData(0x2A, "Backspace")]
    [InlineData(0x2B, "Tab")]
    [InlineData(0x2C, "Space")]
    [InlineData(0x3A, "F1")]
    [InlineData(0x45, "F12")]
    [InlineData(0x4F, "Right")]
    [InlineData(0x50, "Left")]
    [InlineData(0x51, "Down")]
    [InlineData(0x52, "Up")]
    [InlineData(0xE0, "Left Ctrl")]
    [InlineData(0xE1, "Left Shift")]
    [InlineData(0xE2, "Left Alt")]
    [InlineData(0xE3, "Left GUI")]
    [InlineData(0xE4, "Right Ctrl")]
    [InlineData(0xE5, "Right Shift")]
    [InlineData(0xE6, "Right Alt")]
    [InlineData(0xE7, "Right GUI")]
    public void HidUsageToName_KnownKeys_ReturnsCorrectName(byte hidUsage, string expectedName)
    {
        string result = HidKeyConverter.HidUsageToName(hidUsage);
        Assert.Equal(expectedName, result);
    }

    [Fact]
    public void HidUsageToName_UnknownKey_ReturnsHexFormat()
    {
        string result = HidKeyConverter.HidUsageToName(0xFF);
        Assert.Contains("0xFF", result);
    }

    [Theory]
    [InlineData(0x41, 0x04)] // A
    [InlineData(0x42, 0x05)] // B
    [InlineData(0x5A, 0x1D)] // Z
    [InlineData(0x31, 0x1E)] // 1
    [InlineData(0x30, 0x27)] // 0
    [InlineData(0x0D, 0x28)] // Enter
    [InlineData(0x1B, 0x29)] // Escape
    [InlineData(0x08, 0x2A)] // Backspace
    [InlineData(0x09, 0x2B)] // Tab
    [InlineData(0x20, 0x2C)] // Space
    [InlineData(0x70, 0x3A)] // F1
    [InlineData(0x7B, 0x45)] // F12
    public void VirtualKeyToHidUsage_KnownKeys_ReturnsCorrectHidUsage(int vkCode, byte expectedHidUsage)
    {
        byte result = HidKeyConverter.VirtualKeyToHidUsage(vkCode);
        Assert.Equal(expectedHidUsage, result);
    }

    [Fact]
    public void VirtualKeyToHidUsage_UnknownKey_ReturnsZero()
    {
        byte result = HidKeyConverter.VirtualKeyToHidUsage(0x00);
        Assert.Equal((byte)0, result);
    }

    [Theory]
    [InlineData(Key.A, 0x04)]
    [InlineData(Key.B, 0x05)]
    [InlineData(Key.Z, 0x1D)]
    [InlineData(Key.D1, 0x1E)]
    [InlineData(Key.D0, 0x27)]
    [InlineData(Key.Enter, 0x28)]
    [InlineData(Key.Escape, 0x29)]
    [InlineData(Key.Back, 0x2A)]
    [InlineData(Key.Tab, 0x2B)]
    [InlineData(Key.Space, 0x2C)]
    [InlineData(Key.F1, 0x3A)]
    [InlineData(Key.F12, 0x45)]
    [InlineData(Key.LeftCtrl, 0xE0)]
    [InlineData(Key.LeftShift, 0xE1)]
    [InlineData(Key.LeftAlt, 0xE2)]
    [InlineData(Key.LWin, 0xE3)]
    [InlineData(Key.RightCtrl, 0xE4)]
    [InlineData(Key.RightShift, 0xE5)]
    [InlineData(Key.RightAlt, 0xE6)]
    [InlineData(Key.RWin, 0xE7)]
    public void KeyToHidUsage_KnownKeys_ReturnsCorrectHidUsage(Key key, byte expectedHidUsage)
    {
        byte result = HidKeyConverter.KeyToHidUsage(key);
        Assert.Equal(expectedHidUsage, result);
    }

    [Fact]
    public void HidUsageToName_AllLetters_AreMapped()
    {
        for (byte i = 0x04; i <= 0x1D; i++)
        {
            string name = HidKeyConverter.HidUsageToName(i);
            Assert.False(string.IsNullOrEmpty(name), $"HID Usage 0x{i:X2} should have a name");
            Assert.DoesNotContain("0x", name); // 不应该是未知键的十六进制格式
        }
    }

    [Fact]
    public void HidUsageToName_AllNumbers_AreMapped()
    {
        for (byte i = 0x1E; i <= 0x27; i++)
        {
            string name = HidKeyConverter.HidUsageToName(i);
            Assert.False(string.IsNullOrEmpty(name), $"HID Usage 0x{i:X2} should have a name");
        }
    }

    [Fact]
    public void HidUsageToName_AllFunctionKeys_AreMapped()
    {
        for (byte i = 0x3A; i <= 0x45; i++)
        {
            string name = HidKeyConverter.HidUsageToName(i);
            Assert.False(string.IsNullOrEmpty(name), $"HID Usage 0x{i:X2} should have a name");
            Assert.StartsWith("F", name);
        }
    }

    [Fact]
    public void HidUsageToName_AllModifiers_AreMapped()
    {
        for (byte i = 0xE0; i <= 0xE7; i++)
        {
            string name = HidKeyConverter.HidUsageToName(i);
            Assert.False(string.IsNullOrEmpty(name), $"HID Usage 0x{i:X2} should have a name");
        }
    }
}
