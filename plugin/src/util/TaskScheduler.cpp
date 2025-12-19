#include "TaskScheduler.h"
#include <thread>

namespace Sidechain {
namespace Util {

std::unique_ptr<TaskScheduler> TaskScheduler::instance_;
std::mutex TaskScheduler::instanceMutex_;

TaskScheduler::TaskScheduler() {
  // Determine number of worker threads
  // Use hardware concurrency if available, cap at reasonable maximum
  size_t numCores = std::thread::hardware_concurrency();
  if (numCores == 0) {
    numCores = 4; // Fallback default
  }

  // For balanced workloads, use a conservative pool size
  // I/O bound: 2x cores, CPU bound: 1x cores, balanced: 1.5x or just use fixed 4
  size_t poolSize = std::min(static_cast<size_t>(8), numCores);

  // Create worker threads
  for (size_t i = 0; i < poolSize; ++i) {
    workers_.emplace_back([this]() { workerThread(); });
  }
}

TaskScheduler::~TaskScheduler() {
  shutdown();
}

TaskScheduler &TaskScheduler::getInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex_);
  if (!instance_) {
    instance_ = std::make_unique<TaskScheduler>();
  }
  return *instance_;
}

void TaskScheduler::workerThread() {
  while (!shutdown_) {
    std::unique_lock<std::mutex> lock(queueMutex_);

    // Wait for task or shutdown signal
    conditionVariable_.wait(lock, [this]() { return !taskQueue_.empty() || shutdown_; });

    if (shutdown_ && taskQueue_.empty()) {
      break;
    }

    if (taskQueue_.empty()) {
      continue;
    }

    // Get task from queue
    auto task = std::move(taskQueue_.front());
    taskQueue_.pop();
    activeTaskCount_++;

    lock.unlock();

    // Execute task outside of lock
    try {
      task();
    } catch (...) {
      // Log but don't propagate - background tasks shouldn't crash thread
    }

    activeTaskCount_--;
  }
}

bool TaskScheduler::waitForAll(int timeoutMs) {
  auto start = std::chrono::steady_clock::now();

  while (true) {
    {
      std::lock_guard<std::mutex> lock(queueMutex_);
      if (taskQueue_.empty() && activeTaskCount_ == 0) {
        return true;
      }
    }

    // Check timeout
    if (timeoutMs > 0) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
      if (elapsed.count() >= timeoutMs) {
        return false;
      }
    }

    // Yield to prevent busy waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void TaskScheduler::shutdown() {
  if (shutdown_.exchange(true)) {
    return; // Already shut down
  }

  // Wake up all worker threads
  {
    std::unique_lock<std::mutex> lock(queueMutex_);
    conditionVariable_.notify_all();
  }

  // Wait for all workers to finish
  for (auto &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  workers_.clear();
}

} // namespace Util
} // namespace Sidechain
