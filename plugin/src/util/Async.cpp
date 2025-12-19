#include "Async.h"

namespace Async {
// ==========================================================================
// Internal Timer Management

// We use a custom timer class to manage delayed execution. Each timer
// has a unique ID and stores its callback. This pattern avoids the need
// for users to manage timer lifecycle manually.
// ==========================================================================

namespace {
// Thread-safe counter for generating unique timer IDs
std::atomic<int> nextTimerId{1};

// Shutdown flag to prevent new async work after shutdown starts
std::atomic<bool> isShuttingDown{false};

/**
 * DelayTimer - Custom timer for delayed execution
 *
 * Timer that executes a callback once after a delay and then
 * self-destructs. Used internally by Async::delay().
 */
class DelayTimer : public juce::Timer {
public:
  /**
   * Create a delay timer
   * @param id Unique timer ID
   * @param cb Callback to execute when timer fires
   */
  DelayTimer(int id, std::function<void()> cb) : timerId(id), callback(std::move(cb)) {}

  void timerCallback() override {
    stopTimer();
    if (callback)
      callback();
    // Self-destruct after firing
    // Must be done on message thread, which we're already on
    juce::MessageManager::callAsync([this]() { delete this; });
  }

  /** Get the unique timer ID */
  int getTimerId() const {
    return timerId;
  }

private:
  int timerId;                    // /< Unique identifier for this timer
  std::function<void()> callback; // /< Callback to execute
};

// Active delay timers (protected by mutex)
std::mutex delayTimersMutex;
std::map<int, DelayTimer *> delayTimers;

// Debounce timers keyed by string (protected by mutex)
std::mutex debounceMutex;
std::map<juce::String, std::unique_ptr<juce::Timer>> debounceTimers;

// Throttle state keyed by string (protected by mutex)
std::mutex throttleMutex;

/**
 * ThrottleState - State tracking for throttled function calls
 *
 * Tracks the last execution time and any pending timer for
 * a throttled callback key.
 */
struct ThrottleState {
  juce::int64 lastExecutionTime = 0;         // /< Timestamp of last execution
  std::unique_ptr<juce::Timer> pendingTimer; // /< Timer for pending execution
};
std::map<juce::String, ThrottleState> throttleStates;

/**
 * CallbackTimer - Simple timer wrapper for debounce/throttle
 *
 * Timer that can have its callback changed dynamically.
 * Used internally by Async::debounce() and Async::throttle().
 */
class CallbackTimer : public juce::Timer {
public:
  /**
   * Create a callback timer
   * @param cb Initial callback (may be nullptr)
   */
  explicit CallbackTimer(std::function<void()> cb) : callback(std::move(cb)) {}

  void timerCallback() override {
    stopTimer();
    if (callback)
      callback();
  }

