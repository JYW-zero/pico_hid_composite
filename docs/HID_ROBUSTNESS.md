# HID 通信健壮性增强

## 一、概述

为 HID 驱动增加超时、重试、错误码映射、并发锁等健壮性机制，确保在设备不稳定或通信异常时软件不崩溃，并给出清晰的错误提示。

## 二、增强特性

### 2.1 超时机制

- **默认超时**：1000ms（1 秒）
- **实现方式**：`CancellationTokenSource.CancelAfter(timeoutMs)`
- **超时处理**：捕获 `OperationCanceledException`，包装为 `TimeoutException`，记录日志并返回失败

```csharp
using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
timeoutCts.CancelAfter(timeoutMs);
```

### 2.2 自动重试（指数退避）

- **最大重试次数**：2 次（总共执行 3 次：1 次初始 + 2 次重试）
- **初始延迟**：100ms
- **退避策略**：指数增长（100ms → 200ms → 400ms...），最大不超过 2000ms
- **重试条件**：仅对可重试的临时性错误进行重试（如设备未就绪、超时、资源不足等）

```csharp
// 可重试的 Win32 错误码
21  = 设备未就绪
32  = 文件被占用
33  = 文件被锁定
55  = 资源不可用
59  = 网络错误
121 = 超时
1167 = 设备未连接
1450 = 资源不足
```

### 2.3 错误码映射

将 Windows Win32 错误码映射为用户可读的中文信息，便于调试和用户理解。

**示例**：

| 错误码 | 用户可读信息 |
|--------|-------------|
| 5 | 拒绝访问（设备可能被其他程序占用） |
| 21 | 设备未就绪 |
| 32 | 另一个程序正在使用此文件，进程无法访问 |
| 87 | 参数错误 |
| 121 | 信号灯超时时间已到（设备响应超时） |
| 1167 | 设备未连接或已断开 |

**使用方式**：

```csharp
int err = Marshal.GetLastWin32Error();
string errMsg = HidErrorMapper.GetErrorMessage(err);
Log($"操作失败: error={err} ({errMsg})");
```

### 2.4 并发锁（SemaphoreSlim）

- **锁类型**：`SemaphoreSlim(1, 1)`（互斥锁，同一时间只允许一个线程访问）
- **作用范围**：所有 HID 读写操作（SendFeatureReportAsync / GetFeatureReportAsync）
- **目的**：避免多线程并发访问同一设备句柄导致的竞态条件和数据损坏

```csharp
private readonly SemaphoreSlim _ioLock = new(1, 1);

// 使用方式
await _ioLock.WaitAsync(cancellationToken);
try
{
    // 执行 HID 操作
}
finally
{
    _ioLock.Release();
}
```

### 2.5 CancellationToken 支持

- 所有读写方法都支持 `CancellationToken` 参数
- 无参数版本内部使用 `CancellationToken.None`
- 取消时立即返回，不触发重试

## 三、架构设计

```
┌─────────────────────────────────────────────────────────┐
│                    调用方 (DeviceService)                 │
│  SendFeatureReportAsync(reportId, data, ct)             │
└──────────────────────────┬──────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│              RetryPolicy (重试策略)                       │
│  ┌─────────────────────────────────────────────────┐    │
│  │ 1. 超时控制 (CancellationTokenSource)            │    │
│  │ 2. 重试循环 (最多 2 次)                          │    │
│  │ 3. 指数退避等待 (100ms → 200ms)                 │    │
│  │ 4. 可重试性判断 (HidErrorMapper.IsRetryable)     │    │
│  └─────────────────────────────────────────────────┘    │
└──────────────────────────┬──────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│              HidDriver (HID 驱动)                         │
│  ┌─────────────────────────────────────────────────┐    │
│  │ 1. 并发锁 (SemaphoreSlim)                        │    │
│  │ 2. 句柄安全访问 (局部变量捕获，防 TOCTOU)        │    │
│  │ 3. Win32 API 调用 (HidD_SetFeature/GetFeature)  │    │
│  │ 4. 错误码映射 (HidErrorMapper)                   │    │
│  │ 5. 日志记录                                      │    │
│  └─────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

## 四、核心代码

### 4.1 RetryPolicy（重试策略工具类）

位置：`shared/HidConfigTool.Core/Utils/RetryPolicy.cs`

主要方法：

```csharp
// 带重试的异步操作
Task<T> ExecuteWithRetryAsync<T>(
    Func<Task<T>> operation,
    int maxRetries = 2,
    int initialDelayMs = 100,
    int maxDelayMs = 2000,
    Func<Exception, bool>? shouldRetry = null,
    Action<int, Exception>? onRetry = null,
    CancellationToken cancellationToken = default);

