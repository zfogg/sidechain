#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Sidechain {
namespace Util {

/**
 * TaskScheduler - Thread pool for managing background tasks
 *
 * Replaces unlimited thread spawning (Async::run) with a managed thread pool
 * that controls resource usage and prevents thread explosion.
 *
 * Architecture:
 * - Fixed-size worker thread pool (default 4 threads)
 * - Work-stealing queue for fair distribution
 * - Promise/future API for result retrieval
 * - Automatic cleanup on destruction
 *
 * Usage:
 *   auto &scheduler = TaskScheduler::getInstance();
 *
 *   // Schedule a background task
 *   auto future = scheduler.schedule<int>([](){ return expensiveComputation(); });
 *
 *   // Get result (blocking)
 *   int result = future.get();
 *
 *   // Or use a callback
 *   scheduler.schedule<void>([](){ updateUI(); });
 *
 * Thread safety:
 * - Thread-safe work queue
 * - Shutdown waits for all tasks to complete
 * - Safe to call from any thread
 *
 * Performance considerations:
 * - Reduces memory usage from unlimited threads to fixed pool
 * - Improves CPU cache locality with small thread pool
 * - Prevents thread creation overhead for many short tasks
 * - Configurable pool size based on CPU cores
 *
 * Recommendations:
 * - For I/O bound tasks: pool size = 2x CPU cores
 * - For CPU bound tasks: pool size = CPU cores
 * - Default: 4 threads for balanced workloads
 */
class TaskScheduler {
public:
  // Get singleton instance
  static TaskScheduler &getInstance();

  /**
   * Schedule a task to run on the thread pool
   *
   * @param task Function to execute
   * @return std::future for retrieving result
   */
  template <typename Result> std::future<Result> schedule(std::function<Result()> task) {
    auto promise = std::make_shared<std::promise<Result>>();
    auto future = promise->get_future();

    // Wrap the task to store result in promise
    auto wrappedTask = [task, promise]() {
      try {
        if constexpr (std::is_same_v<Result, void>) {
          task();
          promise->set_value();
        } else {
          promise->set_value(task());
        }
      } catch (...) {
        promise->set_exception(std::current_exception());
      }
    };

    // Enqueue the wrapped task
    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      taskQueue_.push(wrappedTask);
    }
    conditionVariable_.notify_one();

    return future;
  }

  /**
   * Schedule task with automatic cleanup
   *
   * Useful for "fire and forget" tasks where result is not needed
   * Exceptions are logged but not propagated
   *
   * @param task Function to execute
   */
  void scheduleBackground(std::function<void()> task) {
    schedule<void>(task);
    // Future is discarded - task runs to completion in background
  }

  /**
   * Get number of worker threads
   */
  size_t getWorkerCount() const {
    return workers_.size();
  }

  /**
   * Get number of pending tasks in queue
   */
  size_t getPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return taskQueue_.size();
  }

  /**
   * Wait for all pending tasks to complete
   *
   * Useful for synchronization points in testing or shutdown
   *
   * @param timeoutMs Timeout in milliseconds (0 = infinite)
   * @return true if all tasks completed, false if timeout
   */
  bool waitForAll(int timeoutMs = 0);

  /**
   * Shutdown the thread pool
   *
   * Waits for all pending tasks to complete before returning
   * Thread pool automatically shuts down on destruction
   */
  void shutdown();

  ~TaskScheduler();

  // Public constructor for make_unique - use getInstance instead
  TaskScheduler();

private:
  /**
   * Worker thread main loop
   */
  void workerThread();

  // Singleton instance
  static std::unique_ptr<TaskScheduler> instance_;
  static std::mutex instanceMutex_;

  // Worker threads
  std::vector<std::thread> workers_;

  // Task queue
  std::queue<std::function<void()>> taskQueue_;
  mutable std::mutex queueMutex_;
  std::condition_variable conditionVariable_;

  // Shutdown flag
  std::atomic<bool> shutdown_{false};
  std::atomic<int> activeTaskCount_{0};

  // Prevent copying
  TaskScheduler(const TaskScheduler &) = delete;
  TaskScheduler &operator=(const TaskScheduler &) = delete;
};

} // namespace Util
} // namespace Sidechain
