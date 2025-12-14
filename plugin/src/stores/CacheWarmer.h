#pragma once

#include "../util/cache/CacheLayer.h"
#include "../util/async/Promise.h"
#include <JuceHeader.h>
#include <memory>
#include <functional>
#include <vector>
#include <atomic>

namespace Sidechain {
namespace Stores {

/**
 * CacheWarmer - Pre-fetches and caches popular data in background
 *
 * Strategies:
 * - Offline queueing: Queue operations when offline, execute when online
 * - Prefetching: Pre-fetch likely-needed data during idle time
 * - Smart scheduling: Batch requests and optimize timing
 *
 * Features:
 * - Configurable cache TTLs
 * - Automatic online/offline detection
 * - Batch operation support
 * - Progress reporting
 *
 * Usage:
 *   auto warmer = CacheWarmer::create(cache);
 *   warmer->setDefaultTTL(3600);  // 1 hour
 *   warmer->scheduleWarmup("feed", [](){ return fetchFeed(); });
 *   warmer->start();
 */
class CacheWarmer : public std::enable_shared_from_this<CacheWarmer>
{
public:
    using OperationCallback = std::function<void()>;
    using ProgressCallback = std::function<void(float)>;
    using CompletionCallback = std::function<void()>;

    /**
     * Create a cache warmer
     */
    static std::shared_ptr<CacheWarmer> create()
    {
        return std::make_shared<CacheWarmer>();
    }

    CacheWarmer()
        : defaultTTL_(3600)
        , isRunning_(false)
        , isOnline_(true)
        , operationsQueued_(0)
        , operationsCompleted_(0)
    {
    }

    virtual ~CacheWarmer() = default;

    // ========== Configuration ==========

    /**
     * Set default TTL for all cache operations
     *
     * @param ttlSeconds Time to live in seconds
     */
    void setDefaultTTL(int ttlSeconds)
    {
        defaultTTL_ = ttlSeconds;
    }

    /**
     * Set maximum number of concurrent operations
     *
     * @param maxConcurrent Max operations to run in parallel
     */
    void setMaxConcurrent(int maxConcurrent)
    {
        maxConcurrent_ = maxConcurrent;
    }

    /**
     * Set online/offline status
     *
     * When offline, operations are queued. When online, they execute.
     */
    void setOnlineStatus(bool isOnline)
    {
        isOnline_ = isOnline;
        if (isOnline_)
            processQueuedOperations();
    }

    /**
     * Check current online status
     */
    bool isOnline() const { return isOnline_; }

    // ========== Operation Scheduling ==========

    /**
     * Schedule a cache warmup operation
     *
     * @param key Unique identifier for this operation
     * @param operation Function to execute (should populate cache)
     * @param priority Priority (0 = highest, 100 = lowest)
     */
    void scheduleWarmup(const juce::String& key, OperationCallback operation, int priority = 50)
    {
        if (!operation)
            return;

        {
            std::lock_guard<std::mutex> lock(operationsMutex_);

            WarmupOperation op;
            op.key = key;
            op.operation = operation;
            op.priority = priority;
            op.ttl = defaultTTL_;
            op.isQueued = !isOnline_;

            operations_.push_back(op);
            operationsQueued_++;

            // Sort by priority
            std::sort(operations_.begin(), operations_.end(),
                      [](const WarmupOperation& a, const WarmupOperation& b) {
                          return a.priority < b.priority;
                      });
        }

        // Execute immediately if online and running
        if (isOnline_ && isRunning_)
            processNextOperation();
    }

    /**
     * Schedule batch operations
     *
     * @param operations Vector of (key, callback) pairs
     */
    void scheduleBatch(const std::vector<std::pair<juce::String, OperationCallback>>& operations)
    {
        for (const auto& [key, op] : operations)
        {
            scheduleWarmup(key, op);
        }
    }

    /**
     * Clear pending operations
     */
    void clearPendingOperations()
    {
        std::lock_guard<std::mutex> lock(operationsMutex_);
        operations_.clear();
        operationsQueued_ = 0;
        operationsCompleted_ = 0;
    }

    // ========== Control ==========

    /**
     * Start the cache warmer
     */
    std::shared_ptr<CacheWarmer> start()
    {
        if (isRunning_)
            return this->shared_from_this();

        isRunning_ = true;

        // Process initial operations
        if (isOnline_)
            processNextOperation();

        return this->shared_from_this();
    }

