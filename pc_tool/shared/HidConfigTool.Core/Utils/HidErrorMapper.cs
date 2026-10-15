namespace HidConfigTool.Core.Utils;

/// <summary>
/// HID 错误码映射工具
/// 将 Windows Win32 错误码映射为用户可读的中文信息
/// </summary>
public static class HidErrorMapper
{
    /// <summary>
    /// Win32 错误码到用户可读信息的映射
    /// </summary>
    private static readonly Dictionary<int, string> _errorMessages = new()
    {
        // 通用错误
        { 0, "操作成功" },
        { 1, "函数不正确" },
        { 2, "系统找不到指定的文件" },
        { 3, "系统找不到指定的路径" },
        { 5, "拒绝访问（设备可能被其他程序占用）" },
        { 6, "句柄无效" },
        { 8, "存储空间不足，无法处理此命令" },
        { 13, "数据无效" },
        { 21, "设备未就绪" },
        { 31, "连到系统上的设备没有发挥作用" },
        { 32, "另一个程序正在使用此文件，进程无法访问" },
        { 33, "另一个程序已锁定文件的一部分，进程无法访问" },
        { 50, "不支持该请求" },
        { 55, "指定的网络资源或设备不再可用" },
        { 59, "发生了意外的网络错误" },
        { 87, "参数错误" },
        { 995, "由于线程退出或应用程序请求，已中止 I/O 操作" },
        { 996, "重叠 I/O 事件不在信号状态中" },
        { 997, "重叠 I/O 操作在进行中" },
        { 1004, "无效的标志" },

        // HID 特定错误
        { 1167, "设备未连接或已断开" },
        { 121, "信号灯超时时间已到（设备响应超时）" },
        { 1450, "系统资源不足，无法完成请求的服务" },
        { 1451, "系统资源不足，无法完成请求的服务" },
        { 1452, "系统资源不足，无法完成请求的服务" },
        { 1453, "配额不足，无法完成请求的服务" },
        { 1454, "配额不足，无法完成请求的服务" },
        { 1455, "页面文件太小，无法完成操作" },

        // USB 相关错误
        { 433, "指定的设备不存在或未连接" },
        { 434, "设备连接已断开" },
        { 435, "设备忙，请稍后重试" },
        { 436, "设备通信超时" },
        { 437, "设备返回数据格式错误" },
        { 438, "设备固件版本不兼容" },
        { 439, "设备配置锁定，需要先解锁" },
        { 440, "设备 CRC 校验失败" },
    };

    /// <summary>
    /// 将 Win32 错误码映射为用户可读信息
    /// </summary>
    /// <param name="errorCode">Win32 错误码</param>
    /// <returns>用户可读的中文错误信息</returns>
    public static string GetErrorMessage(int errorCode)
    {
        if (_errorMessages.TryGetValue(errorCode, out string? message))
        {
            return message;
        }
        return $"未知错误（错误码: {errorCode}）";
    }

    /// <summary>
    /// 获取最后一个 Win32 错误的用户可读信息
    /// </summary>
    /// <returns>用户可读的中文错误信息</returns>
    public static string GetLastErrorMessage()
    {
        int errorCode = System.Runtime.InteropServices.Marshal.GetLastWin32Error();
        return GetErrorMessage(errorCode);
    }

    /// <summary>
    /// 判断错误是否可重试（临时性错误）
    /// </summary>
    /// <param name="errorCode">Win32 错误码</param>
    /// <returns>是否可重试</returns>
    public static bool IsRetryable(int errorCode)
    {
        return errorCode switch
        {
            21 => true,
            32 => true,
            33 => true,
            55 => true,
            59 => true,
            121 => true,
            1167 => true,
            1450 => true,
            _ => false
        };
    }

    /// <summary>
    /// 判断异常是否可重试
    /// </summary>
    /// <param name="ex">异常</param>
    /// <returns>是否可重试</returns>
    public static bool IsRetryable(Exception ex)
    {
        if (ex is OperationCanceledException || ex is TimeoutException)
            return true;
        if (ex is IOException)
            return true;
        return false;
    }

    /// <summary>
    /// 获取固件 HID 协议 ACK 错误码对应的中文消息（对应固件 config_hid_error_t）。
    /// 这是设备通过 ACK 报告返回的<b>固件协议错误码</b>（0-12、0xFF），
    /// 与 <see cref="GetErrorMessage"/> 映射的 Win32/HIDAPI 传输层错误码属于两套命名空间，
    /// 切勿混用（例如固件 9 = 功能不支持，而 Win32 9 = 存储控制块地址无效）。
    /// </summary>
    /// <param name="protocolErrorCode">固件 ACK 错误码（HidCommandResult.ErrorCode）</param>
    /// <returns>面向用户的中文错误消息</returns>
    public static string GetProtocolErrorMessage(byte protocolErrorCode)
    {
        return protocolErrorCode switch
        {
            0 => "成功",
            1 => "设备不认识该命令（无效命令）",
            2 => "命令参数无效",
            3 => "设备配置已锁定，请先解锁",
            4 => "数据校验失败（CRC 不匹配）",
            5 => "配置数据不完整，请重新下发全部配置块",
            6 => "设备 Flash 写入失败，请重试",
            7 => "设备操作超时，请重试",
            8 => "设备忙，请稍后再试",
            9 => "设备不支持该功能（当前固件未编入此能力）",
            10 => "协议版本不兼容，请升级固件或上位机",
            11 => "诊断功能未授权：设备已出厂锁定，需售后解锁后再操作",
            12 => "制造功能已锁定：工厂/制造命令不可用",
            0xFF => "设备返回未知错误",
            _ => $"设备返回未知错误码: {protocolErrorCode}"
        };
    }
}
