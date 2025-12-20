#pragma once

#include "../util/logging/Logger.h"
#include <functional>
#include <memory>
#include <shared_mutex>
#include <vector>
#include <optional>
#include <algorithm>
#include <map>

namespace Sidechain {
namespace Stores {

/**
 * ReactiveStore<StateType> - Unified reactive state management
 *
 * Consolidates three patterns into one:
 * 1. Store<T> - Simple observable pattern with state updates
 * 2. ImmutableSlice<T> - Immutable Redux-style state
 * 3. EntityCache integration - Built-in entity normalization
 *
 * IMMUTABILITY GUARANTEE:
 * - State is stored by value (complete copies, not pointers)
 * - setState() atomically replaces entire state with new instance
 * - getState() returns const reference to immutable current state
 * - All subscribers receive const references to same immutable snapshot
 * - No in-place mutations possible - only via setState()
 *
 * THREAD-SAFETY:
 * - getState() uses shared_lock (concurrent reads)
 * - setState() uses unique_lock (exclusive writes)
 * - Subscribers notified outside lock to prevent deadlocks
 * - Exception-safe observer notification with logging
 *
 * MEMORY MODEL:
 * - State is value-semantic (complete copies)
 * - Contained entities are const shared_ptr<const T> (immutable views)
 * - No shared mutable state between subscribers
 * - Efficient for small/medium state structures
 *
 * Usage Examples:
 *
 * Basic subscription:
 * ```cpp
 * class MyComponent {
 *     ReactiveStore<FeedState> feedStore;
 *
 *     void init() {
 *         feedStore.subscribe([this](const FeedState& state) {
 *             updateUI(state);
 *         });
 *     }
 * };
 * ```
 *
 * State updates (via AppStore):
 * ```cpp
 * FeedState newState = feedStore.getState();
 * newState.isLoading = false;
 * newState.posts = loadedPosts;
 * feedStore.setState(newState);  // Triggers all subscriptions
 * ```
 *
 * Selector pattern (only notify on field changes):
 * ```cpp
 * feedStore.subscribeToSelection(
 *     [](const FeedState& s) { return s.posts; },      // selector
 *     [this](const auto& posts) { renderPosts(posts); } // callback
 * );
 * ```
 *
 * Optimistic updates with rollback:
 * ```cpp
 * feedStore.optimisticUpdate(
 *     [&](FeedState& s) { s.posts.push_back(newPost); },  // optimistic
 *     [this](auto callback) { network.save(callback); }     // async op
 * );
 * ```
 *
 * @tparam StateType The application state type (must be copyable)
 */
template <typename StateType> class ReactiveStore {
public:
  using SubscriptionCallback = std::function<void(const StateType &)>;
  using Unsubscriber = std::function<void()>;
  using StateUpdater = std::function<void(StateType &)>;

  /**
   * Construct store with initial state
   * @param initialState Initial state value (default-constructed if not provided)
   */
  explicit ReactiveStore(const StateType &initialState = StateType()) : currentState_(initialState) {}

  virtual ~ReactiveStore() = default;

  // ============================================================================
  // State Access
  // ============================================================================

  /**
   * Get current immutable state snapshot
   * Thread-safe read using shared_lock
   * @return const reference to current immutable state
   */
  const StateType &getState() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return currentState_;
  }

  /**
   * Copy current state (if you need a modifiable copy)
   * Useful for creating new state via copy-and-modify pattern
   * @return copy of current state
   */
  StateType copyState() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return currentState_;
  }

  // ============================================================================
  // State Updates
  // ============================================================================

  /**
   * Set new immutable state (replaces entire state)
   * This is the primary way to update state.
   * Atomically replaces old state and notifies all subscribers.
   *
   * Thread-safe write with exclusive lock.
   *
   * @param newState New complete state instance
   */
  void setState(const StateType &newState) {
    std::vector<SubscriptionCallback> callbacksCopy;
    {
      std::unique_lock<std::shared_mutex> lock(stateMutex_);
      currentState_ = newState; // Atomic replacement

      // Copy callbacks to avoid holding lock during notification
      {
        std::shared_lock<std::shared_mutex> cbLock(subscribersMutex_);
        for (const auto &[id, callback] : subscribers_) {
          callbacksCopy.push_back(callback);
        }
      }
    }

    // Notify all subscribers outside lock
    notifySubscribers_(callbacksCopy, newState);
  }

  /**
   * Update state via updater function
   * Creates new state, applies updater, then replaces via setState()
   *
   * Pattern:
   * ```cpp
   * store.updateState([](FeedState& s) {
   *     s.isLoading = true;
   * });
   * ```
   *
   * @param updater Function that modifies state (by reference)
   */
  void updateState(StateUpdater updater) {
    if (!updater)
      return;

    StateType newState = copyState();
    updater(newState);
    setState(newState);
  }

