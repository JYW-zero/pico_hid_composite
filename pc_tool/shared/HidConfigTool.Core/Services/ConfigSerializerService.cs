/*
 * ConfigSerializerService.cs
 * 配置序列化/反序列化服务
 *
 * 从 DeviceService.cs 提取，包含配置二进制数据与 DeviceConfig 对象之间的转换逻辑。
 * 支持 v1-v8 所有配置版本的解析和序列化。
 *
 * 与固件 firmware/include/board/config.h 中的 device_config_t 结构体保持同步。
 */

namespace HidConfigTool.Core.Services;

using HidConfigTool.Core.Models;

/// <summary>
/// 配置序列化/反序列化服务
/// 将二进制配置数据转换为 DeviceConfig 对象，或将 DeviceConfig 对象序列化为二进制数据
/// </summary>
public class ConfigSerializerService
{
    /// <summary>
    /// 配置魔数（统一从 ProtocolConstants.ConfigMagic 引用，避免重复定义）
    /// </summary>
    public const uint CONFIG_MAGIC = ProtocolConstants.ConfigMagic;

    /// <summary>
    /// 配置结构总大小（字节）
    /// </summary>
    public static readonly int CONFIG_SIZE = ConfigOffsets.TotalSize;

    /// <summary>
    /// 解析二进制配置数据为 DeviceConfig 对象
    /// </summary>
    /// <param name="data">二进制配置数据</param>
    /// <returns>解析后的 DeviceConfig 对象（魔数不匹配时返回默认配置）</returns>
    public DeviceConfig ParseConfig(byte[] data)
    {
        var config = new DeviceConfig();

        // 验证魔数
        if (data.Length < ConfigOffsets.Magic + 4)
            return config;

        uint magic = BitConverter.ToUInt32(data, ConfigOffsets.Magic);
        if (magic != CONFIG_MAGIC)
        {
            // 魔数不对，返回默认配置
            return config;
        }

        // 版本
        if (data.Length >= ConfigOffsets.Version + 2)
            config.Version = BitConverter.ToUInt16(data, ConfigOffsets.Version);

        // DPI
        if (data.Length >= ConfigOffsets.Dpi + 2)
        {
            config.Dpi = BitConverter.ToUInt16(data, ConfigOffsets.Dpi);
            // 根据 DPI 值计算索引
            for (int i = 0; i < config.DpiLevels.Length; i++)
            {
                if (config.DpiLevels[i] == config.Dpi)
                {
                    config.DpiIndex = i;
                    break;
                }
            }
        }

        // 摇杆死区
        if (data.Length >= ConfigOffsets.JoystickDeadzone + 2)
            config.JoystickDeadzone = BitConverter.ToUInt16(data, ConfigOffsets.JoystickDeadzone);

        // 编码器方向
        if (data.Length >= ConfigOffsets.EncoderReverse + 1)
            config.EncoderReverse = data[ConfigOffsets.EncoderReverse] != 0;

        // 按键映射
        config.Keymap = new byte[64];
        if (data.Length >= ConfigOffsets.Keymap + 64)
        {
            Array.Copy(data, ConfigOffsets.Keymap, config.Keymap, 0, 64);
        }

        // Fn 层按键映射
        config.FnKeymap = new byte[64];
        if (data.Length >= ConfigOffsets.FnKeymap + 64)
        {
            Array.Copy(data, ConfigOffsets.FnKeymap, config.FnKeymap, 0, 64);
        }

        // v3 新增字段（版本>=3时有效；长度门槛按本块末尾字段，避免随 CRC 偏移漂移）
        if (config.Version >= 3 && data.Length >= ConfigOffsets.JoystickSensitivity + 2)
        {
            config.JoystickInvertX = data[ConfigOffsets.JoyInvertX] != 0;
            config.JoystickInvertY = data[ConfigOffsets.JoyInvertY] != 0;
            config.EncoderStepsPerTick = data[ConfigOffsets.EncoderSteps];
            config.EncoderScrollSpeed = data[ConfigOffsets.EncoderScroll];
            if (data.Length >= ConfigOffsets.JoystickSensitivity + 2)
            {
                config.JoystickSensitivity = BitConverter.ToUInt16(data, ConfigOffsets.JoystickSensitivity) / 1000.0;
            }
        }

        // v5 新增字段：摇杆校准数据（版本>=5时有效）
        if (config.Version >= 5 && data.Length >= ConfigOffsets.JoystickCalibrated + 1)
        {
            config.JoystickCenterX = BitConverter.ToUInt16(data, ConfigOffsets.JoystickCenterX);
            config.JoystickCenterY = BitConverter.ToUInt16(data, ConfigOffsets.JoystickCenterY);
            config.JoystickCalibrated = data[ConfigOffsets.JoystickCalibrated] != 0;
        }

        // v6 新增字段：光学传感器高级配置（版本>=6时有效；长度门槛按本块末尾字段）
        if (config.Version >= 6 && data.Length >= ConfigOffsets.DpiSwitchKey + 1)
        {
            // 4档 DPI 值
            for (int i = 0; i < 4; i++)
            {
                config.DpiSlots[i] = BitConverter.ToUInt16(data, ConfigOffsets.DpiSlot + i * 2);
            }
            config.CurrentDpiSlot = data[ConfigOffsets.CurrentDpiSlot];
            config.LodLevel = data[ConfigOffsets.LodLevel];
            config.AngleSnapping = data[ConfigOffsets.AngleSnapping] != 0;
            config.DpiSwitchKey = data[ConfigOffsets.DpiSwitchKey];

            // 使用当前档位的 DPI 值更新 Dpi 字段
            if (config.CurrentDpiSlot < 4 && config.DpiSlots[config.CurrentDpiSlot] > 0)
            {
                config.Dpi = config.DpiSlots[config.CurrentDpiSlot];
            }
        }

        // v7 新增字段：4层按键映射（版本>=7时有效）
        if (config.Version >= 7 && data.Length >= ConfigOffsets.Layer3Keymap + 64)
        {
            config.Layer2Keymap = new byte[64];
            Array.Copy(data, ConfigOffsets.Layer2Keymap, config.Layer2Keymap, 0, 64);
            config.Layer3Keymap = new byte[64];
            Array.Copy(data, ConfigOffsets.Layer3Keymap, config.Layer3Keymap, 0, 64);
        }

        // v8 新增字段：性能监控阈值（版本>=8时有效，8个 uint16）
        if (config.Version >= 8 && data.Length >= ConfigOffsets.PerfThresholds + 16)
        {
            for (int i = 0; i < 8; i++)
            {
                config.PerfThresholds[i] = BitConverter.ToUInt16(data, ConfigOffsets.PerfThresholds + i * 2);
            }
        }

        return config;
    }

