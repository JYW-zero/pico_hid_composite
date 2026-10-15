/*
 * MacroService.cs
 * 宏管理服务
 *
 * 从 DeviceService.cs 提取，包含宏配置的读写和播放/停止控制。
 * 支持 8 个宏（ID 0-7），每个宏 148 字节，分 3 块传输。
 *
 * 与固件 firmware/src/protocol/config_hid.c 中的宏处理逻辑保持同步。
 */

namespace HidConfigTool.Core.Services;

using HidConfigTool.Core.Interfaces;
using HidConfigTool.Core.Models;

/// <summary>
/// 宏管理服务
/// 负责宏配置的读写和播放/停止控制
/// </summary>
public class MacroService
{
    // ==================== 常量（Report ID 和命令 ID 统一从 ProtocolConstants 引用） ====================

    /// <summary>
    /// 宏配置读写的 Report ID
    /// </summary>
    public const byte REPORT_ID_MACRO_CONFIG = ProtocolConstants.ReportIdMacroConfig;

    /// <summary>
    /// 宏配置报告大小（不含 Report ID）
    /// </summary>
    public const int MACRO_REPORT_SIZE = 62;

    /// <summary>
    /// 每块数据大小（去掉2字节头）
    /// </summary>
    public const int MACRO_BLOCK_DATA_SIZE = 60;

    /// <summary>
    /// 宏数据总大小（字节）
    /// </summary>
    public const int MACRO_DATA_SIZE = 148;

    /// <summary>
    /// 宏的数量（ID 0-7）
    /// </summary>
    public const int MACRO_COUNT = 8;

    /// <summary>
    /// 播放宏命令
    /// </summary>
    public const byte CMD_MACRO_PLAY = ProtocolConstants.CmdMacroPlay;

    /// <summary>
    /// 停止宏命令（参数：宏ID，0xFF=停止所有）
    /// </summary>
    public const byte CMD_MACRO_STOP = ProtocolConstants.CmdMacroStop;

    // ==================== 依赖注入 ====================

    private readonly IHidDriver _hidDriver;
    private readonly SemaphoreSlim _writeLock;
    private readonly Func<bool> _isConnectedFunc;
    private readonly Action<string, string> _logAction;
    private readonly Func<byte, byte, Task<bool>> _sendControlCommandFunc;

    /// <summary>
    /// 构造函数
    /// </summary>
    /// <param name="hidDriver">HID 驱动</param>
    /// <param name="writeLock">写入锁（用于串行化 HID 写入操作）</param>
    /// <param name="isConnectedFunc">设备连接状态检查函数</param>
    /// <param name="logAction">日志记录函数（参数：级别, 消息）</param>
    /// <param name="sendControlCommandFunc">发送控制命令函数（参数：命令字节, 参数字节）</param>
    public MacroService(
        IHidDriver hidDriver,
        SemaphoreSlim writeLock,
        Func<bool> isConnectedFunc,
        Action<string, string> logAction,
        Func<byte, byte, Task<bool>> sendControlCommandFunc)
    {
        _hidDriver = hidDriver ?? throw new ArgumentNullException(nameof(hidDriver));
        _writeLock = writeLock ?? throw new ArgumentNullException(nameof(writeLock));
        _isConnectedFunc = isConnectedFunc ?? throw new ArgumentNullException(nameof(isConnectedFunc));
        _logAction = logAction ?? throw new ArgumentNullException(nameof(logAction));
        _sendControlCommandFunc = sendControlCommandFunc ?? throw new ArgumentNullException(nameof(sendControlCommandFunc));
    }

    // ==================== 宏配置读写 ====================