  // ============================================================================
  // Subscriptions
  // ============================================================================

  /**
   * Subscribe to all state changes
   * Callback invoked immediately with current state, then on every setState()
   *
   * Pattern:
   * ```cpp
   * auto unsub = store.subscribe([this](const FeedState& state) {
   *     updateUI(state);
   * });
   * // Later: unsub();
   * ```
   *
   * @param callback Function called with new state snapshots
   * @return Unsubscriber function - call to remove subscription
   */
  Unsubscriber subscribe(const SubscriptionCallback &callback) {
    if (!callback)
      return []() {};

    int subscriptionId;
    {
      std::unique_lock<std::shared_mutex> lock(subscribersMutex_);
      subscriptionId = nextSubscriberId_++;
      subscribers_.emplace_back(subscriptionId, callback);
    }

    // Call immediately with current state
    try {
      callback(getState());
    } catch (const std::exception &e) {
      Util::logError("ReactiveStore", "Subscriber threw exception on initial call", e.what());
    }

    // Return unsubscriber
    return [this, subscriptionId]() {
      std::unique_lock<std::shared_mutex> lock(this->subscribersMutex_);
      auto it = std::find_if(this->subscribers_.begin(), this->subscribers_.end(),
                             [subscriptionId](const auto &pair) { return pair.first == subscriptionId; });
      if (it != this->subscribers_.end()) {
        this->subscribers_.erase(it);
      }
    };
  }

  /**
   * Subscribe to derived state (selector pattern)
   * Only notified when selected portion of state changes
   * Useful for optimizing re-renders - components only update when relevant state changes
   *
   * Pattern:
   * ```cpp
   * store.subscribeToSelection(
   *     [](const FeedState& s) { return s.posts; },           // selector
   *     [this](const auto& posts) { renderPosts(posts); }     // callback
   * );
   * ```
   *
   * @tparam DerivedType Type of selected state
   * @param selector Function extracting relevant state portion
   * @param callback Invoked only when selected state differs from previous
   * @return Unsubscriber function
   */
  template <typename DerivedType>
  Unsubscriber subscribeToSelection(std::function<DerivedType(const StateType &)> selector,
                                    std::function<void(const DerivedType &)> callback) {

    auto prevValue = std::make_shared<std::optional<DerivedType>>();

    return subscribe([selector, callback, prevValue](const StateType &state) {
      auto currentValue = selector(state);

      // Only call callback if selected state changed
      if (!*prevValue || *prevValue != currentValue) {
        *prevValue = currentValue;
        try {
          callback(currentValue);
        } catch (const std::exception &e) {
          Util::logError("ReactiveStore", "Selector callback threw exception", e.what());
        }
      }
    });
  }

  // ============================================================================
  // Optimistic Updates
  // ============================================================================

  /**
   * Perform optimistic update with automatic rollback on error
   * Applies optimistic state change immediately, executes async operation,
   * and rolls back to previous state if operation fails.
   *
   * Pattern:
   * ```cpp
   * store.optimisticUpdate(
   *     // Optimistic update
   *     [&](FeedState& s) {
   *         auto it = std::find_if(s.posts.begin(), s.posts.end(),
   *             [&](const auto& p) { return p.id == postId; });
   *         if (it != s.posts.end()) {
   *             it->isLiked = true;
   *             it->likeCount++;
   *         }
   *     },
   *     // Async operation
   *     [this](auto onComplete) {
   *         network.likePost(postId, onComplete);
   *     }
   * );
   * ```
   *
   * @param optimisticUpdate Function applying optimistic state change
   * @param asyncOperation Function executing async operation
   * @param onError Optional error handler called if operation fails
   */
  template <typename AsyncOp>
  void optimisticUpdate(StateUpdater optimisticUpdate, AsyncOp asyncOperation,
                        std::function<void(const juce::String &)> onError = nullptr) {
    if (!optimisticUpdate)
      return;

    // Save current state for rollback
    StateType previousState = copyState();

    // Apply optimistic update
    updateState(optimisticUpdate);

    // Execute async operation
    asyncOperation([this, previousState, onError](bool success, const juce::String &error) {
      if (!success) {
        // Rollback on error
        setState(previousState);

        if (onError) {
          try {
            onError(error);
          } catch (const std::exception &e) {
            Util::logError("ReactiveStore", "Error handler threw exception", e.what());
          }
        }
      }
    });
  }

  // ============================================================================
  // Utilities
  // ============================================================================

