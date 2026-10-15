namespace HidConfigTool.Core.Utils;

/// <summary>
/// 重试策略工具类
/// 提供指数退避重试机制
/// </summary>
public static class RetryPolicy
{
    /// <summary>
    /// 默认最大重试次数
    /// </summary>
    public const int DefaultMaxRetries = 2;

    /// <summary>
    /// 默认初始延迟（毫秒）
    /// </summary>
    public const int DefaultInitialDelayMs = 100;

    /// <summary>
    /// 默认最大延迟（毫秒）
    /// </summary>
    public const int DefaultMaxDelayMs = 2000;

    /// <summary>
    /// 执行带重试的异步操作
    /// </summary>
    /// <typeparam name="T">返回值类型</typeparam>
    /// <param name="operation">要执行的操作</param>
    /// <param name="maxRetries">最大重试次数（默认 2 次，即总共执行 3 次）</param>
    /// <param name="initialDelayMs">初始延迟（毫秒，默认 100ms）</param>
    /// <param name="maxDelayMs">最大延迟（毫秒，默认 2000ms）</param>
    /// <param name="shouldRetry">判断是否应该重试的函数（默认所有异常都重试）</param>
    /// <param name="onRetry">重试时的回调（参数：当前重试次数，异常）</param>
    /// <param name="cancellationToken">取消令牌</param>
    /// <returns>操作结果</returns>
    public static async Task<T> ExecuteWithRetryAsync<T>(
        Func<Task<T>> operation,
        int maxRetries = DefaultMaxRetries,
        int initialDelayMs = DefaultInitialDelayMs,
        int maxDelayMs = DefaultMaxDelayMs,
        Func<Exception, bool>? shouldRetry = null,
        Action<int, Exception>? onRetry = null,
        CancellationToken cancellationToken = default)
    {
        Exception? lastException = null;
        int delay = initialDelayMs;

        for (int attempt = 0; attempt <= maxRetries; attempt++)
        {
            cancellationToken.ThrowIfCancellationRequested();

            try
            {
                return await operation();
            }
            catch (Exception ex)
            {
                lastException = ex;

                // 如果是最后一次尝试，不再重试
                if (attempt >= maxRetries)
                    break;

                // 判断是否应该重试
                if (shouldRetry != null && !shouldRetry(ex))
                    break;

                // 触发重试回调
                onRetry?.Invoke(attempt + 1, ex);

                // 指数退避等待
                await Task.Delay(delay, cancellationToken);

                // 计算下一次延迟（指数增长，但不超过最大值）
                delay = Math.Min(delay * 2, maxDelayMs);
            }
        }

        // 所有重试都失败，抛出最后一个异常
        throw lastException!;
    }

    /// <summary>
    /// 执行带重试的异步操作（无返回值）
    /// </summary>
    public static async Task ExecuteWithRetryAsync(
        Func<Task> operation,
        int maxRetries = DefaultMaxRetries,
        int initialDelayMs = DefaultInitialDelayMs,
        int maxDelayMs = DefaultMaxDelayMs,
        Func<Exception, bool>? shouldRetry = null,
        Action<int, Exception>? onRetry = null,
        CancellationToken cancellationToken = default)
    {
        await ExecuteWithRetryAsync<object?>(async () =>
        {
            await operation();
            return null;
        }, maxRetries, initialDelayMs, maxDelayMs, shouldRetry, onRetry, cancellationToken);
    }

    /// <summary>
    /// 执行带重试和超时的异步操作
    /// </summary>
    /// <typeparam name="T">返回值类型</typeparam>
    /// <param name="operation">要执行的操作（接受 CancellationToken 参数）</param>
    /// <param name="timeoutMs">单次操作超时时间（毫秒，默认 1000ms）</param>
    /// <param name="maxRetries">最大重试次数</param>
    /// <param name="initialDelayMs">初始延迟</param>
    /// <param name="maxDelayMs">最大延迟</param>
    /// <param name="shouldRetry">判断是否应该重试的函数</param>
    /// <param name="onRetry">重试时的回调</param>
    /// <param name="cancellationToken">外部取消令牌</param>
    /// <returns>操作结果</returns>
    public static async Task<T> ExecuteWithRetryAndTimeoutAsync<T>(
        Func<CancellationToken, Task<T>> operation,
        int timeoutMs = 1000,
        int maxRetries = DefaultMaxRetries,
        int initialDelayMs = DefaultInitialDelayMs,
        int maxDelayMs = DefaultMaxDelayMs,
        Func<Exception, bool>? shouldRetry = null,
        Action<int, Exception>? onRetry = null,
        CancellationToken cancellationToken = default)
    {
        return await ExecuteWithRetryAsync(async () =>
        {
            using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            timeoutCts.CancelAfter(timeoutMs);

            try
            {
                return await operation(timeoutCts.Token);
            }
            catch (OperationCanceledException) when (timeoutCts.IsCancellationRequested && !cancellationToken.IsCancellationRequested)
            {
                // 超时，包装为 TimeoutException
                throw new TimeoutException($"操作超时（{timeoutMs}ms）");
            }
        }, maxRetries, initialDelayMs, maxDelayMs, shouldRetry, onRetry, cancellationToken);
    }

    /// <summary>
    /// 执行带重试和超时的异步操作（无返回值）
    /// </summary>
    public static async Task ExecuteWithRetryAndTimeoutAsync(
        Func<CancellationToken, Task> operation,
        int timeoutMs = 1000,
        int maxRetries = DefaultMaxRetries,
        int initialDelayMs = DefaultInitialDelayMs,
        int maxDelayMs = DefaultMaxDelayMs,
        Func<Exception, bool>? shouldRetry = null,
        Action<int, Exception>? onRetry = null,
        CancellationToken cancellationToken = default)
    {
        await ExecuteWithRetryAndTimeoutAsync<object?>(async (ct) =>
        {
            await operation(ct);
            return null;
        }, timeoutMs, maxRetries, initialDelayMs, maxDelayMs, shouldRetry, onRetry, cancellationToken);
    }
}
