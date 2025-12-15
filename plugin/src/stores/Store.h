#pragma once

#include "../util/logging/Logger.h"
#include "../util/reactive/ObservableCollection.h"
#include "../util/reactive/ObservableProperty.h"
#include <JuceHeader.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <typeindex>
#include <vector>

namespace Sidechain {
namespace Stores {

/**
 * Store<State> - Base class for reactive state management stores
 *
 * Provides a Redux-like pattern for managing application state:
 * - Single source of truth for a specific domain's state
 * - Observable state changes via subscriptions
 * - Thread-safe state access and mutations
 * - Optimistic updates with rollback support
 * - Error recovery and notification
 *
 * Usage:
 *   // Define state struct
 *   struct FeedState {
 *       juce::Array<FeedPost> posts;
 *       bool isLoading = false;
 *       juce::String error;
 *       int64_t lastUpdated = 0;
 *   };
 *
 *   // Create store
 *   class FeedStore : public Store<FeedState> {
 *   public:
 *       void loadFeed() {
 *           updateState([](FeedState& state) {
 *               state.isLoading = true;
 *           });
 *           // ... fetch data ...
 *       }
 *   };
 *
 *   // Subscribe to changes
 *   feedStore.subscribe([](const FeedState& state) {
 *       if (!state.isLoading) {
 *           displayPosts(state.posts);
 *       }
 *   });
 *
 * @tparam State The state type (must be copyable)
 */
template <typename State> class Store {
public:
  using StateType = State;
  using Observer = std::function<void(const State &)>;
  using Unsubscriber = std::function<void()>;
  using StateUpdater = std::function<void(State &)>;

  /**
   * Construct store with initial state
   * @param initialState The initial state value
   */
  explicit Store(const State &initialState = State()) : currentState(initialState) {}

  virtual ~Store() = default;

  /**
   * Get the current state (thread-safe read)
   * @return Copy of current state
   */
  State getState() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentState;
  }

  /**
   * Subscribe to state changes
   * @param observer Callback invoked when state changes
   * @return Unsubscriber function - call to unsubscribe
   *
   * Observer is called immediately with current state, then on every change.
   */
  Unsubscriber subscribe(Observer observer) {
    if (!observer)
      return []() {};

    auto observerId = nextObserverId++;

    {
      std::lock_guard<std::mutex> lock(observerMutex);
      observers[observerId] = observer;
    }

    // Notify immediately with current state
    observer(getState());

    // Return unsubscriber
    return [this, observerId]() {
      std::lock_guard<std::mutex> lock(this->observerMutex);
      this->observers.erase(observerId);
    };
  }

  /**
   * Get number of active subscribers
   */
  size_t getSubscriberCount() const {
    std::lock_guard<std::mutex> lock(observerMutex);
    return observers.size();
  }

  /**
   * Check if store has any subscribers
   */
  bool hasSubscribers() const {
    return getSubscriberCount() > 0;
  }

protected:
  /**
   * Update state and notify observers
   * @param updater Function that mutates state
   *
   * Thread-safe. Observers are notified outside the lock.
   */
  void updateState(StateUpdater updater) {
    if (!updater)
      return;

    State newState;
    std::vector<Observer> observersToNotify;

    {
      std::lock_guard<std::mutex> lock(stateMutex);
      newState = currentState;
      updater(newState);

      // Only update and notify if state actually changed
      // Note: This requires State to have operator==
      // If not defined, always update
      if constexpr (requires { newState == currentState; }) {
        if (newState == currentState)
          return;
      }

      currentState = newState;
    }

    // Collect observers outside state lock to avoid deadlock
    {
      std::lock_guard<std::mutex> lock(observerMutex);
      for (const auto &[id, observer] : observers)
        observersToNotify.push_back(observer);
    }

    // Notify observers outside all locks
    for (auto &observer : observersToNotify) {
      try {
        observer(newState);
      } catch (const std::exception &e) {
        Util::logError("Store", "Observer threw exception", e.what());
      }
    }
  }

  /**
   * Set state directly (replaces entire state)
   * @param newState The new state value
   */
  void setState(const State &newState) {
    updateState([&newState](State &state) { state = newState; });
  }

  /**
   * Perform an optimistic update with rollback on error
   * @param optimisticUpdate Function to apply optimistically
   * @param asyncOperation Async operation to perform (returns success/failure)
   * @param onError Optional error handler
   *
   * Pattern:
   *   1. Apply optimisticUpdate immediately
   *   2. Execute asyncOperation
   *   3. If operation fails, rollback to previous state
   */
  template <typename AsyncOp>
  void optimisticUpdate(StateUpdater optimisticUpdate, AsyncOp asyncOperation,
                        std::function<void(const juce::String &)> onError = nullptr) {
    // Save current state for rollback
    State previousState = getState();

    // Apply optimistic update
    updateState(optimisticUpdate);

    // Execute async operation
    asyncOperation([this, previousState, onError](bool success, const juce::String &error) {
      if (!success) {
        // Rollback on error
        setState(previousState);

        if (onError)
          onError(error);
      }
    });
  }

  /**
   * Notify observers without changing state
   * Useful when internal data changes that isn't tracked in state
   */
  void notifyObservers() {
    State currentStateCopy = getState();
    std::vector<Observer> observersToNotify;

    {
      std::lock_guard<std::mutex> lock(observerMutex);
      for (const auto &[id, observer] : observers)
        observersToNotify.push_back(observer);
    }

    for (auto &observer : observersToNotify) {
      try {
        observer(currentStateCopy);
      } catch (const std::exception &e) {
        Util::logError("Store", "Observer threw exception", e.what());
      }
    }
  }

private:
  State currentState;
  mutable std::mutex stateMutex;
  mutable std::mutex observerMutex;

  std::map<uint64_t, Observer> observers;
  std::atomic<uint64_t> nextObserverId{1};

  // Deleted copy/move to ensure single instance
  Store(const Store &) = delete;
  Store &operator=(const Store &) = delete;
  Store(Store &&) = delete;
  Store &operator=(Store &&) = delete;
};

/**
 * ScopedSubscription - RAII wrapper for store subscriptions
 *
 * Automatically unsubscribes when destroyed, perfect for component lifecycles.
 *
 * Usage:
 *   class MyComponent : public juce::Component {
 *       ScopedSubscription feedSubscription;
 *
 *       void init() {
 *           feedSubscription = feedStore.subscribe([this](const auto& state) {
 *               // Update UI
 *           });
 *       }
 *   };  // Automatically unsubscribes when component is destroyed
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
 *   class MyComponent {
 *       SubscriptionBag subscriptions;
 *
 *       void init() {
 *           subscriptions.add(feedStore.subscribe(...));
 *           subscriptions.add(userStore.subscribe(...));
 *       }
 *   };  // All subscriptions cleaned up when component destroyed
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
