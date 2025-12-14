#pragma once

#include <JuceHeader.h>
#include <functional>
#include <thread>
#include <map>
#include <mutex>
#include <memory>

//==============================================================================
/**
 * Async - Threading utilities for background work and UI callbacks
 *
 * Provides clean abstractions over common threading patterns:
 * - run(): Execute work on background thread, callback on message thread
 * - runVoid(): Fire-and-forget background work with UI callback
 * - delay(): Execute a callback after a delay on message thread
 * - debounce(): Coalesce rapid calls (e.g., search input)
 *
 * Usage:
 *   // Background work with result
 *   Async::run<juce::Image>(
 *       []() { return downloadImage(); },
 *       [this](const juce::Image& img) { avatarImage = img; repaint(); }
 *   );
 *
 *   // Fire-and-forget with completion callback
 *   Async::runVoid(
 *       []() { doExpensiveWork(); },
 *       [this]() { isLoading = false; repaint(); }
 *   );
 *
 *   // Delayed execution
 *   Async::delay(500, [this]() { showTooltip(); });
 *
 *   // Debounced search (only fires after 300ms of inactivity)
 *   Async::debounce("search", 300, [this, query]() { performSearch(query); });
 */
namespace Async
{
    //==========================================================================
    // Background Work with Result

    // Internal - check if shutdown is in progress (implemented in Async.cpp)
    bool isShutdownInProgress();

    /**
     * Run work on a background thread and deliver result to message thread.
     *
     * @tparam T        The return type of the work function
     * @param work      Function to execute on background thread
     * @param onComplete Callback receiving result, called on message thread
     */
    template<typename T>
    void run(std::function<T()> work, std::function<void(T)> onComplete)
    {
        // Don't start new work if we're shutting down
        if (isShutdownInProgress())
            return;

        std::thread([workFn = std::move(work), completeFn = std::move(onComplete)]() {
            T result = workFn();

            // Only call back to message thread if not shutting down
            if (!isShutdownInProgress())
            {
                if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
                {
                    mm->callAsync([res = std::move(result), cb = completeFn]() {
                        if (cb && !isShutdownInProgress())
                            cb(res);
                    });
                }
            }
        }).detach();
    }

    /**
     * Run void work on background thread with completion callback.
     *
     * @param work       Function to execute on background thread
     * @param onComplete Called on message thread when work completes (may be nullptr)
     */
    void runVoid(std::function<void()> work, std::function<void()> onComplete = nullptr);

    //==========================================================================
    // Delayed Execution

    /**
     * Execute a callback after a delay on the message thread.
     * Returns an ID that can be used to cancel the delay.
     *
     * @param delayMs    Delay in milliseconds
     * @param callback   Function to call after delay
     * @return           Timer ID for cancellation (0 if failed)
     */
    int delay(int delayMs, std::function<void()> callback);

    /**
     * Cancel a pending delayed callback.
     *
     * @param timerId    Timer ID returned from delay()
     */
    void cancelDelay(int timerId);

    //==========================================================================
    // Debouncing

    /**
     * Debounce calls to a function - only executes after a period of inactivity.
     * Useful for search inputs, resize handlers, etc.
     *
     * Each call resets the timer. The callback only fires when no new calls
     * have been made for `delayMs` milliseconds.
     *
     * @param key       Unique identifier for this debounce group
     * @param delayMs   Delay in milliseconds to wait after last call
     * @param callback  Function to execute after debounce period
     */
    void debounce(const juce::String& key, int delayMs, std::function<void()> callback);

    /**
     * Cancel any pending debounced callback for the given key.
     *
     * @param key       The debounce group to cancel
     */
    void cancelDebounce(const juce::String& key);

    /**
     * Cancel all pending debounced callbacks.
     */
    void cancelAllDebounces();

    //==========================================================================
    // Throttling

    /**
     * Throttle calls to a function - executes at most once per period.
     * Unlike debounce, this ensures the callback fires periodically during
     * rapid calls, rather than waiting for calls to stop.
     *
     * @param key       Unique identifier for this throttle group
     * @param periodMs  Minimum time between executions in milliseconds
     * @param callback  Function to execute
     */
    void throttle(const juce::String& key, int periodMs, std::function<void()> callback);

    /**
     * Cancel throttling for the given key.
     *
     * @param key       The throttle group to cancel
     */
    void cancelThrottle(const juce::String& key);

    //==========================================================================
    // Shutdown

    /**
     * Shutdown the async system - call before app exit to prevent hangs.
     * This cancels all pending timers and prevents new async work from starting.
     * Should be called early in the destruction sequence, before other subsystems
     * that might be accessed by pending callbacks.
     */
    void shutdown();

}  // namespace Async