    /// <summary>
    /// 读取一个宏的配置数据
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <returns>宏的原始字节数据，失败返回null</returns>
    public async Task<byte[]?> GetMacroAsync(byte macroId)
    {
        if (!_isConnectedFunc() || macroId >= MACRO_COUNT)
            return null;

        try
        {
            await _writeLock.WaitAsync();
            try
            {
                byte[] macroData = new byte[MACRO_DATA_SIZE];
                int bytesRead = 0;

                // 分3块读取
                for (byte block = 0; block < 3; block++)
                {
                    // 1. 先设置读取索引（SET_REPORT）
                    // block最高位设为1，表示这是读取命令，只设置索引，不写入数据
                    byte[] indexData = new byte[MACRO_REPORT_SIZE];
                    indexData[0] = macroId;
                    indexData[1] = (byte)(0x80 | block);  // 最高位=1表示读命令
                    await _hidDriver.SendFeatureReportAsync(REPORT_ID_MACRO_CONFIG, indexData);

                    // 短暂延迟，确保设备处理
                    await Task.Delay(5);

                    // 2. 再读取数据（GET_REPORT）
                    byte[]? blockData = await _hidDriver.GetFeatureReportAsync(REPORT_ID_MACRO_CONFIG);
                    if (blockData == null || blockData.Length < 2)
                    {
                        _logAction("ERROR", $"读取宏 {macroId} 块 {block} 失败");
                        return null;
                    }

                    // 3. 复制数据（前2字节是宏ID和块号，后面是数据）
                    int offset = block * MACRO_BLOCK_DATA_SIZE;
                    int copyLen = Math.Min(MACRO_BLOCK_DATA_SIZE, blockData.Length - 2);
                    if (offset + copyLen > macroData.Length)
                        copyLen = macroData.Length - offset;

                    Array.Copy(blockData, 2, macroData, offset, copyLen);
                    bytesRead += copyLen;
                }

                _logAction("INFO", $"读取宏 {macroId} 成功，共 {bytesRead} 字节");
                return macroData;
            }
            finally
            {
                _writeLock.Release();
            }
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"读取宏 {macroId} 异常: {ex.Message}");
            return null;
        }
    }

    /// <summary>
    /// 写入一个宏的配置数据
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <param name="data">宏的原始字节数据</param>
    /// <returns>是否成功</returns>
    public async Task<bool> SetMacroAsync(byte macroId, byte[] data)
    {
        if (!_isConnectedFunc() || macroId >= MACRO_COUNT || data == null || data.Length == 0)
            return false;

        try
        {
            await _writeLock.WaitAsync();
            try
            {
                // 分3块写入
                for (byte block = 0; block < 3; block++)
                {
                    int offset = block * MACRO_BLOCK_DATA_SIZE;
                    if (offset >= data.Length)
                        break;

                    int copyLen = Math.Min(MACRO_BLOCK_DATA_SIZE, data.Length - offset);

                    // 构造数据：[宏ID, 块号, ...数据...]
                    byte[] blockData = new byte[MACRO_REPORT_SIZE];
                    blockData[0] = macroId;
                    blockData[1] = block;
                    Array.Copy(data, offset, blockData, 2, copyLen);

                    // 写入
                    bool ok = await _hidDriver.SendFeatureReportAsync(REPORT_ID_MACRO_CONFIG, blockData);
                    if (!ok)
                    {
                        _logAction("ERROR", $"写入宏 {macroId} 块 {block} 失败");
                        return false;
                    }

                    // 短暂延迟
                    await Task.Delay(5);
                }

                _logAction("INFO", $"写入宏 {macroId} 成功，共 {data.Length} 字节");
                return true;
            }
            finally
            {
                _writeLock.Release();
            }
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"写入宏 {macroId} 异常: {ex.Message}");
            return false;
        }
    }

    // ==================== 宏播放控制 ====================

    /// <summary>
    /// 播放指定宏
    /// </summary>
    /// <param name="macroId">宏ID (0-7)</param>
    /// <returns>是否成功发送命令</returns>
    public async Task<bool> PlayMacroAsync(byte macroId)
    {
        if (!_isConnectedFunc() || macroId >= MACRO_COUNT)
            return false;

        try
        {
            bool result = await _sendControlCommandFunc(CMD_MACRO_PLAY, macroId);
            if (result)
            {
                _logAction("INFO", $"播放宏 {macroId}");
            }
            else
            {
                _logAction("ERROR", $"播放宏 {macroId} 失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"播放宏 {macroId} 异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 停止指定宏
    /// </summary>
    /// <param name="macroId">宏ID (0-7)，0xFF 表示停止所有宏</param>
    /// <returns>是否成功发送命令</returns>
    public async Task<bool> StopMacroAsync(byte macroId)
    {
        if (!_isConnectedFunc())
            return false;

        try
        {
            bool result = await _sendControlCommandFunc(CMD_MACRO_STOP, macroId);
            if (result)
            {
                _logAction("INFO", macroId == 0xFF ? "停止所有宏" : $"停止宏 {macroId}");
            }
            else
            {
                _logAction("ERROR", $"停止宏 {macroId} 失败");
            }
            return result;
        }
        catch (Exception ex)
        {
            _logAction("ERROR", $"停止宏 {macroId} 异常: {ex.Message}");
            return false;
        }
    }

    /// <summary>
    /// 停止所有宏
    /// </summary>
    /// <returns>是否成功发送命令</returns>
    public Task<bool> StopAllMacrosAsync()
    {
        return StopMacroAsync(0xFF);
    }
}