  /**
   * Update the callback function
   * @param cb New callback to execute
   */
  void setCallback(std::function<void()> cb) {
    callback = std::move(cb);
  }

private:
  std::function<void()> callback; // /< Callback to execute when timer fires
};

} // anonymous namespace

// ==========================================================================
// Shutdown Check
// ==========================================================================

bool isShutdownInProgress() {
  return isShuttingDown.load();
}

// ==========================================================================
// Background Work (void version)
// ==========================================================================

/** Execute work on background thread with optional completion callback
 * @param work Function to execute on background thread
 * @param onComplete Optional callback called on message thread when work
 * completes
 */
void runVoid(std::function<void()> work, std::function<void()> onComplete) {
  // Don't start new work if we're shutting down
  if (isShuttingDown.load())
    return;

  std::thread([workFn = std::move(work), completeFn = std::move(onComplete)]() {
    if (workFn)
      workFn();

    // Only call back to message thread if not shutting down
    if (completeFn && !isShuttingDown.load()) {
      // Check if MessageManager still exists
      if (auto *mm = juce::MessageManager::getInstanceWithoutCreating()) {
        mm->callAsync([completeFn]() {
          if (!isShuttingDown.load())
            completeFn();
        });
      }
    }
  }).detach();
}

// ==========================================================================
// Delayed Execution
// ==========================================================================

/** Schedule a callback to execute after a delay on the message thread
 * @param delayMs Delay in milliseconds before executing callback
 * @param callback Function to call after delay
 * @return Timer ID for cancellation, or 0 if invalid parameters
 */
int delay(int delayMs, std::function<void()> callback) {
  if (!callback || delayMs < 0)
    return 0;

  int timerId = nextTimerId.fetch_add(1);

  // Must create timer on message thread
  juce::MessageManager::callAsync([timerId, delayMs, delayCallback = std::move(callback)]() {
    auto *timer = new DelayTimer(timerId, delayCallback);

    {
      std::lock_guard<std::mutex> lock(delayTimersMutex);
      delayTimers[timerId] = timer;
    }

    timer->startTimer(delayMs);
  });

  return timerId;
}

/** Cancel a pending delayed callback
 * @param timerId Timer ID returned from delay()
 */
void cancelDelay(int timerId) {
  if (timerId <= 0)
    return;

  juce::MessageManager::callAsync([timerId]() {
    std::lock_guard<std::mutex> lock(delayTimersMutex);
    auto it = delayTimers.find(timerId);
    if (it != delayTimers.end()) {
      it->second->stopTimer();
      delete it->second;
      delayTimers.erase(it);
    }
  });
}

// ==========================================================================
// Debouncing
// ==========================================================================

/** Debounce function calls - only executes after period of inactivity
 * @param key Unique identifier for this debounce group
 * @param delayMs Delay in milliseconds to wait after last call
 * @param callback Function to execute after debounce period
 */
void debounce(const juce::String &key, int delayMs, std::function<void()> callback) {
  if (key.isEmpty() || !callback || delayMs < 0)
    return;

  // Debounce logic must run on message thread for timer safety
  juce::MessageManager::callAsync([key, delayMs, debounceCallback = std::move(callback)]() {
    std::lock_guard<std::mutex> lock(debounceMutex);

    auto it = debounceTimers.find(key);
    if (it != debounceTimers.end()) {
      // Reset existing timer with new callback
      it->second->stopTimer();
      static_cast<CallbackTimer *>(it->second.get())->setCallback(debounceCallback);
      it->second->startTimer(delayMs);
    } else {
      // Create new timer
      auto timer = std::make_unique<CallbackTimer>(debounceCallback);
      timer->startTimer(delayMs);
      debounceTimers[key] = std::move(timer);
    }
  });
}

/** Cancel pending debounced callback for a key
 * @param key The debounce group to cancel
 */
void cancelDebounce(const juce::String &key) {
  if (key.isEmpty())
    return;

  juce::MessageManager::callAsync([key]() {
    std::lock_guard<std::mutex> lock(debounceMutex);
    auto it = debounceTimers.find(key);
    if (it != debounceTimers.end()) {
      it->second->stopTimer();
      debounceTimers.erase(it);
    }
  });
}

/** Cancel all pending debounced callbacks
 */
void cancelAllDebounces() {
  juce::MessageManager::callAsync([]() {
    std::lock_guard<std::mutex> lock(debounceMutex);
    for (auto &pair : debounceTimers) {
      pair.second->stopTimer();
    }
    debounceTimers.clear();
  });
}

// ==========================================================================
// Throttling
// ==========================================================================

/** Throttle function calls - executes at most once per period
 * @param key Unique identifier for this throttle group
 * @param periodMs Minimum time between executions in milliseconds
 * @param callback Function to execute
 */
void throttle(const juce::String &key, int periodMs, std::function<void()> callback) {
  if (key.isEmpty() || !callback || periodMs < 0)
    return;

  juce::MessageManager::callAsync([key, periodMs, throttleCallback = std::move(callback)]() {
    std::lock_guard<std::mutex> lock(throttleMutex);

    auto &state = throttleStates[key];
    juce::int64 now = juce::Time::currentTimeMillis();
    juce::int64 timeSinceLastExecution = now - state.lastExecutionTime;

    if (timeSinceLastExecution >= periodMs) {
      // Enough time has passed, execute immediately
      state.lastExecutionTime = now;
      throttleCallback();
    } else {
      // Schedule for later (replace any pending callback)
      int remainingMs = static_cast<int>(periodMs - timeSinceLastExecution);

      if (state.pendingTimer) {
        state.pendingTimer->stopTimer();
      } else {
        state.pendingTimer = std::make_unique<CallbackTimer>(nullptr);
      }

      // Wrap callback to update lastExecutionTime when it fires
      auto wrappedCallback = [key, throttleCallback]() {
        std::lock_guard<std::mutex> innerLock(throttleMutex);
        auto it = throttleStates.find(key);
        if (it != throttleStates.end()) {
          it->second.lastExecutionTime = juce::Time::currentTimeMillis();
        }
        throttleCallback();
      };

      static_cast<CallbackTimer *>(state.pendingTimer.get())->setCallback(wrappedCallback);
      state.pendingTimer->startTimer(remainingMs);
    }
  });
}

/** Cancel throttling for a key
 * @param key The throttle group to cancel
 */
void cancelThrottle(const juce::String &key) {
  if (key.isEmpty())
    return;

  juce::MessageManager::callAsync([key]() {
    std::lock_guard<std::mutex> lock(throttleMutex);
    auto it = throttleStates.find(key);
    if (it != throttleStates.end()) {
      if (it->second.pendingTimer)
        it->second.pendingTimer->stopTimer();
      throttleStates.erase(it);
    }
  });
}

// ==========================================================================
// Shutdown
// ==========================================================================

/** Shutdown the async system - call before app exit to prevent hangs
 * This should be called early in the destruction sequence.
 */
void shutdown() {
  // Set shutdown flag to prevent new work from starting
  isShuttingDown.store(true);

  // Cancel all pending timers synchronously (we're on message thread)
  {
    std::lock_guard<std::mutex> lock(delayTimersMutex);
    for (auto &pair : delayTimers) {
      pair.second->stopTimer();
      delete pair.second;
    }
    delayTimers.clear();
  }

  {
    std::lock_guard<std::mutex> lock(debounceMutex);
    for (auto &pair : debounceTimers) {
      pair.second->stopTimer();
    }
    debounceTimers.clear();
  }

  {
    std::lock_guard<std::mutex> lock(throttleMutex);
    for (auto &pair : throttleStates) {
      if (pair.second.pendingTimer)
        pair.second.pendingTimer->stopTimer();
    }
    throttleStates.clear();
  }

  // Give detached threads a moment to check the shutdown flag
  // This is a best-effort - we can't force-join detached threads
  juce::Thread::sleep(50);
}

} // namespace Async
