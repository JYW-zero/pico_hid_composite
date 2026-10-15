using HidConfigTool.Core.Interfaces;
using HidSharp;
using HidSharp.Reports;
using System.Threading;

namespace HidConfigTool.Hid;

/// <summary>
/// 跨平台 HID 驱动（Windows / macOS / Linux）
/// </summary>
public sealed class HidSharpDriver : IHidDriver
{
    private HidStream? _stream;
    private HidDevice? _device;
    private int _featureLength = 65;
    private int _inputLength = 65;
    private int _outputLength = 65;

    public ushort VendorId { get; private set; }
    public ushort ProductId { get; private set; }
    public bool IsConnected => _stream != null;

    public Task<IReadOnlyList<HidDeviceInfo>> FindDevicesAsync(ushort vendorId, ushort productId)
    {
        var result = new List<HidDeviceInfo>();

        // 如果 productId 为 0，搜索所有 HID 设备并按 VID 过滤
        IEnumerable<HidDevice> devices;
        if (productId == 0)
        {
            devices = DeviceList.Local.GetHidDevices().Where(d => d.VendorID == vendorId);
        }
        else
        {
            devices = DeviceList.Local.GetHidDevices(vendorID: vendorId, productID: productId);
        }

        foreach (var device in devices)
        {
            try
            {
                ushort usagePage = 0;
                ushort usageId = 0;
                TryReadUsage(device, out usagePage, out usageId);

                result.Add(new HidDeviceInfo
                {
                    DevicePath = device.DevicePath,
                    VendorId = (ushort)device.VendorID,
                    ProductId = (ushort)device.ProductID,
                    ProductName = SafeGet(() => device.GetProductName()) ?? string.Empty,
                    ManufacturerName = SafeGet(() => device.GetManufacturer()) ?? string.Empty,
                    SerialNumber = SafeGet(() => device.GetSerialNumber()) ?? string.Empty,
                    UsagePage = usagePage,
                    UsageId = usageId
                });
            }
            catch
            {
                // 跳过打不开的接口
            }
        }

        return Task.FromResult<IReadOnlyList<HidDeviceInfo>>(result);
    }

    public Task<bool> OpenAsync(string devicePath)
    {
        Close();

        return Task.Run(() =>
        {
            try
            {
                var device = DeviceList.Local.GetHidDevices()
                    .FirstOrDefault(d => d.DevicePath == devicePath);
                if (device == null)
                    return false;

                if (!device.TryOpen(out var stream))
                    return false;

                _device = device;
                _stream = stream;
                VendorId = (ushort)device.VendorID;
                ProductId = (ushort)device.ProductID;
                try
                {
                    _featureLength = Math.Max(device.GetMaxFeatureReportLength(), 65);
                }
                catch
                {
                    _featureLength = 65;
                }

                try
                {
                    _inputLength = Math.Max(device.GetMaxInputReportLength(), 65);
                }
                catch
                {
                    _inputLength = 65;
                }

                try
                {
                    _outputLength = Math.Max(device.GetMaxOutputReportLength(), 65);
                }
                catch
                {
                    _outputLength = 65;
                }

                return true;
            }
            catch
            {
                Close();
                return false;
            }
        });
    }

    public void Close()
    {
        try
        {
            _stream?.Dispose();
        }
        catch
        {
            // ignore
        }

        _stream = null;
        _device = null;
    }

    public Task<bool> SendFeatureReportAsync(byte reportId, byte[] data)
    {
        if (_stream == null)
            return Task.FromResult(false);

        return Task.Run(() =>
        {
            try
            {
                byte[] buffer = new byte[_featureLength];
                buffer[0] = reportId;
                Array.Copy(data, 0, buffer, 1, Math.Min(data.Length, buffer.Length - 1));
                _stream.SetFeature(buffer);
                return true;
            }
            catch
            {
                return false;
            }
        });
    }

    public Task<byte[]?> GetFeatureReportAsync(byte reportId)
    {
        if (_stream == null)
            return Task.FromResult<byte[]?>(null);

        return Task.Run(() =>
        {
            try
            {
                byte[] buffer = new byte[_featureLength];
                buffer[0] = reportId;
                _stream.GetFeature(buffer);
                byte[] result = new byte[buffer.Length - 1];
                Array.Copy(buffer, 1, result, 0, result.Length);
                return (byte[]?)result;
            }
            catch
            {
                return null;
            }
        });
    }

