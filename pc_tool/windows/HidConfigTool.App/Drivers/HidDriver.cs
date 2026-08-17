using System.Runtime.InteropServices;
using System.IO;
using System.Text;
using HidConfigTool.Core.Interfaces;
using Microsoft.Win32.SafeHandles;

namespace HidConfigTool.App.Drivers;

/// <summary>
/// HID 驱动实现（P/Invoke 调用 Windows 原生 HID API）
/// 100% 官方 Windows 系统 API，无需额外 NuGet 包
/// </summary>
public class HidDriver : IHidDriver
{
    #region Win32 API 定义

    private const uint GENERIC_READ = 0x80000000;
    private const uint GENERIC_WRITE = 0x40000000;
    private const uint FILE_SHARE_READ = 0x00000001;
    private const uint FILE_SHARE_WRITE = 0x00000002;
    private const uint OPEN_EXISTING = 3;
    private const uint FILE_ATTRIBUTE_NORMAL = 0x80;

    private const int HIDP_STATUS_SUCCESS = 0x00110000;

    // IOCTL 控制码（按照 CTL_CODE 宏计算：DeviceType << 16 | Access << 14 | Function << 2 | Method）
    // FILE_DEVICE_HID = 0x0000000B, FILE_ANY_ACCESS = 0, METHOD_BUFFERED = 0
    private const uint IOCTL_HID_GET_FEATURE = (0x000B << 16) | (0 << 14) | (2 << 2) | 0;  // 0x000B0008
    private const uint IOCTL_HID_SET_FEATURE = (0x000B << 16) | (0 << 14) | (3 << 2) | 0;  // 0x000B000C