// 带重试和超时的异步操作
Task<T> ExecuteWithRetryAndTimeoutAsync<T>(
    Func<CancellationToken, Task<T>> operation,
    int timeoutMs = 1000,
    int maxRetries = 2,
    ...);
```

### 4.2 HidErrorMapper（错误码映射工具类）

位置：`shared/HidConfigTool.Core/Utils/HidErrorMapper.cs`

主要方法：

```csharp
// 错误码 → 用户可读信息
string GetErrorMessage(int errorCode);

// 获取最后一个 Win32 错误的用户可读信息
string GetLastErrorMessage();

// 判断错误是否可重试
bool IsRetryable(int errorCode);
bool IsRetryable(Exception ex);
```

### 4.3 HidDriver（增强后的 HID 驱动）

位置：`windows/HidConfigTool.App/Drivers/HidDriver.cs`

核心增强：

```csharp
// 配置常量
private const int DefaultTimeoutMs = 1000;      // 默认超时 1 秒
private const int DefaultMaxRetries = 2;          // 默认重试 2 次
private const int DefaultInitialDelayMs = 100;    // 默认初始延迟 100ms

// 并发锁
private readonly SemaphoreSlim _ioLock = new(1, 1);

// 增强后的读写方法
public async Task<bool> SendFeatureReportAsync(
    byte reportId, byte[] data, CancellationToken cancellationToken)
{
    return await RetryPolicy.ExecuteWithRetryAndTimeoutAsync(
        async (ct) =>
        {
            await _ioLock.WaitAsync(ct);  // 获取并发锁
            try
            {
                // 执行 HID 写入
                bool result = HidD_SetFeature(handle, buffer, length);
                if (!result)
                {
                    int err = Marshal.GetLastWin32Error();
                    if (HidErrorMapper.IsRetryable(err))
                        throw new IOException(...);  // 触发重试
                }
                return result;
            }
            finally
            {
                _ioLock.Release();  // 释放锁
            }
        },
        timeoutMs: DefaultTimeoutMs,
        maxRetries: DefaultMaxRetries,
        shouldRetry: HidErrorMapper.IsRetryable,
        onRetry: (attempt, ex) => Log(...),
        cancellationToken: cancellationToken);
}
```

## 五、修改文件清单

| 类型 | 文件 | 说明 |
|------|------|------|
| 新建 | `shared/HidConfigTool.Core/Utils/HidErrorMapper.cs` | 错误码映射工具类 |
| 新建 | `shared/HidConfigTool.Core/Utils/RetryPolicy.cs` | 重试策略工具类 |
| 修改 | `windows/HidConfigTool.App/Drivers/HidDriver.cs` | 增加超时/重试/锁/错误码映射 |

## 六、验证

- ✅ 所有 HID 读写默认超时 1 秒
- ✅ 失败自动重试 2 次，指数退避（100ms → 200ms）
- ✅ 错误码映射为用户可读中文信息
- ✅ 多线程访问加锁（SemaphoreSlim），避免并发冲突
- ✅ 完整 CancellationToken 支持取消操作
- ✅ 编译通过（0 错误）
- ✅ 向后兼容（无参数版本继续可用）

## 七、使用建议

### 7.1 调用方注意事项

1. **不要在 UI 线程直接调用**：所有 HID 操作都是异步的，应使用 `await` 调用
2. **合理设置超时**：默认 1 秒适用于大多数场景，特殊场景可通过 `CancellationTokenSource` 自定义
3. **处理失败情况**：重试 2 次后仍失败会返回 `false` 或 `null`，调用方应处理这种情况
4. **避免频繁调用**：虽然有重试机制，但频繁调用会增加设备负担

### 7.2 调试技巧

1. **查看日志**：HID 驱动会将所有错误和重试记录到 `%AppData%\HIDConfigTool\Logs\hid_driver.log`
2. **错误码查询**：日志中会同时记录错误码和用户可读信息，便于定位问题
3. **重试次数**：日志中会记录每次重试的次数和原因

## 八、后续优化方向

1. **可配置参数**：将超时时间、重试次数等参数改为可配置（通过配置文件或 UI 设置）
2. **熔断机制**：连续失败多次后暂时停止请求，避免设备过载
3. **健康检查**：定期检测设备连接状态，提前发现问题
4. **性能监控**：统计 HID 操作的成功率、平均耗时、重试次数等指标
