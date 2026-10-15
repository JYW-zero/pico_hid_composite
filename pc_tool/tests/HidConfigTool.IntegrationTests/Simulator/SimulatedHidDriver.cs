/*
 * SimulatedHidDriver.cs
 * 模拟 HID 驱动
 * 实现 IHidDriver 接口，将调用转发到模拟设备
 */

using HidConfigTool.Core.Interfaces;

namespace HidConfigTool.IntegrationTests.Simulator;

/// <summary>
/// 模拟 HID 驱动
/// </summary>
public class SimulatedHidDriver : IHidDriver
{
    private readonly SimulatedHidDevice _device;
    private bool _disposed;
    private string _currentDevicePath = string.Empty;

    /// <summary>
    /// 构造函数
    /// </summary>
    public SimulatedHidDriver(SimulatedHidDevice device)
    {
        _device = device;
    }

    /// <summary>
    /// 设备 VID
    /// </summary>
    public ushort VendorId => SimulatedHidDevice.DEFAULT_VENDOR_ID;

    /// <summary>
    /// 设备 PID
    /// </summary>
    public ushort ProductId => SimulatedHidDevice.DEFAULT_PRODUCT_ID;

    /// <summary>
    /// 是否已连接
    /// </summary>
    public bool IsConnected => !_disposed && _device.IsConnected && !string.IsNullOrEmpty(_currentDevicePath);

    /// <summary>
    /// 查找所有匹配的 HID 设备
    /// </summary>
    public Task<IReadOnlyList<HidDeviceInfo>> FindDevicesAsync(ushort vendorId, ushort productId)
    {
        var devices = new List<HidDeviceInfo>();

        // 检查设备行为 - 如果断开或设备未连接，返回空
        if (_device.Behavior.Mode == DeviceBehaviorMode.Disconnected || !_device.IsConnected)
        {
            return Task.FromResult<IReadOnlyList<HidDeviceInfo>>(devices);
        }

        // 如果 VID/PID 匹配，返回模拟设备
        if (vendorId == VendorId && productId == ProductId)
        {
            devices.Add(new HidDeviceInfo
            {
                DevicePath = @"\\?\hid#simulated_device#1",
                VendorId = VendorId,
                ProductId = ProductId,
                ProductName = _device.ProductName,
                ManufacturerName = _device.ManufacturerName,
                SerialNumber = _device.SerialNumber,
                UsagePage = SimulatedHidDevice.DEFAULT_USAGE_PAGE,
                UsageId = SimulatedHidDevice.DEFAULT_USAGE_ID
            });
        }

        return Task.FromResult<IReadOnlyList<HidDeviceInfo>>(devices);
    }

    /// <summary>
    /// 打开设备
    /// </summary>
    public async Task<bool> OpenAsync(string devicePath)
    {
        if (_disposed) return false;
        if (string.IsNullOrEmpty(devicePath)) return false;

        // 检查设备行为
        if (_device.Behavior.ShouldFail())
        {
            return false;
        }

        // 模拟延迟
        int delay = _device.Behavior.GetDelayMs();
        if (delay > 0)
        {
            await Task.Delay(delay);
        }

        _currentDevicePath = devicePath;
        return true;
    }

    /// <summary>
    /// 关闭设备
    /// </summary>
    public void Close()
    {
        _currentDevicePath = string.Empty;
    }

    /// <summary>
    /// 发送 Feature 报告
    /// </summary>
    public async Task<bool> SendFeatureReportAsync(byte reportId, byte[] data)
    {
        if (_disposed || !IsConnected) return false;
        if (data == null) return false;

        // 模拟延迟
        int delay = _device.Behavior.GetDelayMs();
        if (delay > 0)
        {
            await Task.Delay(delay);
        }

        return _device.HandleSendFeatureReport(reportId, data);
    }

