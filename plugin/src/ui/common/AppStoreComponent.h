#pragma once

#include "../../stores/AppStore.h"
#include "../../util/Log.h"
#include <JuceHeader.h>
#include <functional>

namespace Sidechain {
namespace UI {

/**
 * AppStoreComponent - Base class for components with automatic AppStore binding
 *
 * Eliminates boilerplate by accepting a subscription lambda in the constructor.
 * No virtual method overrides needed - subscription is set up automatically.
 *
 * Features:
 * - Automatic subscription setup (no initialize() call needed)
 * - No virtual method overrides required
 * - Thread-safe state callbacks via MessageManager
 * - RAII cleanup in destructor
 * - Runtime binding/unbinding support
 *
 * Usage (Before - 9 lines of boilerplate per component):
 *   class MyComponent : public AppStoreComponent<ChallengeState> {
 *   public:
 *     explicit MyComponent(AppStore *store = nullptr) : AppStoreComponent(store) {
 *       setupUI();
 *       initialize();  // Manual call needed
 *     }
 *   protected:
 *     void onAppStateChanged(const ChallengeState &state) override { ... }
 *     void subscribeToAppStore() override {
 *       juce::Component::SafePointer<MyComponent> safeThis(this);
 *       storeUnsubscriber = appStore->subscribeToChallenges(
 *         [safeThis](const auto& state) { ... });
 *     }
 *   };
 *
 * Usage (After - 1 line, no boilerplate):
 *   class MyComponent : public AppStoreComponent<ChallengeState> {
 *   public:
 *     explicit MyComponent(AppStore *store = nullptr)
 *         : AppStoreComponent(store, [store](auto cb) {
 *             return store->subscribeToChallenges(cb);
 *           }) {}
 *
 *   protected:
 *     void onAppStateChanged(const ChallengeState &state) override { ... }
 *   };
 */
template <typename StateType> class AppStoreComponent : public juce::Component {
public:
  /**
   * Type for subscription function
   * Takes a callback and returns an unsubscriber function
   */
  using SubscriptionFn = std::function<std::function<void()>(std::function<void(const StateType &)>)>;

  /**
   * Constructor with optional AppStore and subscription function
   *
   * The subscription function is called immediately to set up the store binding.
   * No need to call initialize() - subscription happens automatically.
   *
   * @param store Pointer to AppStore (can be nullptr)
   * @param subscriptionFn Lambda that takes a callback and returns unsubscriber
   *
   * Example:
   *   AppStoreComponent(store, [store](auto cb) {
   *     return store->subscribeToChallenges(cb);
   *   })
   */
  explicit AppStoreComponent(Stores::AppStore *store = nullptr, SubscriptionFn subscriptionFn = nullptr)
      : appStore(store), userSubscriptionFn(subscriptionFn) {
    if (appStore && userSubscriptionFn) {
      setupSubscription();
    }
  }

  /**
   * Destructor - automatically unsubscribes from store
   */
  ~AppStoreComponent() override {
    unsubscribeFromAppStore();
  }

  /**
   * Bind to a different store at runtime
   * Useful for components that are created before store is available
   */
  void bindToStore(Stores::AppStore *store, SubscriptionFn subscriptionFn = nullptr) {
    unsubscribeFromAppStore();
    appStore = store;

    if (subscriptionFn) {
      userSubscriptionFn = subscriptionFn;
    }

    if (appStore && userSubscriptionFn) {
      setupSubscription();
    }
  }

  /**
   * Unbind from store
   */
  void unbindFromStore() {
    unsubscribeFromAppStore();
    appStore = nullptr;
  }

  /**
   * Get the current store
   */
  Stores::AppStore *getAppStore() const {
    return appStore;
  }

protected:
  Stores::AppStore *appStore = nullptr;
  SubscriptionFn userSubscriptionFn;
  std::function<void()> storeUnsubscriber;

  /**
   * Called when app state changes
   * Subclasses override this to react to state updates
   * Called on message thread (safe for JUCE operations)
   */
  virtual void onAppStateChanged(const StateType &state) = 0;

private:
  /**
   * Set up subscription using the provided subscription function
   * Automatically wraps callback with SafePointer and MessageManager
   */
  void setupSubscription() {
    if (!appStore || !userSubscriptionFn) {
      return;
    }

    // Create a safe pointer to this component
    juce::Component::SafePointer<AppStoreComponent> safeThis(this);

    // Call the user's subscription function with our wrapped callback
    storeUnsubscriber = userSubscriptionFn([safeThis](const StateType &state) {
      if (!safeThis)
        return;

      // Schedule UI update on message thread for thread safety
      juce::MessageManager::callAsync([safeThis, state]() {
        if (!safeThis)
          return;
        safeThis->onAppStateChanged(state);
      });
    });
  }

  /**
   * Unsubscribe from app store
   */
  void unsubscribeFromAppStore() {
    if (storeUnsubscriber) {
      storeUnsubscriber();
      storeUnsubscriber = nullptr;
    }
  }
};

} // namespace UI
} // namespace Sidechain
