#pragma once

#include "JuceScheduler.h"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace Sidechain {
namespace Rx {

/**
 * StateSubject<T> - Thread-safe reactive state container (BehaviorSubject pattern)
 *
 * Core RxCpp-style state management for the Sidechain plugin.
 * Holds current state value and notifies subscribers on changes.
 *
 * Key features:
 * - Thread-safe read/write with shared_mutex
 * - Immediate notification to new subscribers with current value
 * - Selector subscriptions (only notify when derived value changes)
 * - Returns unsubscribe function for cleanup
 * - Compatible with RxCpp observable composition via asObservable()
 *
 * Usage:
 *   StateSubject<AuthState> auth(AuthState{});
 *
 *   // Read current value
 *   auto current = auth.getValue();
 *
 *   // Update value (notifies all subscribers)
 *   auth.next(newState);
 *
 *   // Subscribe to all changes
 *   auto unsub = auth.subscribe([](const AuthState& s) { updateUI(s); });
 *
 *   // Subscribe to derived value (only when isLoggedIn changes)
 *   auto unsub2 = auth.select(
 *       [](const AuthState& s) { return s.isLoggedIn; },
 *       [](bool loggedIn) { updateLoginButton(loggedIn); }
 *   );
 *
 *   // Cleanup
 *   unsub();
 *   unsub2();
 */
template <typename T> class StateSubject {
public:
  using Callback = std::function<void(const T &)>;
  using Unsubscriber = std::function<void()>;

  explicit StateSubject(T initialValue = T{}) : value_(std::move(initialValue)) {}

  // Non-copyable, movable
  StateSubject(const StateSubject &) = delete;
  StateSubject &operator=(const StateSubject &) = delete;
  StateSubject(StateSubject &&) = default;
  StateSubject &operator=(StateSubject &&) = default;

  /**
   * Get current value (thread-safe read, returns copy)
   */
  T getValue() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return value_;
  }

  /**
   * Get current state reference (thread-safe read)
   */
  const T &getState() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return value_;
  }

  /**
   * Update value and notify all subscribers (thread-safe write)
   */
  void next(T newValue) {
    std::vector<Callback> callbacksCopy;
    {
      std::unique_lock<std::shared_mutex> lock(mutex_);
      value_ = std::move(newValue);

      // Copy callbacks to avoid holding lock during notification
      std::shared_lock<std::shared_mutex> subLock(subscribersMutex_);
      for (const auto &[id, callback] : subscribers_) {
        callbacksCopy.push_back(callback);
      }
    }

    // Notify outside lock to prevent deadlocks
    for (const auto &callback : callbacksCopy) {
      callback(value_);
    }
  }

  /**
   * Update state (alias for next())
   */
  void setState(const T &newValue) {
    next(newValue);
  }

  /**
   * Subscribe to value changes
   * Callback is invoked immediately with current value, then on each change.
   * Returns unsubscriber function.
   */
  Unsubscriber subscribe(Callback callback) {
    int subscriptionId;
    {
      std::unique_lock<std::shared_mutex> lock(subscribersMutex_);
      subscriptionId = nextId_++;
      subscribers_.emplace_back(subscriptionId, std::move(callback));
    }

    // Immediate callback with current value
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      subscribers_.back().second(value_);
    }

    return [this, subscriptionId]() { unsubscribe(subscriptionId); };
  }

  /**
   * Subscribe to derived/selected value
   * Only notifies when the selected value changes.
   *
   * Example:
   *   auth.select(
   *       [](const AuthState& s) { return s.isLoggedIn; },
   *       [](bool loggedIn) { updateUI(loggedIn); }
   *   );
   */
  template <typename Derived>
  Unsubscriber select(std::function<Derived(const T &)> selector, std::function<void(const Derived &)> callback) {
    auto prevValue = std::make_shared<std::optional<Derived>>();

    return subscribe([sel = std::move(selector), cb = std::move(callback), prevValue](const T &state) {
      Derived currentValue = sel(state);

      // Only call callback if selected value changed
      if (!prevValue->has_value() || !(prevValue->value() == currentValue)) {
        *prevValue = currentValue;
        cb(currentValue);
      }
    });
  }

  /**
   * Subscribe to derived/selected value (alias for select())
   */
  template <typename Derived>
  Unsubscriber subscribeToSelection(std::function<Derived(const T &)> selector,
                                    std::function<void(const Derived &)> callback) {
    return select(std::move(selector), std::move(callback));
  }

  /**
   * Get observable that emits on value changes
   * Integrates with RxCpp for composition with other observables
   */
  rxcpp::observable<T> asObservable() const {
    return rxcpp::sources::create<T>([this](auto subscriber) {
      auto unsub =
          const_cast<StateSubject *>(this)->subscribe([subscriber](const T &value) { subscriber.on_next(value); });

      // Note: In a full implementation, we'd need to handle unsubscription
      // when the observable is disposed. For now, the subscription lives
      // until the StateSubject is destroyed.
    });
  }

  /**
   * Update value using a transform function
   * Useful for immutable updates: state.update([](auto s) { s.count++; return s; });
   */
  void update(std::function<T(T)> transform) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    T newValue = transform(value_);
    lock.unlock();
    next(std::move(newValue));
  }

private:
  void unsubscribe(int subscriptionId) {
    std::unique_lock<std::shared_mutex> lock(subscribersMutex_);
    auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
                           [subscriptionId](const auto &pair) { return pair.first == subscriptionId; });
    if (it != subscribers_.end()) {
      subscribers_.erase(it);
    }
  }

  T value_;
  mutable std::shared_mutex mutex_;

  std::vector<std::pair<int, Callback>> subscribers_;
  int nextId_ = 0;
  mutable std::shared_mutex subscribersMutex_;
};

/**
 * Convenience alias for shared_ptr to StateSubject
 * Used throughout the app for reactive state management
 */
template <typename T> using State = std::shared_ptr<StateSubject<T>>;

/**
 * Factory function to create a State<T>
 */
template <typename T> State<T> makeState(T initialValue = T{}) {
  return std::make_shared<StateSubject<T>>(std::move(initialValue));
}

} // namespace Rx
} // namespace Sidechain
