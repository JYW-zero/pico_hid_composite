/*
 * ConfigImportExportServiceTests.cs
 * ConfigImportExportService 单元测试
 * 测试配置导入导出、校验逻辑、错误处理
 */

using HidConfigTool.Core.Models;
using HidConfigTool.Core.Services;

namespace HidConfigTool.Tests;

/// <summary>
/// ConfigImportExportService 单元测试
/// </summary>
public class ConfigImportExportServiceTests : IDisposable
{
    private readonly ConfigImportExportService _service;
    private readonly string _tempDirectory;

    public ConfigImportExportServiceTests()
    {
        _service = new ConfigImportExportService();
        _tempDirectory = Path.Combine(Path.GetTempPath(), $"HidConfigTest_{Guid.NewGuid():N}");
        Directory.CreateDirectory(_tempDirectory);
    }

    public void Dispose()
    {
        if (Directory.Exists(_tempDirectory))
        {
            Directory.Delete(_tempDirectory, true);
        }
    }

    #region 辅助方法

    private DeviceConfig CreateValidConfig()
    {
        return new DeviceConfig
        {
            Version = 7,
            Dpi = 800,
            DpiIndex = 1,
            JoystickDeadzone = 100,
            JoystickSensitivity = 1.0,
            JoystickCenterX = 2048,
            JoystickCenterY = 2048,
            AccelerationEnabled = false,
            AccelerationThreshold = 10,
            AccelerationRatio = 1.5,
            DpiLevels = new int[] { 400, 800, 1600, 3200 },
            DpiSlots = new ushort[] { 400, 800, 1600, 3200 },
            CurrentDpiSlot = 1,
            LodLevel = 1,
            AngleSnapping = false
        };
    }

    #endregion

    #region JSON 导出测试

    [Fact]
    public async Task ExportToJsonAsync_WithValidConfig_ShouldReturnTrue()
    {
        // Arrange
        var config = CreateValidConfig();
        var filePath = Path.Combine(_tempDirectory, "config.json");

        // Act
        var result = await _service.ExportToJsonAsync(config, filePath);

        // Assert
        Assert.True(result);
        Assert.True(File.Exists(filePath));
    }

    [Fact]
    public async Task ExportToJsonAsync_WithNullConfig_ShouldReturnFalse()
    {
        // Arrange
        var filePath = Path.Combine(_tempDirectory, "config.json");

        // Act
        var result = await _service.ExportToJsonAsync(null!, filePath);

        // Assert
        Assert.False(result);
    }

    [Fact]
    public async Task ExportToJsonAsync_ShouldCreateDirectoryIfNotExists()
    {
        // Arrange
        var config = CreateValidConfig();
        var subDirectory = Path.Combine(_tempDirectory, "subdir", "nested");
        var filePath = Path.Combine(subDirectory, "config.json");

        // Act
        var result = await _service.ExportToJsonAsync(config, filePath);

        // Assert
        Assert.True(result);
        Assert.True(Directory.Exists(subDirectory));
        Assert.True(File.Exists(filePath));
    }

    [Fact]
    public async Task ExportToJsonAsync_ShouldWriteValidJson()
    {
        // Arrange
        var config = CreateValidConfig();
        config.Dpi = 1200;
        var filePath = Path.Combine(_tempDirectory, "config.json");

        // Act
        await _service.ExportToJsonAsync(config, filePath);
        var json = await File.ReadAllTextAsync(filePath);

        // Assert
        Assert.Contains("1200", json);
        Assert.Contains("Dpi", json);
    }

    #endregion

    #region JSON 导入测试

    [Fact]
    public async Task ImportFromJsonAsync_WithValidFile_ShouldReturnSuccess()
    {
        // Arrange
        var config = CreateValidConfig();
        config.Dpi = 1600;
        var filePath = Path.Combine(_tempDirectory, "config.json");
        await _service.ExportToJsonAsync(config, filePath);

        // Act
        var result = await _service.ImportFromJsonAsync(filePath);

        // Assert
        Assert.NotNull(result);
        Assert.True(result.Success);
        Assert.NotNull(result.Config);
        Assert.Equal(1600, result.Config.Dpi);
    }