    public Task<bool> SendFeatureReportAsync(byte reportId, byte[] data, CancellationToken cancellationToken)
    {
        if (_stream == null)
            return Task.FromResult(false);

        return Task.Run(() =>
        {
            cancellationToken.ThrowIfCancellationRequested();
            try
            {
                byte[] buffer = new byte[_featureLength];
                buffer[0] = reportId;
                Array.Copy(data, 0, buffer, 1, Math.Min(data.Length, buffer.Length - 1));
                _stream.SetFeature(buffer);
                return true;
            }
            catch (OperationCanceledException)
            {
                throw;
            }
            catch
            {
                return false;
            }
        }, cancellationToken);
    }

    public Task<byte[]?> GetFeatureReportAsync(byte reportId, CancellationToken cancellationToken)
    {
        if (_stream == null)
            return Task.FromResult<byte[]?>(null);

        return Task.Run(() =>
        {
            cancellationToken.ThrowIfCancellationRequested();
            try
            {
                byte[] buffer = new byte[_featureLength];
                buffer[0] = reportId;
                _stream.GetFeature(buffer);
                byte[] result = new byte[buffer.Length - 1];
                Array.Copy(buffer, 1, result, 0, result.Length);
                return (byte[]?)result;
            }
            catch (OperationCanceledException)
            {
                throw;
            }
            catch
            {
                return null;
            }
        }, cancellationToken);
    }

    public Task<byte[]?> ReadInputReportAsync(CancellationToken cancellationToken = default)
    {
        if (_stream == null)
            return Task.FromResult<byte[]?>(null);

        return Task.Run(() =>
        {
            try
            {
                byte[] buffer = new byte[_inputLength];
                int oldTimeout = _stream.ReadTimeout;

                try
                {
                    // 设置较短超时，便于检查取消令牌
                    _stream.ReadTimeout = 100;

                    while (!cancellationToken.IsCancellationRequested)
                    {
                        try
                        {
                            int bytesRead = _stream.Read(buffer, 0, buffer.Length);
                            if (bytesRead > 0)
                            {
                                // 返回完整缓冲区（包含 Report ID）
                                byte[] result = new byte[bytesRead];
                                Array.Copy(buffer, 0, result, 0, bytesRead);
                                return (byte[]?)result;
                            }
                        }
                        catch (TimeoutException)
                        {
                            // 超时，继续循环检查取消令牌
                        }
                    }

                    // 被取消
                    return null;
                }
                finally
                {
                    _stream.ReadTimeout = oldTimeout;
                }
            }
            catch (OperationCanceledException)
            {
                throw;
            }
            catch
            {
                return null;
            }
        }, cancellationToken);
    }

    public Task<bool> SendOutputReportAsync(byte reportId, byte[] data)
    {
        return SendOutputReportAsync(reportId, data, CancellationToken.None);
    }

    public Task<bool> SendOutputReportAsync(byte reportId, byte[] data, CancellationToken cancellationToken)
    {
        if (_stream == null)
            return Task.FromResult(false);

        return Task.Run(() =>
        {
            cancellationToken.ThrowIfCancellationRequested();
            try
            {
                // Output Report 缓冲区（第一个字节为 Report ID）
                byte[] buffer = new byte[_outputLength];
                buffer[0] = reportId;
                Array.Copy(data, 0, buffer, 1, Math.Min(data.Length, buffer.Length - 1));
                _stream.Write(buffer, 0, buffer.Length);
                return true;
            }
            catch
            {
                return false;
            }
        }, cancellationToken);
    }

    public void Dispose()
    {
        Close();
        GC.SuppressFinalize(this);
    }

    private static void TryReadUsage(HidDevice device, out ushort usagePage, out ushort usageId)
    {
        usagePage = 0;
        usageId = 0;
        try
        {
            ReportDescriptor descriptor = device.GetReportDescriptor();
            var item = descriptor.DeviceItems.FirstOrDefault();
            if (item == null)
                return;

            foreach (uint usage in item.Usages.GetAllValues())
            {
                usagePage = (ushort)((usage >> 16) & 0xFFFF);
                usageId = (ushort)(usage & 0xFFFF);
                break;
            }
        }
        catch
        {
            // macOS 上部分接口可能读不到描述符
        }
    }

    private static string? SafeGet(Func<string> getter)
    {
        try
        {
            return getter();
        }
        catch
        {
            return null;
        }
    }
}
