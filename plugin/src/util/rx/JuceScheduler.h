#pragma once

#include <JuceHeader.h>
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Rx {

// ==============================================================================
// JUCE Message Thread Scheduler
// ==============================================================================
//
// Custom RxCpp scheduler that dispatches work to the JUCE message thread.
// Use with observe_on() to ensure UI updates happen on the correct thread.
//
// Usage:
//   someObservable
//       .observe_on(Rx::juce_message_thread())
//       .subscribe([this](auto value) {
//           // This runs on the JUCE message thread
//           updateUI(value);
//       });
//
// Thread Safety:
// - All work is dispatched via juce::MessageManager::callAsync()
// - Safe to call from any thread
// - Callbacks guaranteed to run on message thread
//
// ==============================================================================

/**
 * RxCpp worker that dispatches work items to the JUCE message thread.
 *
 * This implements the worker interface required by rxcpp::schedulers::scheduler.
 * Each work item is queued via juce::MessageManager::callAsync().
 */
class juce_message_thread_worker : public rxcpp::schedulers::worker_interface {
public:
  using clock_type = rxcpp::schedulers::scheduler_base::clock_type;

  juce_message_thread_worker() = default;

  /**
   * Get current time for scheduling.
   * Uses rxcpp's default clock.
   */
  clock_type::time_point now() const override {
    return clock_type::now();
  }

  /**
   * Schedule work to run on the JUCE message thread.
   *
   * @param scbl The schedulable action to execute
   */
  void schedule(const rxcpp::schedulers::schedulable &scbl) const override {
    schedule(now(), scbl);
  }

  /**
   * Schedule work to run at a specific time on the JUCE message thread.
   *
   * If the time is in the future, we delay before dispatching.
   * If the time is now or past, we dispatch immediately.
   *
   * @param when Time point when work should execute
   * @param scbl The schedulable action to execute
   */
  void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable &scbl) const override {
    if (!scbl.is_subscribed()) {
      return;
    }

    auto delay = when - now();

    if (delay <= clock_type::duration::zero()) {
      // Execute immediately on message thread
      dispatch_to_message_thread(scbl);
    } else {
      // Delay then execute on message thread
      auto delayMs = std::chrono::duration_cast<std::chrono::milliseconds>(delay).count();

      // Capture schedulable by value for safety
      auto action = std::make_shared<rxcpp::schedulers::schedulable>(scbl);

      // Use JUCE Timer for delayed execution
      juce::Timer::callAfterDelay(static_cast<int>(delayMs), [action]() {
        if (action->is_subscribed()) {
          // Create a recurse object that allows the schedulable to reschedule itself
          rxcpp::schedulers::recursion r;
          r.reset(true); // Allow recursion
          (*action)(r.get_recurse());
        }
      });
    }
  }

private:
  /**
   * Dispatch a schedulable action to the JUCE message thread.
   *
   * Uses juce::MessageManager::callAsync() which is thread-safe
   * and can be called from any thread.
   */
  void dispatch_to_message_thread(const rxcpp::schedulers::schedulable &scbl) const {
    // Capture schedulable by value for thread safety
    auto action = std::make_shared<rxcpp::schedulers::schedulable>(scbl);

    juce::MessageManager::callAsync([action]() {
      if (action->is_subscribed()) {
        // Create a recurse object that allows the schedulable to reschedule itself
        rxcpp::schedulers::recursion r;
        r.reset(true); // Allow recursion
        (*action)(r.get_recurse());
      }
    });
  }
};

/**
 * RxCpp scheduler that runs work on the JUCE message thread.
 *
 * This wraps juce_message_thread_worker and provides the scheduler interface
 * required by rxcpp's observe_on() and subscribe_on() operators.
 */
class juce_message_thread_scheduler : public rxcpp::schedulers::scheduler_interface {
public:
  using clock_type = rxcpp::schedulers::scheduler_base::clock_type;

  /**
   * Get current time for scheduling.
   */
  clock_type::time_point now() const override {
    return clock_type::now();
  }