    /// <summary>
    /// 序列化 DeviceConfig 对象为二进制配置数据
    /// </summary>
    /// <param name="config">DeviceConfig 对象</param>
    /// <returns>二进制配置数据（包含 CRC32 校验）</returns>
    public byte[] SerializeConfig(DeviceConfig config)
    {
        byte[] data = new byte[CONFIG_SIZE];

        // 魔数
        BitConverter.GetBytes(CONFIG_MAGIC).CopyTo(data, ConfigOffsets.Magic);

        // 版本
        BitConverter.GetBytes((ushort)config.Version).CopyTo(data, ConfigOffsets.Version);

        // DPI
        BitConverter.GetBytes(config.Dpi).CopyTo(data, ConfigOffsets.Dpi);

        // 摇杆死区
        BitConverter.GetBytes(config.JoystickDeadzone).CopyTo(data, ConfigOffsets.JoystickDeadzone);

        // 编码器方向
        data[ConfigOffsets.EncoderReverse] = (byte)(config.EncoderReverse ? 1 : 0);

        // 序列号（暂时用 0）
        BitConverter.GetBytes((ushort)0).CopyTo(data, ConfigOffsets.Seq);

        // 按键映射
        if (config.Keymap != null)
        {
            int len = Math.Min(64, config.Keymap.Length);
            Array.Copy(config.Keymap, 0, data, ConfigOffsets.Keymap, len);
        }

        // Fn 层按键映射
        if (config.FnKeymap != null)
        {
            int len = Math.Min(64, config.FnKeymap.Length);
            Array.Copy(config.FnKeymap, 0, data, ConfigOffsets.FnKeymap, len);
        }

        // v3 新增字段
        data[ConfigOffsets.JoyInvertX] = (byte)(config.JoystickInvertX ? 1 : 0);
        data[ConfigOffsets.JoyInvertY] = (byte)(config.JoystickInvertY ? 1 : 0);
        data[ConfigOffsets.EncoderSteps] = (byte)Math.Clamp(config.EncoderStepsPerTick, 0, 255);
        data[ConfigOffsets.EncoderScroll] = (byte)Math.Clamp(config.EncoderScrollSpeed, 0, 255);
        ushort sensRaw = (ushort)Math.Clamp((int)(config.JoystickSensitivity * 1000), 0, 65535);
        BitConverter.GetBytes(sensRaw).CopyTo(data, ConfigOffsets.JoystickSensitivity);

        // v4 新增字段
        BitConverter.GetBytes((ushort)CONFIG_SIZE).CopyTo(data, ConfigOffsets.Length);
        // write_count 由固件端维护，上位机写0，固件端会递增
        BitConverter.GetBytes((uint)0).CopyTo(data, ConfigOffsets.WriteCount);

        // v5 新增字段：摇杆校准数据
        BitConverter.GetBytes((ushort)config.JoystickCenterX).CopyTo(data, ConfigOffsets.JoystickCenterX);
        BitConverter.GetBytes((ushort)config.JoystickCenterY).CopyTo(data, ConfigOffsets.JoystickCenterY);
        data[ConfigOffsets.JoystickCalibrated] = (byte)(config.JoystickCalibrated ? 1 : 0);

        // v6 新增字段：光学传感器高级配置
        for (int i = 0; i < 4; i++)
        {
            BitConverter.GetBytes((ushort)config.DpiSlots[i]).CopyTo(data, ConfigOffsets.DpiSlot + i * 2);
        }
        data[ConfigOffsets.CurrentDpiSlot] = (byte)Math.Clamp((int)config.CurrentDpiSlot, 0, 3);
        data[ConfigOffsets.LodLevel] = (byte)Math.Clamp((int)config.LodLevel, 0, 2);
        data[ConfigOffsets.AngleSnapping] = (byte)(config.AngleSnapping ? 1 : 0);
        data[ConfigOffsets.DpiSwitchKey] = config.DpiSwitchKey;

        // v7 新增字段：4层按键映射
        if (config.Layer2Keymap != null)
        {
            int len = Math.Min(64, config.Layer2Keymap.Length);
            Array.Copy(config.Layer2Keymap, 0, data, ConfigOffsets.Layer2Keymap, len);
        }
        if (config.Layer3Keymap != null)
        {
            int len = Math.Min(64, config.Layer3Keymap.Length);
            Array.Copy(config.Layer3Keymap, 0, data, ConfigOffsets.Layer3Keymap, len);
        }

        // v8 新增字段：性能监控阈值（8个 uint16，默认全0=禁用告警；reserved5 保持清零）
        if (config.PerfThresholds != null)
        {
            int n = Math.Min(8, config.PerfThresholds.Length);
            for (int i = 0; i < n; i++)
            {
                BitConverter.GetBytes(config.PerfThresholds[i])
                    .CopyTo(data, ConfigOffsets.PerfThresholds + i * 2);
            }
        }

        // 同步更新 Dpi 字段为当前档位的值（向后兼容）
        if (config.CurrentDpiSlot < 4 && config.DpiSlots[config.CurrentDpiSlot] > 0)
        {
            BitConverter.GetBytes((ushort)config.DpiSlots[config.CurrentDpiSlot]).CopyTo(data, ConfigOffsets.Dpi);
        }

        // CRC32（与固件端 crc32_calc 算法一致：标准 IEEE 802.3 CRC32）
        uint crc = Crc32Calculate(data, 0, ConfigOffsets.Crc32);
        BitConverter.GetBytes(crc).CopyTo(data, ConfigOffsets.Crc32);

        return data;
    }