    [Fact]
    public async Task ImportFromJsonAsync_WithNonExistentFile_ShouldReturnFailure()
    {
        // Arrange
        var filePath = Path.Combine(_tempDirectory, "nonexistent.json");

        // Act
        var result = await _service.ImportFromJsonAsync(filePath);

        // Assert
        Assert.NotNull(result);
        Assert.False(result.Success);
        Assert.Equal("文件不存在", result.ErrorMessage);
    }

    [Fact]
    public async Task ImportFromJsonAsync_WithInvalidJson_ShouldReturnFailure()
    {
        // Arrange
        var filePath = Path.Combine(_tempDirectory, "invalid.json");
        await File.WriteAllTextAsync(filePath, "this is not valid json {{{");

        // Act
        var result = await _service.ImportFromJsonAsync(filePath);

        // Assert
        Assert.NotNull(result);
        Assert.False(result.Success);
        Assert.Contains("JSON", result.ErrorMessage);
    }

    [Fact]
    public async Task ImportFromJsonAsync_WithOversizedFile_ShouldReturnFailure()
    {
        // Arrange
        var filePath = Path.Combine(_tempDirectory, "large.json");
        // 创建一个超过 1MB 的文件
        var largeContent = new string('x', 1024 * 1024 + 1);
        await File.WriteAllTextAsync(filePath, largeContent);

        // Act
        var result = await _service.ImportFromJsonAsync(filePath);

        // Assert
        Assert.NotNull(result);
        Assert.False(result.Success);
        Assert.Contains("过大", result.ErrorMessage);
    }

    [Fact]
    public async Task ImportFromJsonAsync_WithHighVersion_ShouldSetVersionWarning()
    {
        // Arrange
        var config = CreateValidConfig();
        config.Version = ConfigImportExportService.MaxSupportedConfigVersion + 1;
        var filePath = Path.Combine(_tempDirectory, "high_version.json");
        await _service.ExportToJsonAsync(config, filePath);

        // Act
        var result = await _service.ImportFromJsonAsync(filePath);

        // Assert
        Assert.NotNull(result);
        Assert.True(result.Success);
        Assert.True(result.HasVersionWarning);
    }

    #endregion

    #region 配置校验测试

    [Fact]
    public void ValidateConfig_WithValidConfig_ShouldReturnValid()
    {
        // Arrange
        var config = CreateValidConfig();

        // Act
        var result = _service.ValidateConfig(config);

        // Assert
        Assert.True(result.IsValid);
        Assert.Null(result.ErrorMessage);
    }

    [Fact]
    public void ValidateConfig_WithNullConfig_ShouldReturnInvalid()
    {
        // Act
        var result = _service.ValidateConfig(null!);

        // Assert
        Assert.False(result.IsValid);
        Assert.Equal("配置为空", result.ErrorMessage);
    }

    [Theory]
    [InlineData(0)]
    [InlineData(-1)]
    public void ValidateConfig_WithLowVersion_ShouldReturnInvalid(int version)
    {
        // Arrange
        var config = CreateValidConfig();
        config.Version = version;

        // Act
        var result = _service.ValidateConfig(config);

        // Assert
        Assert.False(result.IsValid);
        Assert.Contains("版本过低", result.ErrorMessage);
    }

    [Theory]
    [InlineData(99)]
    [InlineData(6401)]
    public void ValidateConfig_WithInvalidDpi_ShouldReturnInvalid(int dpi)
    {
        // Arrange
        var config = CreateValidConfig();
        config.Dpi = (ushort)dpi;

        // Act
        var result = _service.ValidateConfig(config);

        // Assert
        Assert.False(result.IsValid);
        Assert.Contains("DPI", result.ErrorMessage);
    }

    [Fact]
    public void ValidateConfig_WithInvalidDeadzone_ShouldReturnInvalid()
    {
        // Arrange
        var config = CreateValidConfig();
        config.JoystickDeadzone = 4096;

        // Act
        var result = _service.ValidateConfig(config);

        // Assert
        Assert.False(result.IsValid);
        Assert.Contains("死区", result.ErrorMessage);
    }

