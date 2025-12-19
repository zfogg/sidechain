#pragma once

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <optional>
#include <mutex>
#include <vector>

namespace Sidechain {
namespace Stores {

/**
 * StateSlice - Foundation for slice-based store architecture
 *
 * Enables Redux-like state management with modular slices:
 * - Each slice manages a portion of application state
 * - Slices define their own actions, reducers, and selectors
 * - Enables independent testing and composition
 * - Reduces coupling between different state domains
 *
 * Architecture:
 * - Each feature domain has a slice (Auth, Feed, User, Chat, etc.)
 * - Slices are composed into a root AppState
 * - Components subscribe to full state or specific slices
 * - Actions are dispatched to update slice state
 *
 * Benefits over monolithic state:
 * - Easier to understand (slice contains all related logic)
 * - Easier to test (slices can be tested independently)
 * - Better code organization (slice files stay focused)
 * - Incremental refactoring (can adopt slices gradually)
 * - Reusable patterns (same structure across slices)
 *
 * Usage:
 *   // Define slice state
 *   struct AuthSlice {
 *     bool isLoggedIn;
 *     String userId;
 *     String error;
 *   };
 *
 *   // Create action handler
 *   auto loginAction = [](AuthSlice& state, const LoginPayload& payload) {
 *     state.isLoggedIn = true;
 *     state.userId = payload.userId;
 *   };
 *
 *   // Subscribe to slice changes
 *   store.subscribeToSlice<AuthSlice>(
 *     [this](const AuthSlice& auth) { updateUI(auth); }
 *   );
 */
template <typename StateType> class StateSlice {
public:
  // Action type: function that modifies state
  using Action = std::function<void(StateType &)>;

  // Selector type: function that extracts derived state
  template <typename Derived> using Selector = std::function<Derived(const StateType &)>;

  // Subscription callback
  using SubscriptionCallback = std::function<void(const StateType &)>;

  virtual ~StateSlice() = default;

  /**
   * Get current state value
   */
  virtual const StateType &getState() const = 0;

  /**
   * Dispatch an action to modify state
   * Action receives mutable reference to state
   */
  virtual void dispatch(const Action &action) = 0;

  /**
   * Subscribe to state changes
   * Callback invoked whenever state changes
   */
  virtual void subscribe(const SubscriptionCallback &callback) = 0;

  /**
   * Subscribe to derived state
   * Only invoked when selected state changes
   *
   * @param selector Function that extracts derived state
   * @param callback Invoked when selected state differs from previous
   */
  template <typename Derived>
  void subscribeToSelection(const Selector<Derived> &selector, std::function<void(const Derived &)> callback) {
    // Store previous value for comparison
    auto prevValue = std::make_shared<std::optional<Derived>>();

    subscribe([selector, callback, prevValue](const StateType &state) {
      auto currentValue = selector(state);

      // Only call callback if selected state changed
      if (!*prevValue || *prevValue != currentValue) {
        *prevValue = currentValue;
        callback(currentValue);
      }
    });
  }
};

/**
 * InMemorySlice - Simple in-memory implementation of StateSlice
 *
 * Holds state in memory with simple action dispatch and subscriptions.
 * Thread-safe with mutex protection.
 */
template <typename StateType> class InMemorySlice : public StateSlice<StateType> {
public:
  explicit InMemorySlice(const StateType &initialState = StateType()) : state_(initialState) {}

  const StateType &getState() const override {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return state_;
  }

  void dispatch(const typename StateSlice<StateType>::Action &action) override {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    action(state_);
    lock.unlock();

    // Always notify subscribers on dispatch (Redux pattern)
    // Action handlers are responsible for only modifying state when needed
    {
      std::shared_lock<std::shared_mutex> readLock(stateMutex_);
      StateType currentState = state_;
      readLock.unlock();

      for (const auto &callback : subscribers_) {
        callback(currentState);
      }
    }
  }

  void subscribe(const typename StateSlice<StateType>::SubscriptionCallback &callback) override {
    std::unique_lock<std::shared_mutex> lock(subscribersMutex_);
    subscribers_.push_back(callback);

    // Immediately invoke callback with current state
    lock.unlock();
    callback(getState());
  }

private:
  StateType state_;
  mutable std::shared_mutex stateMutex_;

  std::vector<typename StateSlice<StateType>::SubscriptionCallback> subscribers_;
  std::shared_mutex subscribersMutex_;
};

} // namespace Stores
} // namespace Sidechain