    /// <summary>
    /// 计算 CRC32（标准 IEEE 802.3，多项式 0xEDB88320 反转）
    /// 与固件端 crc32_calc 算法一致
    /// </summary>
    /// <param name="data">数据缓冲区</param>
    /// <param name="offset">起始偏移</param>
    /// <param name="length">计算长度</param>
    /// <returns>CRC32 校验值</returns>
    public static uint Crc32Calculate(byte[] data, int offset, int length)
    {
        uint crc = 0xFFFFFFFF;
        for (int i = offset; i < offset + length && i < data.Length; i++)
        {
            crc ^= data[i];
            for (int j = 0; j < 8; j++)
            {
                if ((crc & 1) != 0)
                    crc = (crc >> 1) ^ 0xEDB88320u;
                else
                    crc >>= 1;
            }
        }
        return ~crc;
    }

    /// <summary>
    /// 验证配置数据的魔数和 CRC32
    /// </summary>
    /// <param name="data">二进制配置数据</param>
    /// <returns>true=验证通过，false=验证失败</returns>
    public bool ValidateConfig(byte[] data)
    {
        if (data.Length < ConfigOffsets.Crc32 + 4)
            return false;

        // 验证魔数
        uint magic = BitConverter.ToUInt32(data, ConfigOffsets.Magic);
        if (magic != CONFIG_MAGIC)
            return false;

        // 验证 CRC32
        uint expectedCrc = BitConverter.ToUInt32(data, ConfigOffsets.Crc32);
        uint actualCrc = Crc32Calculate(data, 0, ConfigOffsets.Crc32);
        return expectedCrc == actualCrc;
    }
}