  /**
   * Create a worker for this scheduler.
   * Each worker dispatches to the same JUCE message thread.
   */
  rxcpp::schedulers::worker create_worker(rxcpp::composite_subscription cs) const override {
    return rxcpp::schedulers::worker(std::move(cs), std::make_shared<juce_message_thread_worker>());
  }
};

/**
 * Get a scheduler that runs work on the JUCE message thread.
 *
 * Use with observe_on() to ensure downstream operators and
 * subscribers run on the message thread.
 *
 * Example:
 *   networkObservable
 *       .observe_on(juce_message_thread())
 *       .subscribe([this](auto data) {
 *           // Safe to update UI here
 *           label.setText(data.name);
 *       });
 *
 * @return rxcpp::scheduler configured for JUCE message thread
 */
inline rxcpp::schedulers::scheduler juce_message_thread() {
  static auto scheduler = rxcpp::schedulers::make_scheduler<juce_message_thread_scheduler>();
  return scheduler;
}

/**
 * Convenience function to create an observe_on coordination for JUCE thread.
 *
 * Example:
 *   someObservable
 *       .observe_on(observe_on_juce_thread())
 *       .subscribe(...);
 *
 * @return rxcpp::observe_on_one_worker coordination
 */
inline auto observe_on_juce_thread() {
  return rxcpp::observe_on_one_worker(juce_message_thread());
}

// ==============================================================================
// Helper Functions for Common Patterns
// ==============================================================================

/**
 * Create an observable that emits on the JUCE message thread.
 *
 * Wraps rxcpp::sources::create with automatic observe_on(juce_message_thread()).
 *
 * @param factory Function that creates the observable's emissions
 * @return Observable that emits on the JUCE message thread
 */
template <typename T, typename Factory> auto create_on_juce_thread(Factory &&factory) {
  return rxcpp::sources::create<T>(std::forward<Factory>(factory)).observe_on(observe_on_juce_thread());
}

/**
 * Helper to run async work and deliver results on JUCE thread.
 *
 * Pattern for network operations:
 *   return async_to_juce_thread<juce::Image>([this, url](auto observer) {
 *       networkClient->fetchImage(url, [observer](auto result) {
 *           if (result.isOk()) {
 *               observer.on_next(result.getValue());
 *               observer.on_completed();
 *           } else {
 *               observer.on_error(std::make_exception_ptr(
 *                   std::runtime_error(result.getError().toStdString())));
 *           }
 *       });
 *   });
 *
 * @param factory Async work factory (receives observer, calls back when done)
 * @return Observable that delivers on JUCE message thread
 */
template <typename T, typename Factory> auto async_to_juce_thread(Factory &&factory) {
  return rxcpp::sources::create<T>(std::forward<Factory>(factory)).observe_on(observe_on_juce_thread());
}

// ==============================================================================
// Retry with Backoff
// ==============================================================================

/**
 * Configuration for retry with exponential backoff.
 */
struct RetryConfig {
  int maxRetries = 3;                                            // Maximum number of retry attempts
  std::chrono::milliseconds initialDelay{1000};                  // Initial delay before first retry
  std::chrono::milliseconds maxDelay{4000};                      // Maximum delay between retries
  double backoffMultiplier = 2.0;                                // Multiplier for exponential backoff
  std::function<bool(std::exception_ptr)> shouldRetry = nullptr; // Optional: custom retry condition
};

/**
 * Retry an observable with exponential backoff.
 *
 * Retries failed observables with increasing delays: 1s, 2s, 4s (with default config).
 * All delays and retries occur on the JUCE message thread.
 *
 * Usage:
 *   retryWithBackoff(loadFeedObservable())
 *       .subscribe([](auto data) { ... });
 *
 *   // With custom config
 *   RetryConfig config;
 *   config.maxRetries = 5;
 *   config.initialDelay = std::chrono::milliseconds(500);
 *   retryWithBackoff(loadDataObservable(), config)
 *       .subscribe([](auto data) { ... });
 *
 * @param source Observable to retry on failure
 * @param config Retry configuration (uses defaults if not specified)
 * @return Observable that retries on failure with backoff
 */
