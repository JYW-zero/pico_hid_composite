/*
 * HidErrorMapperTests.cs
 * HidErrorMapper 单元测试
 * 测试错误码映射、未知错误处理
 */

using HidConfigTool.Core.Utils;

namespace HidConfigTool.Tests;

/// <summary>
/// HidErrorMapper 单元测试
/// </summary>
public class HidErrorMapperTests
{
    #region 已知错误码测试

    [Theory]
    [InlineData(0, "操作成功")]
    [InlineData(2, "系统找不到指定的文件")]
    [InlineData(5, "拒绝访问（设备可能被其他程序占用）")]
    [InlineData(6, "句柄无效")]
    [InlineData(21, "设备未就绪")]
    [InlineData(32, "另一个程序正在使用此文件，进程无法访问")]
    [InlineData(87, "参数错误")]
    public void GetErrorMessage_WithKnownErrorCode_ShouldReturnMappedMessage(int errorCode, string expectedMessage)
    {
        // Act
        var message = HidErrorMapper.GetErrorMessage(errorCode);

        // Assert
        Assert.Equal(expectedMessage, message);
    }

    #endregion

    #region HID 特定错误码测试

    [Theory]
    [InlineData(1167, "设备未连接或已断开")]
    [InlineData(121, "信号灯超时时间已到（设备响应超时）")]
    public void GetErrorMessage_WithHidSpecificErrorCode_ShouldReturnMappedMessage(int errorCode, string expectedMessage)
    {
        // Act
        var message = HidErrorMapper.GetErrorMessage(errorCode);

        // Assert
        Assert.Equal(expectedMessage, message);
    }

    #endregion

    #region USB 相关错误码测试

    [Theory]
    [InlineData(433, "指定的设备不存在或未连接")]
    [InlineData(434, "设备连接已断开")]
    [InlineData(435, "设备忙，请稍后重试")]
    [InlineData(436, "设备通信超时")]
    [InlineData(437, "设备返回数据格式错误")]
    [InlineData(438, "设备固件版本不兼容")]
    [InlineData(439, "设备配置锁定，需要先解锁")]
    [InlineData(440, "设备 CRC 校验失败")]
    public void GetErrorMessage_WithUsbErrorCode_ShouldReturnMappedMessage(int errorCode, string expectedMessage)
    {
        // Act
        var message = HidErrorMapper.GetErrorMessage(errorCode);

        // Assert
        Assert.Equal(expectedMessage, message);
    }

    #endregion

    #region 未知错误码测试

    [Fact]
    public void GetErrorMessage_WithUnknownErrorCode_ShouldReturnUnknownErrorMessage()
    {
        // Arrange
        int unknownErrorCode = 99999;

        // Act
        var message = HidErrorMapper.GetErrorMessage(unknownErrorCode);

        // Assert
        Assert.Contains("未知错误", message);
        Assert.Contains(unknownErrorCode.ToString(), message);
    }

    [Fact]
    public void GetErrorMessage_WithNegativeErrorCode_ShouldReturnUnknownErrorMessage()
    {
        // Arrange
        int negativeErrorCode = -1;

        // Act
        var message = HidErrorMapper.GetErrorMessage(negativeErrorCode);

        // Assert
        Assert.Contains("未知错误", message);
    }

    #endregion

    #region 边界值测试

    [Fact]
    public void GetErrorMessage_WithMaxIntErrorCode_ShouldReturnUnknownErrorMessage()
    {
        // Arrange
        int maxErrorCode = int.MaxValue;

        // Act
        var message = HidErrorMapper.GetErrorMessage(maxErrorCode);

        // Assert
        Assert.Contains("未知错误", message);
    }

    [Fact]
    public void GetErrorMessage_WithMinIntErrorCode_ShouldReturnUnknownErrorMessage()
    {
        // Arrange
        int minErrorCode = int.MinValue;

        // Act
        var message = HidErrorMapper.GetErrorMessage(minErrorCode);

        // Assert
        Assert.Contains("未知错误", message);
    }

    #endregion

    #region 一致性测试

    [Fact]
    public void GetErrorMessage_CalledMultipleTimes_ShouldReturnSameMessage()
    {
        // Arrange
        int errorCode = 5;

        // Act
        var message1 = HidErrorMapper.GetErrorMessage(errorCode);
        var message2 = HidErrorMapper.GetErrorMessage(errorCode);
        var message3 = HidErrorMapper.GetErrorMessage(errorCode);

        // Assert
        Assert.Equal(message1, message2);
        Assert.Equal(message2, message3);
    }

    #endregion

    #region 系统资源错误码测试

    [Theory]
    [InlineData(1450)]
    [InlineData(1451)]
    [InlineData(1452)]
    public void GetErrorMessage_WithSystemResourceErrorCode_ShouldReturnResourceMessage(int errorCode)
    {
        // Act
        var message = HidErrorMapper.GetErrorMessage(errorCode);

        // Assert
        Assert.Contains("系统资源不足", message);
    }

    [Theory]
    [InlineData(1453)]
    [InlineData(1454)]
    public void GetErrorMessage_WithQuotaErrorCode_ShouldReturnQuotaMessage(int errorCode)
    {
        // Act
        var message = HidErrorMapper.GetErrorMessage(errorCode);

        // Assert
        Assert.Contains("配额不足", message);
    }

    [Fact]
    public void GetErrorMessage_WithPageFileErrorCode_ShouldReturnPageFileMessage()
    {
        // Arrange
        int errorCode = 1455;

        // Act
        var message = HidErrorMapper.GetErrorMessage(errorCode);

        // Assert
        Assert.Contains("页面文件太小", message);
    }

    #endregion
}