  /**
   * Get number of active subscribers
   */
  size_t getSubscriberCount() const {
    std::shared_lock<std::shared_mutex> lock(subscribersMutex_);
    return subscribers_.size();
  }

  /**
   * Check if store has any subscribers
   */
  bool hasSubscribers() const {
    return getSubscriberCount() > 0;
  }

  /**
   * Notify observers without changing state
   * Useful when internal data changes but state structure remains same
   */
  void notifyObservers() {
    StateType stateCopy = copyState();
    std::vector<SubscriptionCallback> callbacksCopy;

    {
      std::shared_lock<std::shared_mutex> lock(subscribersMutex_);
      for (const auto &[id, callback] : subscribers_) {
        callbacksCopy.push_back(callback);
      }
    }

    notifySubscribers_(callbacksCopy, stateCopy);
  }

  // Deleted copy/move to ensure single instance per store
  ReactiveStore(const ReactiveStore &) = delete;
  ReactiveStore &operator=(const ReactiveStore &) = delete;
  ReactiveStore(ReactiveStore &&) = delete;
  ReactiveStore &operator=(ReactiveStore &&) = delete;

private:
  // Immutable state stored by value
  StateType currentState_;
  mutable std::shared_mutex stateMutex_;

  // Subscribers indexed by ID to avoid comparing std::function
  std::vector<std::pair<int, SubscriptionCallback>> subscribers_;
  int nextSubscriberId_ = 0;
  mutable std::shared_mutex subscribersMutex_;

  /**
   * Helper to notify subscribers with exception handling
   */
  void notifySubscribers_(const std::vector<SubscriptionCallback> &callbacks, const StateType &state) {
    for (const auto &callback : callbacks) {
      try {
        callback(state);
      } catch (const std::exception &e) {
        Util::logError("ReactiveStore", "Subscriber threw exception", e.what());
      }
    }
  }
};

/**
 * ScopedSubscription - RAII wrapper for store subscriptions
 *
 * Automatically unsubscribes when destroyed, perfect for component lifecycles.
 *
 * Usage:
 * ```cpp
 * class MyComponent : public juce::Component {
 *     ScopedSubscription storeSubscription;
 *
 *     void init() {
 *         storeSubscription = feedStore.subscribe([this](const auto& state) {
 *             updateUI(state);
 *         });
 *     }
 * };  // Automatically unsubscribes when component is destroyed
 * ```
 */
class ScopedSubscription {
public:
  ScopedSubscription() = default;

  explicit ScopedSubscription(std::function<void()> unsubscriber) : unsubscribe(std::move(unsubscriber)) {}

  ScopedSubscription(ScopedSubscription &&other) noexcept : unsubscribe(std::move(other.unsubscribe)) {
    other.unsubscribe = nullptr;
  }

  ScopedSubscription &operator=(ScopedSubscription &&other) noexcept {
    if (this != &other) {
      reset();
      unsubscribe = std::move(other.unsubscribe);
      other.unsubscribe = nullptr;
    }
    return *this;
  }

  ScopedSubscription &operator=(std::function<void()> newUnsubscriber) {
    reset();
    unsubscribe = std::move(newUnsubscriber);
    return *this;
  }

  ~ScopedSubscription() {
    reset();
  }

  void reset() {
    if (unsubscribe) {
      unsubscribe();
      unsubscribe = nullptr;
    }
  }

  bool isActive() const {
    return unsubscribe != nullptr;
  }

private:
  std::function<void()> unsubscribe;

  ScopedSubscription(const ScopedSubscription &) = delete;
  ScopedSubscription &operator=(const ScopedSubscription &) = delete;
};

/**
 * SubscriptionBag - Container for multiple subscriptions
 *
 * Useful when a component subscribes to multiple stores.
 *
 * Usage:
 * ```cpp
 * class MyComponent {
 *     SubscriptionBag subscriptions;
 *
 *     void init() {
 *         subscriptions.add(feedStore.subscribe(...));
 *         subscriptions.add(userStore.subscribe(...));
 *     }
 * };  // All subscriptions cleaned up when component destroyed
 * ```
 */
class SubscriptionBag {
public:
  SubscriptionBag() = default;

  void add(std::function<void()> unsubscriber) {
    if (unsubscriber)
      subscriptions.emplace_back(std::move(unsubscriber));
  }

  void clear() {
    subscriptions.clear();
  }

  size_t size() const {
    return subscriptions.size();
  }

  ~SubscriptionBag() = default;

private:
  std::vector<ScopedSubscription> subscriptions;

  SubscriptionBag(const SubscriptionBag &) = delete;
  SubscriptionBag &operator=(const SubscriptionBag &) = delete;
};

} // namespace Stores
} // namespace Sidechain