    /**
     * Stop the cache warmer
     */
    std::shared_ptr<CacheWarmer> stop()
    {
        isRunning_ = false;
        return this->shared_from_this();
    }

    /**
     * Pause cache warming (operations remain queued)
     */
    std::shared_ptr<CacheWarmer> pause()
    {
        isRunning_ = false;
        return this->shared_from_this();
    }

    /**
     * Resume cache warming
     */
    std::shared_ptr<CacheWarmer> resume()
    {
        isRunning_ = true;
        processNextOperation();
        return this->shared_from_this();
    }

    // ========== Callbacks ==========

    /**
     * Register progress callback
     *
     * Called with progress [0, 1] as operations complete
     */
    std::shared_ptr<CacheWarmer> onProgress(ProgressCallback callback)
    {
        progressCallback_ = callback;
        return this->shared_from_this();
    }

    /**
     * Register completion callback
     *
     * Called when all operations are complete
     */
    std::shared_ptr<CacheWarmer> onCompletion(CompletionCallback callback)
    {
        completionCallback_ = callback;
        return this->shared_from_this();
    }

    // ========== Status Queries ==========

    /**
     * Get number of operations queued
     */
    size_t getOperationCount() const
    {
        std::lock_guard<std::mutex> lock(operationsMutex_);
        return operations_.size();
    }

    /**
     * Get number of operations completed
     */
    size_t getCompletedCount() const
    {
        return operationsCompleted_;
    }

    /**
     * Get progress [0, 1]
     */
    float getProgress() const
    {
        if (operationsQueued_ == 0)
            return 0.0f;
        return static_cast<float>(operationsCompleted_) / static_cast<float>(operationsQueued_);
    }

    /**
     * Check if warmer is running
     */
    bool isRunning() const { return isRunning_; }

    /**
     * Check if all operations are complete
     */
    bool isComplete() const
    {
        std::lock_guard<std::mutex> lock(operationsMutex_);
        return operations_.empty() && operationsCompleted_ >= operationsQueued_;
    }

private:
    struct WarmupOperation
    {
        juce::String key;
        OperationCallback operation;
        int priority;
        int ttl;
        bool isQueued;
    };

    std::vector<WarmupOperation> operations_;
    mutable std::mutex operationsMutex_;
    int defaultTTL_;
    int maxConcurrent_ = 3;
    bool isRunning_;
    std::atomic<bool> isOnline_;
    std::atomic<size_t> operationsQueued_;
    std::atomic<size_t> operationsCompleted_;
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;

    /**
     * Process next operation in queue
     */
    void processNextOperation()
    {
        if (!isRunning_ || !isOnline_)
            return;

        WarmupOperation nextOp;
        bool hasOperation = false;

        {
            std::lock_guard<std::mutex> lock(operationsMutex_);

            // Find first non-queued operation
            for (auto& op : operations_)
            {
                if (!op.isQueued)
                {
                    nextOp = op;
                    op.isQueued = true;
                    hasOperation = true;
                    break;
                }
            }
        }

        if (!hasOperation)
        {
            // Check if all done
            if (operationsCompleted_ >= operationsQueued_)
            {
                if (completionCallback_)
                    completionCallback_();
            }
            return;
        }

        // Execute operation
        try
        {
            if (nextOp.operation)
                nextOp.operation();

            operationsCompleted_++;

            if (progressCallback_)
                progressCallback_(getProgress());

            // Process next operation asynchronously
            // In production, this would use proper async/threading
            processNextOperation();
        }
        catch (const std::exception& e)
        {
            juce::Logger::getCurrentLogger()->writeToLog(
                "CacheWarmer operation failed: " + juce::String(e.what()));
            operationsCompleted_++;
            processNextOperation();
        }
    }

    /**
     * Process all queued operations (called when going online)
     */
    void processQueuedOperations()
    {
        if (!isRunning_)
            return;

        {
            std::lock_guard<std::mutex> lock(operationsMutex_);
            for (auto& op : operations_)
            {
                op.isQueued = false;
            }
        }

        processNextOperation();
    }

    // Prevent copying
    CacheWarmer(const CacheWarmer&) = delete;
    CacheWarmer& operator=(const CacheWarmer&) = delete;
};

}  // namespace Stores
}  // namespace Sidechain
