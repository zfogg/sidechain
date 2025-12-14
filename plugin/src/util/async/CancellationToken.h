#pragma once

#include <memory>
#include <atomic>
#include <functional>
#include <vector>
#include <mutex>

namespace Sidechain {
namespace Util {

/**
 * CancellationToken - Thread-safe token for signaling operation cancellation
 *
 * Used to coordinate cancellation across threads in async operations.
 * Supports:
 * - Checking if cancellation was requested
 * - Registering callbacks to be invoked on cancellation
 * - Automatic cleanup with shared_ptr
 *
 * Example:
 *   auto token = std::make_shared<CancellationToken>();
 *
 *   Async::run([token]() {
 *       for (int i = 0; i < 1000; ++i) {
 *           if (token->isCancellationRequested())
 *               return;  // Exit early
 *           doWork(i);
 *       }
 *   });
 *
 *   // Later, cancel the operation
 *   token->cancel();
 */
class CancellationToken
{
public:
    using CancellationCallback = std::function<void()>;

    CancellationToken() = default;
    ~CancellationToken() = default;

    /**
     * Check if cancellation has been requested
     * @return true if cancel() has been called
     */
    bool isCancellationRequested() const
    {
        return cancelled.load(std::memory_order_acquire);
    }

    /**
     * Request cancellation of the operation
     * Triggers all registered cancellation callbacks
     */
    void cancel()
    {
        // Set flag atomically
        if (cancelled.exchange(true, std::memory_order_release))
            return;  // Already cancelled, avoid duplicate notification

        // Invoke all cancellation callbacks
        std::vector<CancellationCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callbacks = cancellationCallbacks;
        }

        for (auto& callback : callbacks)
        {
            if (callback)
                callback();
        }
    }

    /**
     * Register a callback to be invoked when cancel() is called
     * @param callback Function to invoke on cancellation
     * @return Token/ID for unregistering (currently unused, reserved for future)
     */
    int onCancellation(CancellationCallback callback)
    {
        if (!callback)
            return -1;

        std::lock_guard<std::mutex> lock(callbackMutex);
        cancellationCallbacks.push_back(callback);
        return static_cast<int>(cancellationCallbacks.size() - 1);
    }

    /**
     * Clear all cancellation callbacks (useful for cleanup)
     */
    void clearCallbacks()
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        cancellationCallbacks.clear();
    }

    /**
     * Get number of registered callbacks
     */
    size_t getCallbackCount() const
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        return cancellationCallbacks.size();
    }

    // Prevent copying
    CancellationToken(const CancellationToken&) = delete;
    CancellationToken& operator=(const CancellationToken&) = delete;

    // Allow moving
    CancellationToken(CancellationToken&&) = default;
    CancellationToken& operator=(CancellationToken&&) = default;

private:
    std::atomic<bool> cancelled{false};
    mutable std::mutex callbackMutex;
    std::vector<CancellationCallback> cancellationCallbacks;
};

/**
 * CancellationTokenSource - Factory for creating linked cancellation tokens
 *
 * Useful for:
 * - Cascading cancellation (cancel parent = cancel all children)
 * - Timeout-based cancellation (automatically cancel after delay)
 * - Composing multiple cancellation conditions
 *
 * Example:
 *   auto source = std::make_shared<CancellationTokenSource>();
 *   auto token = source->token();
 *
 *   // Cancel all operations using this token
 *   source->cancel();
 */
class CancellationTokenSource
{
public:
    CancellationTokenSource()
        : token_(std::make_shared<CancellationToken>())
    {
    }

    /**
     * Get the associated cancellation token
     */
    std::shared_ptr<CancellationToken> token() const
    {
        return token_;
    }

    /**
     * Request cancellation
     */
    void cancel()
    {
        token_->cancel();
    }

    /**
     * Check if cancellation was requested
     */
    bool isCancellationRequested() const
    {
        return token_->isCancellationRequested();
    }

    // Prevent copying
    CancellationTokenSource(const CancellationTokenSource&) = delete;
    CancellationTokenSource& operator=(const CancellationTokenSource&) = delete;

private:
    std::shared_ptr<CancellationToken> token_;
};

}  // namespace Util
}  // namespace Sidechain
