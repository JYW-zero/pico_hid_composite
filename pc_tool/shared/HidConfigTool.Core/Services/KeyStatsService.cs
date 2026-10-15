/*
 * KeyStatsService.cs
 * 按键统计服务
 *
 * 从 DeviceService.cs 提取，包含按键统计数据的读取和清零功能。
 * 支持 64 个按键的统计数据，分 4 块传输（每块 16 个按键）。
 *
 * 与固件 firmware/src/app/main.c 中的按键统计逻辑保持同步。
 */

namespace HidConfigTool.Core.Services;

using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

/// <summary>
/// 按键统计服务
/// 负责按键统计数据的读取和清零
/// </summary>
public class KeyStatsService
{
    // ==================== 常量（统一从 ProtocolConstants 引用，避免重复定义） ====================

    /// <summary>
    /// 按键统计块 0 的 Report ID（键 0~15）
    /// </summary>
    public const byte REPORT_ID_KEY_STATS0 = ProtocolConstants.ReportIdKeyStats0;

    /// <summary>
    /// 按键统计块 1 的 Report ID（键 16~31）
    /// </summary>
    public const byte REPORT_ID_KEY_STATS1 = ProtocolConstants.ReportIdKeyStats1;

    /// <summary>
    /// 按键统计块 2 的 Report ID（键 32~47）
    /// </summary>
    public const byte REPORT_ID_KEY_STATS2 = ProtocolConstants.ReportIdKeyStats2;

    /// <summary>
    /// 按键统计块 3 的 Report ID（键 48~63）
    /// </summary>
    public const byte REPORT_ID_KEY_STATS3 = ProtocolConstants.ReportIdKeyStats3;

    /// <summary>
    /// 清零按键统计命令
    /// </summary>
    public const byte CMD_RESET_STATS = ProtocolConstants.CmdResetStats;

    /// <summary>
    /// 按键总数
    /// </summary>
    public const int KEY_COUNT = 64;

    /// <summary>
    /// 每块按键数量
    /// </summary>
    public const int KEYS_PER_BLOCK = 16;

    /// <summary>
    /// 每块数据大小（字节）
    /// </summary>
    public const int BLOCK_DATA_SIZE = 32;

    // ==================== 依赖注入 ====================

    private readonly IHidDriver _hidDriver;
    private readonly Func<bool> _isConnectedFunc;
    private readonly Action<string, string> _logAction;
    private readonly Func<byte, Task<bool>> _sendControlCommandFunc;

    /// <summary>
    /// 构造函数
    /// </summary>
    /// <param name="hidDriver">HID 驱动</param>
    /// <param name="isConnectedFunc">设备连接状态检查函数</param>
    /// <param name="logAction">日志记录函数（参数：级别, 消息）</param>
    /// <param name="sendControlCommandFunc">发送控制命令函数（参数：命令字节）</param>
    public KeyStatsService(
        IHidDriver hidDriver,
        Func<bool> isConnectedFunc,
        Action<string, string> logAction,
        Func<byte, Task<bool>> sendControlCommandFunc)
    {
        _hidDriver = hidDriver ?? throw new ArgumentNullException(nameof(hidDriver));
        _isConnectedFunc = isConnectedFunc ?? throw new ArgumentNullException(nameof(isConnectedFunc));
        _logAction = logAction ?? throw new ArgumentNullException(nameof(logAction));
        _sendControlCommandFunc = sendControlCommandFunc ?? throw new ArgumentNullException(nameof(sendControlCommandFunc));
    }

    // ==================== 按键统计读取 ====================

    /// <summary>
    /// 获取按键统计数据
    /// </summary>
    /// <returns>64个按键的统计数据数组，失败返回null</returns>
    public async Task<uint[]?> GetKeyStatsAsync()
    {
        if (!_isConnectedFunc())
            return null;

        try
        {
            uint[] stats = new uint[KEY_COUNT];
            byte[] reportIds = { REPORT_ID_KEY_STATS0, REPORT_ID_KEY_STATS1, REPORT_ID_KEY_STATS2, REPORT_ID_KEY_STATS3 };

            for (int block = 0; block < 4; block++)
            {
                byte[]? data = await _hidDriver.GetFeatureReportAsync(reportIds[block]);
                if (data == null || data.Length < BLOCK_DATA_SIZE)
                {
                    _logAction("WARNING", $"读取按键统计块 {block} 失败");
                    return null;
                }

                // 解析16个uint16_t（小端）
                for (int i = 0; i < KEYS_PER_BLOCK; i++)
                {
                    int idx = block * KEYS_PER_BLOCK + i;
                    if (idx >= KEY_COUNT) break;
                    ushort val = (ushort)(data[i * 2] | (data[i * 2 + 1] << 8));
                    stats[idx] = val;
                }
            }

            return stats;
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"读取按键统计失败: {ex.Message}");
            return null;
        }
    }

    // ==================== 按键统计清零 ====================

    /// <summary>
    /// 清零按键统计
    /// </summary>
    /// <returns>是否成功</returns>
    public async Task<bool> ResetKeyStatsAsync()
    {
        if (!_isConnectedFunc())
            return false;

        try
        {
            return await _sendControlCommandFunc(CMD_RESET_STATS);
        }
        catch
        {
            return false;
        }
    }

    // ==================== 辅助方法 ====================

    /// <summary>
    /// 获取按键统计的总和
    /// </summary>
    /// <param name="stats">按键统计数据</param>
    /// <returns>所有按键统计的总和</returns>
    public static uint GetTotalKeyPresses(uint[]? stats)
    {
        if (stats == null) return 0;
        uint total = 0;
        foreach (uint count in stats)
        {
            total += count;
        }
        return total;
    }

    /// <summary>
    /// 获取按键统计的最大值
    /// </summary>
    /// <param name="stats">按键统计数据</param>
    /// <returns>最大按键统计值</returns>
    public static uint GetMaxKeyPresses(uint[]? stats)
    {
        if (stats == null || stats.Length == 0) return 0;
        uint max = 0;
        foreach (uint count in stats)
        {
            if (count > max) max = count;
        }
        return max;
    }

    /// <summary>
    /// 获取按键统计的平均值
    /// </summary>
    /// <param name="stats">按键统计数据</param>
    /// <returns>平均按键统计值</returns>
    public static double GetAverageKeyPresses(uint[]? stats)
    {
        if (stats == null || stats.Length == 0) return 0;
        uint total = GetTotalKeyPresses(stats);
        return (double)total / stats.Length;
    }
}
