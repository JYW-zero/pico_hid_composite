using System.Runtime.InteropServices;
using System.Windows.Input;

namespace HidConfigTool.App.Services;

/// <summary>
/// 键盘事件参数
/// </summary>
public class KeyHookEventArgs : EventArgs
{
    public Key Key { get; set; }
    public bool IsPressed { get; set; }
    public bool IsHandled { get; set; }
    public DateTime Timestamp { get; set; } = DateTime.Now;
}

/// <summary>
/// 全局键盘钩子
/// 监听全局键盘事件，用于宏录制
/// </summary>
public class KeyboardHook : IDisposable
{
    private delegate IntPtr LowLevelKeyboardProc(int nCode, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    private static extern IntPtr SetWindowsHookEx(int idHook, LowLevelKeyboardProc lpfn, IntPtr hMod, uint dwThreadId);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool UnhookWindowsHookEx(IntPtr hhk);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    private static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    private static extern IntPtr GetModuleHandle(string lpModuleName);

    private const int WH_KEYBOARD_LL = 13;
    private const int WM_KEYDOWN = 0x0100;
    private const int WM_KEYUP = 0x0101;
    private const int WM_SYSKEYDOWN = 0x0104;
    private const int WM_SYSKEYUP = 0x0105;

    [StructLayout(LayoutKind.Sequential)]
    private struct KBDLLHOOKSTRUCT
    {
        public uint vkCode;
        public uint scanCode;
        public uint flags;
        public uint time;
        public IntPtr dwExtraInfo;
    }

    private IntPtr _hookId = IntPtr.Zero;
    private LowLevelKeyboardProc? _proc;

    /// <summary>
    /// 键盘事件
    /// </summary>
    public event EventHandler<KeyHookEventArgs>? KeyEvent;

    /// <summary>
    /// 是否已安装钩子
    /// </summary>
    public bool IsHooked => _hookId != IntPtr.Zero;

    /// <summary>
    /// 安装钩子
    /// </summary>
    public bool Hook()
    {
        if (IsHooked)
            return true;

        _proc = HookCallback;

        using var curProcess = System.Diagnostics.Process.GetCurrentProcess();
        using var curModule = curProcess.MainModule;
        if (curModule == null || curModule.ModuleName == null)
            return false;

        _hookId = SetWindowsHookEx(WH_KEYBOARD_LL, _proc, GetModuleHandle(curModule.ModuleName), 0);
        return _hookId != IntPtr.Zero;
    }

    /// <summary>
    /// 卸载钩子
    /// </summary>
    public void Unhook()
    {
        if (!IsHooked)
            return;

        UnhookWindowsHookEx(_hookId);
        _hookId = IntPtr.Zero;
        _proc = null;
    }

    private IntPtr HookCallback(int nCode, IntPtr wParam, IntPtr lParam)
    {
        if (nCode >= 0 && KeyEvent != null)
        {
            var kbd = (KBDLLHOOKSTRUCT)Marshal.PtrToStructure(lParam, typeof(KBDLLHOOKSTRUCT))!;
            bool isPressed = (wParam == (IntPtr)WM_KEYDOWN || wParam == (IntPtr)WM_SYSKEYDOWN);

            try
            {
                Key key = KeyInterop.KeyFromVirtualKey((int)kbd.vkCode);

                var args = new KeyHookEventArgs
                {
                    Key = key,
                    IsPressed = isPressed,
                    Timestamp = DateTime.Now
                };

                KeyEvent?.Invoke(this, args);

                if (args.IsHandled)
                    return new IntPtr(1);
            }
            catch
            {
                // 忽略转换错误
            }
        }

        return CallNextHookEx(_hookId, nCode, wParam, lParam);
    }

    public void Dispose()
    {
        Unhook();
    }
}