template <typename T>
rxcpp::observable<T> retryWithBackoff(rxcpp::observable<T> sourceObs, RetryConfig config = RetryConfig{}) {
  return rxcpp::observable<>::create<T>([source = std::move(sourceObs), config](rxcpp::subscriber<T> subscriber) {
    auto retryCount = std::make_shared<int>(0);
    auto currentDelay = std::make_shared<std::chrono::milliseconds>(config.initialDelay);
    auto subscription = std::make_shared<rxcpp::composite_subscription>();

    std::function<void()> attempt;
    attempt = [source, config, retryCount, currentDelay, subscription, subscriber, &attempt]() {
      // Subscribe to source
      source.subscribe(
          *subscription,
          [subscriber](T value) {
            // Forward successful values
            subscriber.on_next(std::move(value));
          },
          [config, retryCount, currentDelay, subscription, subscriber, &attempt](std::exception_ptr ep) {
            // Check if we should retry
            bool shouldRetry = *retryCount < config.maxRetries;

            // Apply custom retry condition if provided
            if (shouldRetry && config.shouldRetry) {
              shouldRetry = config.shouldRetry(ep);
            }

            if (shouldRetry) {
              (*retryCount)++;

              // Schedule retry after delay
              int delayMs = static_cast<int>(currentDelay->count());
              juce::Timer::callAfterDelay(delayMs, [&attempt]() { attempt(); });

              // Increase delay for next retry (exponential backoff)
              auto nextDelay = std::chrono::milliseconds(
                  static_cast<long long>(static_cast<double>(currentDelay->count()) * config.backoffMultiplier));
              *currentDelay = std::min(nextDelay, config.maxDelay);
            } else {
              // Max retries exceeded, propagate error
              subscriber.on_error(ep);
            }
          },
          [subscriber]() {
            // Forward completion
            subscriber.on_completed();
          });
    };

    // Start first attempt
    attempt();

    // Return subscription for cancellation
    subscriber.add(*subscription);
  });
}

/**
 * Convenience overload with default configuration.
 * Retries 3 times with 1s, 2s, 4s delays.
 */
template <typename T> rxcpp::observable<T> retryWithBackoff(rxcpp::observable<T> source, int maxRetries) {
  RetryConfig config;
  config.maxRetries = maxRetries;
  return retryWithBackoff(std::move(source), config);
}

// ==============================================================================
// Debounce Helpers for Search
// ==============================================================================

/**
 * Create a debounced search pipeline from a query subject.
 *
 * Takes a subject that receives search queries and returns an observable
 * that debounces input and flat-maps to search results.
 *
 * Usage:
 *   rxcpp::subjects::subject<juce::String> querySubject;
 *
 *   // Set up debounced search
 *   auto searchResults = debouncedSearch(
 *       querySubject.get_observable(),
 *       [this](auto q) { return appStore.searchPostsObservable(q); }
 *   );
 *
 *   // Subscribe to results
 *   searchResults.subscribe([this](auto posts) {
 *       displayResults(posts);
 *   });
 *
 *   // Push queries from UI
 *   querySubject.get_subscriber().on_next("beat");
 *
 * @param queryStream Observable of query strings
 * @param searchFn Function that performs the search (returns observable)
 * @param debounceMs Debounce delay in milliseconds (default 300)
 * @return Observable of search results
 */
template <typename T, typename SearchFn>
auto debouncedSearch(rxcpp::observable<juce::String> queryStream, SearchFn searchFunc,
                     std::chrono::milliseconds debounceMs = std::chrono::milliseconds(300)) {
  return queryStream.debounce(debounceMs, observe_on_juce_thread())
      .distinct_until_changed()
      .flat_map([fn = std::move(searchFunc)](const juce::String &query) { return fn(query); })
      .observe_on(observe_on_juce_thread());
}

} // namespace Rx
} // namespace Sidechain
