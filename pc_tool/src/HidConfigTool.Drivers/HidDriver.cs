using Windows.Devices.Enumeration;
using Windows.Devices.HumanInterfaceDevice;
using Windows.Storage.Streams;
using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.Drivers;

/// <summary>
/// HID 驱动实现（使用 Windows 官方 WinRT API）
/// </summary>
public class HidDriver : IHidDriver
{
    private HidDevice? _device;

    /// <inheritdoc />
    public ushort VendorId { get; private set; }

    /// <inheritdoc />
    public ushort ProductId { get; private set; }

    /// <inheritdoc />
    public bool IsConnected => _device != null;

    /// <inheritdoc />
    public async Task<IReadOnlyList<HidDeviceInfo>> FindDevicesAsync(ushort vendorId, ushort productId)
    {
        var result = new List<HidDeviceInfo>();

        try
        {
            // 查找所有 HID 设备，然后过滤 VID/PID
            string selector = HidDevice.GetDeviceSelector(0, 0); // 所有 Usage Page 和 Usage
            DeviceInformationCollection devices = await DeviceInformation.FindAllAsync(selector);

            foreach (DeviceInformation deviceInfo in devices)
            {
                try
                {
                    // 尝试从设备 ID 中解析 VID/PID
                    string id = deviceInfo.Id.ToUpperInvariant();
                    if (!id.Contains($"VID_{vendorId:X4}") || !id.Contains($"PID_{productId:X4}"))
                        continue;

                    var info = new HidDeviceInfo
                    {
                        DevicePath = deviceInfo.Id,
                        VendorId = vendorId,
                        ProductId = productId,
                        ProductName = deviceInfo.Name,
                        ManufacturerName = string.Empty,
                        SerialNumber = string.Empty,
                        UsagePage = 0,
                        UsageId = 0
                    };

                    result.Add(info);
                }
                catch
                {
                    // 忽略单个设备的错误
                }
            }
        }
        catch
        {
            // 查找失败，返回空列表
        }

        return result;
    }

    /// <inheritdoc />
    public async Task<bool> OpenAsync(string devicePath)
    {
        try
        {
            Close();

            _device = await HidDevice.FromIdAsync(devicePath, Windows.Storage.FileAccessMode.ReadWrite);
            if (_device != null)
            {
                VendorId = _device.VendorId;
                ProductId = _device.ProductId;
                return true;
            }
        }
        catch
        {
            // 打开失败
        }

        return false;
    }

    /// <inheritdoc />
    public void Close()
    {
        _device?.Dispose();
        _device = null;
    }

    /// <inheritdoc />
    public async Task<bool> SendFeatureReportAsync(byte reportId, byte[] data)
    {
        if (_device == null)
            return false;

        try
        {
            HidFeatureReport report = _device.CreateFeatureReport(reportId);

            // 写入数据
            DataWriter writer = new DataWriter();
            writer.WriteBytes(data);
            report.Data = writer.DetachBuffer();

            uint bytesWritten = await _device.SendFeatureReportAsync(report);
            return bytesWritten > 0;
        }
        catch
        {
            return false;
        }
    }

    /// <inheritdoc />
    public async Task<byte[]?> GetFeatureReportAsync(byte reportId)
    {
        if (_device == null)
            return null;

        try
        {
            HidFeatureReport report = await _device.GetFeatureReportAsync(reportId);

            DataReader reader = DataReader.FromBuffer(report.Data);
            byte[] data = new byte[report.Data.Length];
            reader.ReadBytes(data);

            return data;
        }
        catch
        {
            return null;
        }
    }

    /// <inheritdoc />
    public void Dispose()
    {
        Close();
        GC.SuppressFinalize(this);
    }
}