    /// <summary>
    /// 发送 Feature 报告（支持取消）
    /// </summary>
    public async Task<bool> SendFeatureReportAsync(byte reportId, byte[] data, CancellationToken cancellationToken)
    {
        if (_disposed || !IsConnected) return false;
        if (data == null) return false;

        // 检查取消
        if (cancellationToken.IsCancellationRequested)
        {
            return false;
        }

        // 模拟延迟（支持取消）
        int delay = _device.Behavior.GetDelayMs();
        if (delay > 0)
        {
            try
            {
                await Task.Delay(delay, cancellationToken);
            }
            catch (TaskCanceledException)
            {
                return false;
            }
        }

        // 再次检查取消
        if (cancellationToken.IsCancellationRequested)
        {
            return false;
        }

        return _device.HandleSendFeatureReport(reportId, data);
    }

    /// <summary>
    /// 读取 Feature 报告
    /// </summary>
    public async Task<byte[]?> GetFeatureReportAsync(byte reportId)
    {
        if (_disposed || !IsConnected) return null;

        // 模拟延迟
        int delay = _device.Behavior.GetDelayMs();
        if (delay > 0)
        {
            await Task.Delay(delay);
        }

        return _device.HandleGetFeatureReport(reportId);
    }

    /// <summary>
    /// 读取 Feature 报告（支持取消）
    /// </summary>
    public async Task<byte[]?> GetFeatureReportAsync(byte reportId, CancellationToken cancellationToken)
    {
        if (_disposed || !IsConnected) return null;

        // 检查取消
        if (cancellationToken.IsCancellationRequested)
        {
            return null;
        }

        // 模拟延迟（支持取消）
        int delay = _device.Behavior.GetDelayMs();
        if (delay > 0)
        {
            try
            {
                await Task.Delay(delay, cancellationToken);
            }
            catch (TaskCanceledException)
            {
                return null;
            }
        }

        // 再次检查取消
        if (cancellationToken.IsCancellationRequested)
        {
            return null;
        }

        return _device.HandleGetFeatureReport(reportId);
    }

    /// <summary>
    /// 读取 Input 报告（模拟实现）
    /// 模拟设备在处理控制命令后会入队一条 ACK Input 报告，这里取出交给 DeviceService；
    /// 队列为空时返回 null（DeviceService 会短暂延迟后继续轮询）。
    /// </summary>
    public Task<byte[]?> ReadInputReportAsync(CancellationToken cancellationToken = default)
    {
        if (_disposed || !IsConnected)
            return Task.FromResult<byte[]?>(null);

        // 检查取消
        if (cancellationToken.IsCancellationRequested)
            return Task.FromResult<byte[]?>(null);

        return Task.FromResult<byte[]?>(_device.TryDequeueInputReport());
    }

    /// <summary>
    /// 发送 Output 报告（模拟实现）
    /// 模拟设备中 Output Report 与 Feature Report 行为一致，直接委托给 Feature Report 处理
    /// </summary>
    public Task<bool> SendOutputReportAsync(byte reportId, byte[] data)
    {
        return SendOutputReportAsync(reportId, data, CancellationToken.None);
    }

    /// <summary>
    /// 发送 Output 报告（模拟实现，支持取消）
    /// 控制命令走 v3.1 Output 帧，由 SimulatedHidDevice.HandleSendOutputReport 处理，
    /// 其应答通过 GetFeatureReport(0x18) 轮询读取。
    /// </summary>
    public async Task<bool> SendOutputReportAsync(byte reportId, byte[] data, CancellationToken cancellationToken)
    {
        if (_disposed || !IsConnected)
            return false;

        if (cancellationToken.IsCancellationRequested)
            return false;

        // 模拟延迟（与 Feature 发送一致）
        int delay = _device.Behavior.GetDelayMs();
        if (delay > 0)
        {
            try
            {
                await Task.Delay(delay, cancellationToken);
            }
            catch (TaskCanceledException)
            {
                return false;
            }
        }

        if (cancellationToken.IsCancellationRequested)
            return false;

        return _device.HandleSendOutputReport(reportId, data);
    }

    /// <summary>
    /// 释放资源
    /// </summary>
    public void Dispose()
    {
        if (!_disposed)
        {
            Close();
            _disposed = true;
        }
    }
}