    [Theory]
    [InlineData(0.05)]
    [InlineData(10.5)]
    public void ValidateConfig_WithInvalidSensitivity_ShouldReturnInvalid(double sensitivity)
    {
        // Arrange
        var config = CreateValidConfig();
        config.JoystickSensitivity = sensitivity;

        // Act
        var result = _service.ValidateConfig(config);

        // Assert
        Assert.False(result.IsValid);
        Assert.Contains("灵敏度", result.ErrorMessage);
    }

    [Fact]
    public void ValidateConfig_WithInvalidCenterX_ShouldReturnInvalid()
    {
        // Arrange
        var config = CreateValidConfig();
        config.JoystickCenterX = 4096;

        // Act
        var result = _service.ValidateConfig(config);

        // Assert
        Assert.False(result.IsValid);
        Assert.Contains("中心点", result.ErrorMessage);
    }

    [Fact]
    public void ValidateConfig_WithInvalidCenterY_ShouldReturnInvalid()
    {
        // Arrange
        var config = CreateValidConfig();
        config.JoystickCenterY = 4096;

        // Act
        var result = _service.ValidateConfig(config);

        // Assert
        Assert.False(result.IsValid);
        Assert.Contains("中心点", result.ErrorMessage);
    }

    #endregion

    #region 二进制导入测试

    [Fact]
    public async Task ImportFromBinaryAsync_WithValidFile_ShouldReturnData()
    {
        // Arrange
        var filePath = Path.Combine(_tempDirectory, "config.bin");
        var expectedData = new byte[256];
        for (int i = 0; i < expectedData.Length; i++)
        {
            expectedData[i] = (byte)(i % 256);
        }
        await File.WriteAllBytesAsync(filePath, expectedData);

        // Act
        var result = await _service.ImportFromBinaryAsync(filePath, 256);

        // Assert
        Assert.NotNull(result);
        Assert.Equal(256, result.Length);
        Assert.Equal(expectedData, result);
    }

    [Fact]
    public async Task ImportFromBinaryAsync_WithNonExistentFile_ShouldReturnNull()
    {
        // Arrange
        var filePath = Path.Combine(_tempDirectory, "nonexistent.bin");

        // Act
        var result = await _service.ImportFromBinaryAsync(filePath, 256);

        // Assert
        Assert.Null(result);
    }

    [Fact]
    public async Task ImportFromBinaryAsync_WithFileTooSmall_ShouldReturnNull()
    {
        // Arrange
        var filePath = Path.Combine(_tempDirectory, "small.bin");
        await File.WriteAllBytesAsync(filePath, new byte[100]);

        // Act
        var result = await _service.ImportFromBinaryAsync(filePath, 256);

        // Assert
        Assert.Null(result);
    }

    [Fact]
    public async Task ImportFromBinaryAsync_WithOversizedFile_ShouldReturnNull()
    {
        // Arrange
        var filePath = Path.Combine(_tempDirectory, "large.bin");
        await File.WriteAllBytesAsync(filePath, new byte[1024 * 1024 + 1]);

        // Act
        var result = await _service.ImportFromBinaryAsync(filePath, 256);

        // Assert
        Assert.Null(result);
    }

    #endregion

    #region 常量测试

    [Fact]
    public void MaxSupportedConfigVersion_ShouldBe8()
    {
        Assert.Equal(8, ConfigImportExportService.MaxSupportedConfigVersion);
    }

    [Fact]
    public void MinSupportedConfigVersion_ShouldBe1()
    {
        Assert.Equal(1, ConfigImportExportService.MinSupportedConfigVersion);
    }

    #endregion

    #region 往返测试

    [Fact]
    public async Task ExportThenImportJson_ShouldPreserveConfig()
    {
        // Arrange
        var originalConfig = CreateValidConfig();
        originalConfig.Dpi = 2400;
        originalConfig.JoystickDeadzone = 256;
        originalConfig.AccelerationEnabled = true;
        var filePath = Path.Combine(_tempDirectory, "roundtrip.json");

        // Act
        await _service.ExportToJsonAsync(originalConfig, filePath);
        var result = await _service.ImportFromJsonAsync(filePath);

        // Assert
        Assert.NotNull(result);
        Assert.True(result.Success);
        Assert.NotNull(result.Config);
        Assert.Equal(2400, result.Config.Dpi);
        Assert.Equal(256, result.Config.JoystickDeadzone);
        Assert.True(result.Config.AccelerationEnabled);
    }

    #endregion
}
