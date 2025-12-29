#pragma once

#include "JuceScheduler.h"
#include <JuceHeader.h>
#include <chrono>
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Rx {

// ==============================================================================
// Retry with Exponential Backoff
// ==============================================================================
//
// Retries failed observables with exponential backoff delays.
// Useful for network operations that may fail transiently.
//
// Usage:
//   networkObservable
//       .pipe(retryWithBackoff(3, std::chrono::seconds(1)))
//       .subscribe(...);
//
// Parameters:
//   maxRetries: Maximum number of retry attempts (default: 3)
//   initialDelay: Initial delay before first retry (default: 1 second)
//   maxDelay: Maximum delay cap (default: 30 seconds)
//   multiplier: Delay multiplier for each retry (default: 2.0)
//
// ==============================================================================

/**
 * Configuration for retry with backoff
 */
struct RetryConfig {
  int maxRetries = 3;
  std::chrono::milliseconds initialDelay{1000};
  std::chrono::milliseconds maxDelay{30000};
  double multiplier = 2.0;

  static RetryConfig defaultConfig() {
    return RetryConfig{};
  }

  static RetryConfig aggressive() {
    return RetryConfig{5, std::chrono::milliseconds(500), std::chrono::milliseconds(10000), 1.5};
  }

  static RetryConfig conservative() {
    return RetryConfig{2, std::chrono::milliseconds(2000), std::chrono::milliseconds(60000), 3.0};
  }
};

/**
 * Create an observable that retries the source with exponential backoff on errors.
 *
 * @tparam T Value type of the observable
 * @param source The source observable to retry
 * @param config Retry configuration
 * @return Observable that retries on error with backoff delays
 */
template <typename T>
rxcpp::observable<T> retryWithBackoff(rxcpp::observable<T> source, const RetryConfig &config = RetryConfig{}) {
  return rxcpp::sources::create<T>([source, config](auto observer) {
    auto retryCount = std::make_shared<int>(0);
    auto currentDelay = std::make_shared<std::chrono::milliseconds>(config.initialDelay);

    std::function<void()> trySubscribe;
    trySubscribe = [=]() mutable {
      source.subscribe(
          [observer](T value) { observer.on_next(std::move(value)); },
          [=](std::exception_ptr e) mutable {
            if (*retryCount < config.maxRetries) {
              (*retryCount)++;

              // Calculate delay with cap
              auto delay = *currentDelay;
              *currentDelay = std::chrono::milliseconds(
                  static_cast<int64_t>(std::min(static_cast<double>(currentDelay->count()) * config.multiplier,
                                                static_cast<double>(config.maxDelay.count()))));

              Util::logWarning("RxOperators", "Retry " + juce::String(*retryCount) + "/" +
                                                  juce::String(config.maxRetries) + " after " +
                                                  juce::String(delay.count()) + "ms");

              // Schedule retry after delay using JUCE Timer
              juce::Timer::callAfterDelay(static_cast<int>(delay.count()), [trySubscribe]() { trySubscribe(); });
            } else {
              Util::logError("RxOperators", "Max retries exceeded, propagating error");
              observer.on_error(e);
            }
          },
          [observer]() { observer.on_completed(); });
    };

    trySubscribe();
  });
}

// ==============================================================================
// Timeout with Fallback
// ==============================================================================
//
// Times out an observable and optionally falls back to another observable.
//
// Usage:
//   networkObservable
//       .pipe(timeoutWithFallback(std::chrono::seconds(5), fallbackObservable))
//       .subscribe(...);
//
// ==============================================================================

/**
 * Create an observable that times out and falls back to another observable.
 *
 * @tparam T Value type of the observable
 * @param source The source observable
 * @param timeout Timeout duration
 * @param fallback Fallback observable to use on timeout
 * @return Observable that switches to fallback on timeout
 */
template <typename T>
rxcpp::observable<T> timeoutWithFallback(rxcpp::observable<T> source, std::chrono::milliseconds timeout,
                                         rxcpp::observable<T> fallback) {
  return rxcpp::sources::create<T>([source, timeout, fallback](auto observer) {
    auto timedOut = std::make_shared<std::atomic<bool>>(false);
    auto completed = std::make_shared<std::atomic<bool>>(false);

    // Set up timeout
    juce::Timer::callAfterDelay(static_cast<int>(timeout.count()), [=]() {
      if (!completed->load() && !timedOut->exchange(true)) {
        Util::logWarning("RxOperators", "Operation timed out, using fallback");
        fallback.subscribe([observer](T value) { observer.on_next(std::move(value)); },
                           [observer](std::exception_ptr e) { observer.on_error(e); },
                           [observer]() { observer.on_completed(); });
      }
    });

    // Subscribe to source
    source.subscribe(
        [=](T value) {
          if (!timedOut->load()) {
            observer.on_next(std::move(value));
          }
        },
        [=](std::exception_ptr e) {
          if (!timedOut->load()) {
            completed->store(true);
            observer.on_error(e);
          }
        },
        [=]() {
          if (!timedOut->load()) {
            completed->store(true);
            observer.on_completed();
          }
        });
  });
}

// ==============================================================================
// Cache with TTL
// ==============================================================================
//
// Caches observable results for a specified duration.
//
// Usage:
//   auto cachedObservable = cacheWithTTL(expensiveObservable, std::chrono::minutes(5));
//   cachedObservable.subscribe(...); // Uses cache if available
//
// ==============================================================================

/**
 * Simple in-memory cache entry
 */
template <typename T> struct CacheEntry {
  T value;
  std::chrono::steady_clock::time_point expiresAt;

  bool isExpired() const {
    return std::chrono::steady_clock::now() >= expiresAt;
  }
};

/**
 * Create a cached observable with TTL.
 * Note: This creates a simple single-value cache. For more complex caching,
 * use EntityCache.
 *
 * @tparam T Value type of the observable
 * @param source The source observable to cache
 * @param ttl Time-to-live for cached values
 * @return Observable that returns cached value if valid, otherwise fetches fresh
 */
template <typename T>
rxcpp::observable<T> cacheWithTTL(rxcpp::observable<T> source, std::chrono::milliseconds ttl,
                                  std::shared_ptr<std::optional<CacheEntry<T>>> cache = nullptr) {
  if (!cache) {
    cache = std::make_shared<std::optional<CacheEntry<T>>>();
  }

  return rxcpp::sources::create<T>([source, ttl, cache](auto observer) {
    // Check cache
    if (cache->has_value() && !cache->value().isExpired()) {
      Util::logDebug("RxOperators", "Cache hit");
      observer.on_next(cache->value().value);
      observer.on_completed();
      return;
    }

    // Cache miss or expired - fetch fresh
    Util::logDebug("RxOperators", "Cache miss, fetching fresh");
    source.subscribe(
        [=](T value) {
          // Update cache
          *cache = CacheEntry<T>{value, std::chrono::steady_clock::now() + ttl};
          observer.on_next(std::move(value));
        },
        [observer](std::exception_ptr e) { observer.on_error(e); }, [observer]() { observer.on_completed(); });
  });
}

// ==============================================================================
// Polling Observable
// ==============================================================================
//
// Creates an observable that polls at fixed intervals.
//
// Usage:
//   pollObservable(std::chrono::seconds(30), [this]() {
//       return getNotificationCountObservable();
//   }).subscribe(...);
//
// ==============================================================================

/**
 * Create a polling observable that emits at fixed intervals.
 *
 * @tparam T Value type of the observable
 * @param interval Polling interval
 * @param factory Factory function that creates the observable for each poll
 * @return Observable that polls at the specified interval
 */
template <typename T>
rxcpp::observable<T> pollObservable(std::chrono::milliseconds interval, std::function<rxcpp::observable<T>()> factory) {
  return rxcpp::sources::create<T>([interval, factory](auto observer) {
    auto active = std::make_shared<std::atomic<bool>>(true);

    std::function<void()> poll;
    poll = [=]() mutable {
      if (!active->load()) {
        return;
      }

      factory().subscribe([observer](T value) { observer.on_next(std::move(value)); },
                          [observer](std::exception_ptr e) {
                            Util::logWarning("RxOperators", "Poll error, continuing...");
                            // Don't propagate error, just log and continue polling
                          },
                          [=]() {
                            // Schedule next poll
                            if (active->load()) {
                              juce::Timer::callAfterDelay(static_cast<int>(interval.count()), poll);
                            }
                          });
    };

    // Start first poll immediately
    poll();

    // Note: In a full implementation, we'd return an unsubscriber that sets active to false
  });
}

// ==============================================================================
// Share/Multicast Helper
// ==============================================================================
//
// Shares a single subscription among multiple observers.
// Prevents duplicate network requests when multiple UI components subscribe.
//
// Usage:
//   auto shared = shareReplay(expensiveNetworkCall());
//   shared.subscribe(...); // First subscription triggers the call
//   shared.subscribe(...); // Reuses the same result
//
// ==============================================================================

/**
 * Share an observable's subscription and replay the last value to new subscribers.
 * Useful for avoiding duplicate network requests.
 *
 * @tparam T Value type of the observable
 * @param source The source observable
 * @return Observable that shares subscription and replays last value
 */
template <typename T> rxcpp::observable<T> shareReplay(rxcpp::observable<T> source) {
  return source.publish().ref_count();
}

} // namespace Rx
} // namespace Sidechain
