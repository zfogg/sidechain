#pragma once

#include <vector>
#include <functional>
#include <memory>

namespace Sidechain {
namespace Util {

/**
 * SubscriptionBag - RAII container for managing subscriptions
 *
 * Automatically unsubscribes from all subscriptions when destroyed.
 * Solves the problem of components subscribing to stores and forgetting to unsubscribe.
 *
 * Issues solved:
 * 1. Memory leaks: Subscription callbacks keep component alive even after destruction
 * 2. Use-after-free: Store invokes callback on destroyed component
 * 3. Dangling observers: Observer list grows unbounded with destroyed components
 *
 * Usage:
 * ```cpp
 * class MyComponent : public juce::Component {
 * private:
 *     SubscriptionBag subscriptions;
 *
 * public:
 *     void initialize(ReactiveStore<FeedState>& feedStore) {
 *         // Subscribe automatically - unsubscribe on component destruction
 *         subscriptions += feedStore.subscribe([this](const FeedState& state) {
 *             updateUI(state);
 *         });
 *
 *         subscriptions += feedStore.subscribeToSelection(
 *             [](const FeedState& s) { return s.posts; },
 *             [this](const auto& posts) { renderPosts(posts); }
 *         );
 *     }
 *     // ~MyComponent() - SubscriptionBag destructor auto-unsubscribes
 * };
 * ```
 *
 * Pattern for stores:
 * ```cpp
 * template <typename T>
 * class ReactiveStore {
 * private:
 *     std::vector<std::function<void(const T&)>> subscribers;
 *
 * public:
 *     Subscription subscribe(std::function<void(const T&)> callback) {
 *         size_t index = subscribers.size();
 *         subscribers.push_back(callback);
 *
 *         // Return unsubscribe function
 *         return Subscription([this, index]() {
 *             if (index < subscribers.size()) {
 *                 subscribers[index] = nullptr;  // Null out instead of erase
 *             }
 *         });
 *     }
 * };
 * ```
 */
class SubscriptionBag {
public:
  using Subscription = std::function<void()>;

  SubscriptionBag() = default;

  // Prevent copying (each component gets its own bag)
  SubscriptionBag(const SubscriptionBag &) = delete;
  SubscriptionBag &operator=(const SubscriptionBag &) = delete;

  // Allow moving for component reparenting
  SubscriptionBag(SubscriptionBag &&other) noexcept : subscriptions_(std::move(other.subscriptions_)) {}
  SubscriptionBag &operator=(SubscriptionBag &&other) noexcept {
    if (this != &other) {
      unsubscribeAll();
      subscriptions_ = std::move(other.subscriptions_);
    }
    return *this;
  }

  // Destructor - cleanup all subscriptions
  ~SubscriptionBag() {
    unsubscribeAll();
  }

  /**
   * Add a subscription (via += operator)
   * Usage: subscriptions += store.subscribe(...);
   */
  SubscriptionBag &operator+=(Subscription sub) {
    if (sub) {
      subscriptions_.push_back(sub);
    }
    return *this;
  }

  /**
   * Manual add
   */
  void add(Subscription sub) {
    if (sub) {
      subscriptions_.push_back(sub);
    }
  }

  /**
   * Unsubscribe all subscriptions
   * Automatically called on destruction
   */
  void unsubscribeAll() {
    for (auto &sub : subscriptions_) {
      if (sub) {
        sub();
      }
    }
    subscriptions_.clear();
  }

  /**
   * Clear subscriptions without unsubscribing
   * Use with care - prefer unsubscribeAll()
   */
  void clear() {
    subscriptions_.clear();
  }

  /**
   * Get number of subscriptions
   */
  size_t size() const {
    return subscriptions_.size();
  }

  /**
   * Check if bag is empty
   */
  bool isEmpty() const {
    return subscriptions_.empty();
  }

private:
  std::vector<Subscription> subscriptions_;
};

} // namespace Util
} // namespace Sidechain
