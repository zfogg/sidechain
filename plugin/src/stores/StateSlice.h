#pragma once

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <optional>
#include <mutex>
#include <vector>
#include <algorithm>

namespace Sidechain {
namespace Stores {

/**
 * ImmutableSlice - Truly immutable state management (Redux pattern)
 *
 * Core principle: State is NEVER mutated in-place. Only replaced with new instances.
 *
 * IMMUTABILITY GUARANTEE:
 * - currentState_ holds state by value (complete copies, not pointers)
 * - setState(newState) atomically replaces the entire state copy
 * - getState() returns const reference to current state
 * - Subscribers receive const references to immutable snapshots
 * - No shared mutable state between subscribers
 * - No in-place mutations possible - all changes create new state instances
 *
 * Architecture:
 * - setState(newState) copies StateType into currentState_ and notifies subscribers
 * - getState() returns const reference to immutable currentState_
 * - Each setState() call triggers all subscribers with const reference
 * - Memory is value-based: state instances are complete copies
 *
 * Pattern Flow:
 * 1. AppStore action creates new state: FollowersState newState(users, loading, ...)
 * 2. Action calls: slice->setState(newState)
 * 3. setState() atomically replaces currentState_ with new instance
 * 4. All subscribers notified with const reference to new immutable snapshot
 * 5. Subscribers render - guaranteed immutable
 *
 * Thread-safety:
 * - getState() returns const reference to current state (shared_lock)
 * - setState() replaces entire state copy (unique_lock)
 * - Subscribers notified outside lock to prevent deadlocks
 * - Each subscriber gets const reference to same immutable snapshot
 *
 * Memory model:
 * - State instances are value types - complete copies, not pointers
 * - All contained entities are const shared_ptr<const T> (immutable views)
 * - No shared mutable state between subscribers
 * - Efficient for small/medium state structures (collections of smart pointers)
 */
template <typename StateType> class ImmutableSlice {
public:
  // Subscription callback receives immutable state snapshot
  using SubscriptionCallback = std::function<void(const StateType &)>;

  // Unsubscriber function - call to remove subscription
  using Unsubscriber = std::function<void()>;

  explicit ImmutableSlice(const StateType &initialState = StateType()) : currentState_(initialState) {}

  virtual ~ImmutableSlice() = default;

  /**
   * Get current immutable state snapshot
   * Returns const reference to current state.
   * State is immutable - all changes create new instances via setState().
   * Thread-safe read.
   */
  const StateType &getState() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return currentState_;
  }

  /**
   * Set new immutable state (replaces entire state object)
   *
   * This is the ONLY way to update state - by creating a complete new instance.
   * Old state is replaced, triggering notification to all subscribers.
   *
   * Pattern:
   * ```cpp
   * // Create new immutable state instance
   * FollowersState newState(
   *   users,     // immutable user vector
   *   false,     // isLoading
   *   "",        // errorMessage
   *   users.size(),  // totalCount
   *   userId,    // targetUserId
   *   FollowersState::Followers);
   *
   * // Replace entire state - triggers all notifications
   * followersSlice->setState(newState);
   * ```
   *
   * Thread-safe write. Subscribers notified outside lock.
   *
   * @param newState New complete immutable state instance
   */
  void setState(const StateType &newState) {
    std::vector<SubscriptionCallback> callbacksCopy;
    {
      std::unique_lock<std::shared_mutex> lock(stateMutex_);
      currentState_ = newState; // Atomic copy replacement with new instance

      // Copy callbacks to avoid holding lock during notification
      std::shared_lock<std::shared_mutex> callbackLock(subscribersMutex_);
      for (const auto &[id, callback] : subscribers_) {
        callbacksCopy.push_back(callback);
      }
      callbackLock.unlock();
    }

    // Notify all subscribers outside lock
    // Each subscriber gets called with the new immutable state
    for (const auto &callback : callbacksCopy) {
      callback(currentState_);
    }
  }