    [StructLayout(LayoutKind.Sequential)]
    private struct HIDD_ATTRIBUTES
    {
        public int Size;
        public ushort VendorID;
        public ushort ProductID;
        public ushort VersionNumber;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct HIDP_CAPS
    {
        public ushort Usage;
        public ushort UsagePage;
        public ushort InputReportByteLength;
        public ushort OutputReportByteLength;
        public ushort FeatureReportByteLength;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 17)]
        public ushort[] Reserved;
        public ushort NumberLinkCollectionNodes;
        public ushort NumberInputButtonCaps;
        public ushort NumberInputValueCaps;
        public ushort NumberInputDataIndices;
        public ushort NumberOutputButtonCaps;
        public ushort NumberOutputValueCaps;
        public ushort NumberOutputDataIndices;
        public ushort NumberFeatureButtonCaps;
        public ushort NumberFeatureValueCaps;
        public ushort NumberFeatureDataIndices;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct SP_DEVICE_INTERFACE_DATA
    {
        public int cbSize;
        public Guid InterfaceClassGuid;
        public int Flags;
        public IntPtr Reserved;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
    private struct SP_DEVICE_INTERFACE_DETAIL_DATA
    {
        public int cbSize;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string DevicePath;
    }

    [DllImport("hid.dll", SetLastError = true)]
    private static extern void HidD_GetHidGuid(out Guid Guid);

    [DllImport("hid.dll", SetLastError = true)]
    private static extern bool HidD_GetAttributes(SafeFileHandle HidDeviceObject, ref HIDD_ATTRIBUTES Attributes);

    [DllImport("hid.dll", SetLastError = true)]
    private static extern bool HidD_GetFeature(SafeFileHandle HidDeviceObject, byte[] ReportBuffer, uint ReportBufferLength);

    [DllImport("hid.dll", SetLastError = true)]
    private static extern bool HidD_SetFeature(SafeFileHandle HidDeviceObject, byte[] ReportBuffer, uint ReportBufferLength);

    [DllImport("hid.dll", SetLastError = true)]
    private static extern bool HidD_GetPreparsedData(SafeFileHandle HidDeviceObject, out IntPtr PreparsedData);

    [DllImport("hid.dll", SetLastError = true)]
    private static extern bool HidD_FreePreparsedData(IntPtr PreparsedData);

    [DllImport("hid.dll", SetLastError = true)]
    private static extern uint HidP_GetCaps(IntPtr PreparsedData, out HIDP_CAPS Capabilities);

    [DllImport("hid.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern bool HidD_GetProductString(SafeFileHandle HidDeviceObject, StringBuilder Buffer, uint BufferLength);

    [DllImport("hid.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern bool HidD_GetManufacturerString(SafeFileHandle HidDeviceObject, StringBuilder Buffer, uint BufferLength);

    [DllImport("hid.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern bool HidD_GetSerialNumberString(SafeFileHandle HidDeviceObject, StringBuilder Buffer, uint BufferLength);

    [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern IntPtr SetupDiGetClassDevs(ref Guid ClassGuid, IntPtr Enumerator, IntPtr hwndParent, uint Flags);

    [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern bool SetupDiEnumDeviceInterfaces(IntPtr DeviceInfoSet, IntPtr DeviceInfoData, ref Guid InterfaceClassGuid, uint MemberIndex, ref SP_DEVICE_INTERFACE_DATA DeviceInterfaceData);

    [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr DeviceInfoSet, ref SP_DEVICE_INTERFACE_DATA DeviceInterfaceData, ref SP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData, uint DeviceInterfaceDetailDataSize, out uint RequiredSize, IntPtr DeviceInfoData);

    [DllImport("setupapi.dll", SetLastError = true)]
    private static extern bool SetupDiDestroyDeviceInfoList(IntPtr DeviceInfoSet);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern SafeFileHandle CreateFile(string lpFileName, uint dwDesiredAccess, uint dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool DeviceIoControl(SafeFileHandle hDevice, uint dwIoControlCode, byte[] lpInBuffer, uint nInBufferSize, byte[] lpOutBuffer, uint nOutBufferSize, out uint lpBytesReturned, IntPtr lpOverlapped);

    private const uint DIGCF_PRESENT = 0x00000002;
    private const uint DIGCF_DEVICEINTERFACE = 0x00000010;

    #endregion

    private SafeFileHandle? _deviceHandle;
    private uint _featureReportSize = 65;  // 默认 65 字节（Report ID + 64 数据），OpenAsync 时探测实际值

    /// <summary>
    /// 写日志到文件
    /// </summary>
    private static void Log(string msg)
    {
        try
        {
            string logDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "HIDConfigTool", "Logs");
            Directory.CreateDirectory(logDir);
            string logPath = Path.Combine(logDir, "hid_driver.log");
            File.AppendAllText(logPath, $"[{DateTime.Now:HH:mm:ss.fff}] {msg}\n");
        }
        catch { }
    }

    /// <inheritdoc />
    public ushort VendorId { get; private set; }

    /// <inheritdoc />
    public ushort ProductId { get; private set; }

    /// <inheritdoc />
    public bool IsConnected => _deviceHandle != null && !_deviceHandle.IsInvalid;

    /// <inheritdoc />
    public async Task<IReadOnlyList<HidDeviceInfo>> FindDevicesAsync(ushort vendorId, ushort productId)
    {
        var result = new List<HidDeviceInfo>();

        await Task.Run(() =>
        {
            try
            {
                Log($"FindDevicesAsync start, VID=0x{vendorId:X4}, PID=0x{productId:X4}");

                Guid hidGuid;
                HidD_GetHidGuid(out hidGuid);
                Log($"HID GUID: {hidGuid}");

                IntPtr deviceInfoSet = SetupDiGetClassDevs(ref hidGuid, IntPtr.Zero, IntPtr.Zero, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
                if (deviceInfoSet == new IntPtr(-1))
                {
                    Log("SetupDiGetClassDevs failed");
                    return;
                }
                Log($"SetupDiGetClassDevs success, handle=0x{deviceInfoSet.ToInt64():X}");

                try
                {
                    uint index = 0;
                    int foundCount = 0;
                    int detailFailed = 0;
                    int openFailed = 0;
                    int attrsFailed = 0;
                    int vidPidMismatch = 0;

                    var interfaceData = new SP_DEVICE_INTERFACE_DATA();
                    interfaceData.cbSize = Marshal.SizeOf(interfaceData);
                    Log($"SP_DEVICE_INTERFACE_DATA cbSize={interfaceData.cbSize}");

                    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, IntPtr.Zero, ref hidGuid, index, ref interfaceData))
                    {
                        index++;
                        foundCount++;

                        var detailData = new SP_DEVICE_INTERFACE_DETAIL_DATA();
                        // 注意：cbSize 必须是只有 1 个字符时的结构体大小，用于版本控制
                        // 不是整个缓冲区的大小
                        // 64位系统 cbSize=8，32位系统 cbSize=6（Unicode）
                        detailData.cbSize = IntPtr.Size == 8 ? 8 : 6;
                        Log($"  [{index}] detailData.cbSize={detailData.cbSize}, SystemDefaultCharSize={Marshal.SystemDefaultCharSize}");

                        uint requiredSize = 0;
                        if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, ref interfaceData, ref detailData, 1024, out requiredSize, IntPtr.Zero))
                        {
                            int err = Marshal.GetLastWin32Error();
                            Log($"  [{index}] SetupDiGetDeviceInterfaceDetail failed, err={err}, requiredSize={requiredSize}");
                            detailFailed++;
                            continue;
                        }

                        string devicePath = detailData.DevicePath;
                        Log($"  [{index}] devicePath={devicePath}");

                        // 尝试打开设备读取 VID/PID
                        try
                        {
                            SafeFileHandle handle = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, IntPtr.Zero, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, IntPtr.Zero);
                            if (handle.IsInvalid)
                            {
                                int err = Marshal.GetLastWin32Error();
                                Log($"  [{index}] CreateFile failed, err={err}");
                                openFailed++;
                                continue;
                            }

                            try
                            {
                                var attrs = new HIDD_ATTRIBUTES();
                                attrs.Size = Marshal.SizeOf(attrs);

                                if (!HidD_GetAttributes(handle, ref attrs))
                                {
                                    int err = Marshal.GetLastWin32Error();
                                    Log($"  [{index}] HidD_GetAttributes failed, err={err}");
                                    attrsFailed++;
                                    continue;
                                }

                                Log($"  [{index}] VID=0x{attrs.VendorID:X4}, PID=0x{attrs.ProductID:X4}");

                                if (attrs.VendorID != vendorId || attrs.ProductID != productId)
                                {
                                    vidPidMismatch++;
                                    continue;
                                }

                                var info = new HidDeviceInfo
                                {
                                    DevicePath = devicePath,
                                    VendorId = attrs.VendorID,
                                    ProductId = attrs.ProductID,
                                    ProductName = GetProductString(handle),
                                    ManufacturerName = GetManufacturerString(handle),
                                    SerialNumber = GetSerialNumberString(handle),
                                    UsagePage = GetUsagePage(handle),
                                    UsageId = GetUsageId(handle)
                                };

                                result.Add(info);
                                Log($"  [{index}] Added to list!");
                            }
                            finally
                            {
                                handle.Close();
                            }
                        }
                        catch (Exception ex)
                        {
                            Log($"  [{index}] Exception: {ex.Message}");
                        }
                    }

                    Log($"Enum done: total={foundCount}, detailFailed={detailFailed}, openFailed={openFailed}, attrsFailed={attrsFailed}, vidPidMismatch={vidPidMismatch}, matched={result.Count}");
                }
                finally
                {
                    SetupDiDestroyDeviceInfoList(deviceInfoSet);
                }
            }
            catch (Exception ex)
            {
                Log($"Fatal exception: {ex}");
            }
        });

        Log($"FindDevicesAsync done, found {result.Count} devices");
        return result;
    }

    /// <inheritdoc />
    public async Task<bool> OpenAsync(string devicePath)
    {
        Close();

        return await Task.Run(() =>
        {
            try
            {
                // 独占访问：防止其他程序同时操作设备（竞争条件/信息泄露）
                _deviceHandle = CreateFile(devicePath, GENERIC_READ | GENERIC_WRITE, 0, IntPtr.Zero, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, IntPtr.Zero);

                if (_deviceHandle.IsInvalid)
                    return false;

                var attrs = new HIDD_ATTRIBUTES();
                attrs.Size = Marshal.SizeOf(attrs);

                if (HidD_GetAttributes(_deviceHandle, ref attrs))
                {
                    VendorId = attrs.VendorID;
                    ProductId = attrs.ProductID;
                }

                // 探测 Feature Report 实际长度
                if (HidD_GetPreparsedData(_deviceHandle, out IntPtr preparsed))
                {
                    if (HidP_GetCaps(preparsed, out HIDP_CAPS caps) == (uint)HIDP_STATUS_SUCCESS)
                    {
                        _featureReportSize = caps.FeatureReportByteLength;
                        if (_featureReportSize == 0) _featureReportSize = 65;
                    }
                    HidD_FreePreparsedData(preparsed);
                }

                return true;
            }
            catch
            {
                return false;
            }
        });
    }

    /// <inheritdoc />
    public void Close()
    {
        _deviceHandle?.Close();
        _deviceHandle = null;
    }

    /// <inheritdoc />
    public async Task<bool> SendFeatureReportAsync(byte reportId, byte[] data)
    {
        if (!IsConnected)
            return false;

        return await Task.Run(() =>
        {
            try
            {
                // 使用局部变量捕获句柄，避免 IsConnected 检查与使用之间的 TOCTOU 竞态
                SafeFileHandle? handle = _deviceHandle;
                if (handle == null || handle.IsInvalid)
                    return false;

                // 构造 Feature 报告缓冲区，使用探测到的 FeatureReportByteLength
                byte[] buffer = new byte[_featureReportSize];
                buffer[0] = reportId;
                Array.Copy(data, 0, buffer, 1, Math.Min(data.Length, buffer.Length - 1));

                bool result = HidD_SetFeature(handle, buffer, (uint)buffer.Length);

                if (!result)
                {
                    int err = Marshal.GetLastWin32Error();
                    Log($"SendFeatureReportAsync failed: reportId={reportId}, error={err}");
                }

                return result;
            }
            catch (Exception ex)
            {
                Log($"SendFeatureReportAsync exception: {ex.Message}");
                return false;
            }
        });
    }

    /// <inheritdoc />
    public async Task<bool> SendFeatureReportAsync(byte reportId, byte[] data, CancellationToken cancellationToken)
    {
        if (!IsConnected)
            return false;

        try
        {
            return await Task.Run(() =>
            {
                cancellationToken.ThrowIfCancellationRequested();
                // 使用局部变量捕获句柄，避免 TOCTOU 竞态
                SafeFileHandle? handle = _deviceHandle;
                if (handle == null || handle.IsInvalid)
                    return false;
                byte[] buffer = new byte[_featureReportSize];
                buffer[0] = reportId;
                Array.Copy(data, 0, buffer, 1, Math.Min(data.Length, buffer.Length - 1));
                bool result = HidD_SetFeature(handle, buffer, (uint)buffer.Length);
                if (!result)
                {
                    int err = Marshal.GetLastWin32Error();
                    Log($"SendFeatureReportAsync(cancel) failed: reportId={reportId}, error={err}");
                }
                return result;
            }, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            return false;
        }
        catch (Exception ex)
        {
            Log($"SendFeatureReportAsync(cancel) exception: {ex.Message}");
            return false;
        }
    }

    /// <inheritdoc />
    public async Task<byte[]?> GetFeatureReportAsync(byte reportId)
    {
        if (!IsConnected)
            return null;

        return await Task.Run(() =>
        {
            try
            {
                // 使用局部变量捕获句柄，避免 TOCTOU 竞态
                SafeFileHandle? handle = _deviceHandle;
                if (handle == null || handle.IsInvalid)
                    return null;

                // Feature 报告缓冲区，使用探测到的 FeatureReportByteLength
                byte[] buffer = new byte[_featureReportSize];
                buffer[0] = reportId;

                if (!HidD_GetFeature(handle, buffer, (uint)buffer.Length))
                {
                    int err = Marshal.GetLastWin32Error();
                    Log($"GetFeatureReportAsync failed: reportId={reportId}, error={err}");
                    return null;
                }

                // 返回数据（去掉 Report ID）
                byte[] result = new byte[buffer.Length - 1];
                Array.Copy(buffer, 1, result, 0, result.Length);
                return result;
            }
            catch (Exception ex)
            {
                Log($"GetFeatureReportAsync exception: {ex.Message}");
                return null;
            }
        });
    }

    /// <inheritdoc />
    public async Task<byte[]?> GetFeatureReportAsync(byte reportId, CancellationToken cancellationToken)
    {
        if (!IsConnected)
            return null;

        try
        {
            return await Task.Run(() =>
            {
                cancellationToken.ThrowIfCancellationRequested();
                // 使用局部变量捕获句柄，避免 TOCTOU 竞态
                SafeFileHandle? handle = _deviceHandle;
                if (handle == null || handle.IsInvalid)
                    return null;
                byte[] buffer = new byte[_featureReportSize];
                buffer[0] = reportId;
                if (!HidD_GetFeature(handle, buffer, (uint)buffer.Length))
                    return null;
                byte[] result = new byte[buffer.Length - 1];
                Array.Copy(buffer, 1, result, 0, result.Length);
                return result;
            }, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            return null;
        }
        catch (Exception ex)
        {
            Log($"GetFeatureReportAsync(cancel) exception: {ex.Message}");
            return null;
        }
    }

    #region 辅助方法

    private static string GetProductString(SafeFileHandle handle)
    {
        var sb = new StringBuilder(256);
        if (HidD_GetProductString(handle, sb, 256))
            return sb.ToString();
        return string.Empty;
    }

    private static string GetManufacturerString(SafeFileHandle handle)
    {
        var sb = new StringBuilder(256);
        if (HidD_GetManufacturerString(handle, sb, 256))
            return sb.ToString();
        return string.Empty;
    }

    private static string GetSerialNumberString(SafeFileHandle handle)
    {
        var sb = new StringBuilder(256);
        if (HidD_GetSerialNumberString(handle, sb, 256))
            return sb.ToString();
        return string.Empty;
    }

    private static ushort GetUsagePage(SafeFileHandle handle)
    {
        IntPtr preparsedData = IntPtr.Zero;
        try
        {
            if (HidD_GetPreparsedData(handle, out preparsedData))
            {
                HIDP_CAPS caps;
                uint status = HidP_GetCaps(preparsedData, out caps);
                if (status == HIDP_STATUS_SUCCESS)
                {
                    return caps.UsagePage;
                }
            }
            return 0;
        }
        catch
        {
            return 0;
        }
        finally
        {
            if (preparsedData != IntPtr.Zero)
            {
                HidD_FreePreparsedData(preparsedData);
            }
        }
    }

    private static ushort GetUsageId(SafeFileHandle handle)
    {
        IntPtr preparsedData = IntPtr.Zero;
        try
        {
            if (HidD_GetPreparsedData(handle, out preparsedData))
            {
                HIDP_CAPS caps;
                uint status = HidP_GetCaps(preparsedData, out caps);
                if (status == HIDP_STATUS_SUCCESS)
                {
                    return caps.Usage;
                }
            }
            return 0;
        }
        catch
        {
            return 0;
        }
        finally
        {
            if (preparsedData != IntPtr.Zero)
            {
                HidD_FreePreparsedData(preparsedData);
            }
        }
    }

    #endregion

    /// <inheritdoc />
    public void Dispose()
    {
        Close();
        GC.SuppressFinalize(this);
    }
}