  /**
   * Dispatch an action (compatibility wrapper for setState)
   *
   * This maintains backward compatibility with the dispatch pattern while
   * preserving immutability. The action receives a COPY of current state,
   * modifies it, and setState() replaces the entire state object.
   *
   * Pattern:
   * ```cpp
   * slice->dispatch([](StateType& state) {
   *   state.field = newValue;  // Modify the copy
   * });
   * // The copy becomes the new state; old state is not touched
   * ```
   *
   * @param action Function that modifies a copy of the current state
   */
  void dispatch(const std::function<void(StateType &)> &action) {
    StateType newState;
    {
      std::shared_lock<std::shared_mutex> readLock(stateMutex_);
      newState = currentState_; // Copy current state
    }

    // Apply action to the copy (not the original)
    action(newState);

    // Replace state with the modified copy
    setState(newState);
  }

  /**
   * Subscribe to state changes
   *
   * Callback receives new immutable state snapshot whenever setState() is called.
   * Callback is invoked immediately with current state.
   * Returns unsubscriber function to cleanup subscription.
   *
   * Pattern:
   * ```cpp
   * auto unsub = discoverySlice->subscribe([this](const DiscoveryState& state) {
   *   // state is immutable snapshot - safe to use
   *   renderTrendingUsers(state.trendingUsers);
   * });
   *
   * // Later: cleanup
   * unsub();
   * ```
   *
   * Thread-safe. Callback never called while setState() is active.
   *
   * @param callback Function called with new state snapshots
   * @return Unsubscriber function - call to remove subscription
   */
  Unsubscriber subscribe(const SubscriptionCallback &callback) {
    int subscriptionId;
    {
      std::unique_lock<std::shared_mutex> lock(subscribersMutex_);
      subscriptionId = nextSubscriberId_++;
      subscribers_.push_back({subscriptionId, callback});
    }

    // Call immediately with current state
    callback(getState());

    // Return unsubscriber function identified by ID
    return [this, subscriptionId]() {
      std::unique_lock<std::shared_mutex> lock(subscribersMutex_);
      auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
                             [subscriptionId](const auto &pair) { return pair.first == subscriptionId; });
      if (it != subscribers_.end()) {
        subscribers_.erase(it);
      }
    };
  }

  /**
   * Subscribe to derived state (selector pattern)
   *
   * Only notified when selected state changes.
   * Useful for optimizing re-renders - subscribe to specific fields.
   *
   * Pattern:
   * ```cpp
   * // Only re-render when trending users change, not entire state
   * auto unsub = discoverySlice->subscribeToSelection(
   *   [](const DiscoveryState& s) { return s.trendingUsers; },  // selector
   *   [this](const auto& users) { renderTrending(users); }      // callback
   * );
   * ```
   *
   * @param selector Function that extracts derived state from full state
   * @param callback Invoked only when selected state differs from previous
   * @return Unsubscriber function
   */
  template <typename Derived>
  Unsubscriber subscribeToSelection(std::function<Derived(const StateType &)> selector,
                                    std::function<void(const Derived &)> callback) {

    auto prevValue = std::make_shared<std::optional<Derived>>();

    return subscribe([selector, callback, prevValue](const StateType &state) {
      auto currentValue = selector(state);

      // Only call callback if selected state changed
      if (!*prevValue || *prevValue != currentValue) {
        *prevValue = currentValue;
        callback(currentValue);
      }
    });
  }

private:
  // Immutable state stored by value
  // - setState() atomically replaces with new copy
  // - getState() returns const reference to current value
  // - Subscribers hold const references to immutable snapshots
  StateType currentState_;
  mutable std::shared_mutex stateMutex_;

  // Use indexed subscribers to avoid comparing std::function (which has deleted operator==)
  std::vector<std::pair<int, SubscriptionCallback>> subscribers_; // {id, callback}
  int nextSubscriberId_ = 0;
  std::shared_mutex subscribersMutex_;
};

} // namespace Stores
} // namespace Sidechain
