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
 * Eliminates boilerplate subscribe/unsubscribe code by:
 * - Accepting AppStore in constructor
 * - Automatically subscribing in constructor
 * - Automatically unsubscribing in destructor
 * - Providing thread-safe state callbacks via MessageManager
 *
 * Subclasses must:
 * 1. Inherit from AppStoreComponent<StateType>
 * 2. Override subscribeToAppStore() with their specific subscription
 * 3. Override onAppStateChanged() to handle state updates
 *
 * Usage:
 *   class MyComponent : public AppStoreComponent<ChallengeState> {
 *   public:
 *     MyComponent(AppStore *store = nullptr) : AppStoreComponent(store) {}
 *
 *   protected:
 *     void onAppStateChanged(const ChallengeState &state) override {
 *       // Handle state changes
 *     }
 *
 *     void subscribeToAppStore() override {
 *       juce::Component::SafePointer<MyComponent> safeThis(this);
 *       storeUnsubscriber = appStore->subscribeToChallenges(
 *         [safeThis](const ChallengeState &state) {
 *           if (!safeThis) return;
 *           juce::MessageManager::callAsync([safeThis, state]() {
 *             if (safeThis)
 *               safeThis->onAppStateChanged(state);
 *           });
 *         });
 *     }
 *   };
 */
template <typename StateType> class AppStoreComponent : public juce::Component {
public:
  /**
   * Constructor with optional AppStore binding
   * If store is provided, subscribes immediately
   */
  explicit AppStoreComponent(Stores::AppStore *store = nullptr) : appStore(store) {
    if (appStore) {
      subscribeToAppStore();
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
  void bindToStore(Stores::AppStore *store) {
    if (storeUnsubscriber) {
      storeUnsubscriber();
      storeUnsubscriber = nullptr;
    }

    appStore = store;
    if (appStore) {
      subscribeToAppStore();
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
  std::function<void()> storeUnsubscriber;

  /**
   * Called when app state changes
   * Subclasses override this to react to state updates
   * Called on message thread (safe for JUCE operations)
   */
  virtual void onAppStateChanged(const StateType &state) = 0;

  /**
   * Subscribe to app store changes
   * Subclasses override this to set up their subscription
   * Must set storeUnsubscriber callback
   */
  virtual void subscribeToAppStore() = 0;

  /**
   * Unsubscribe from app store
   * Override if you need custom cleanup
   */
  virtual void unsubscribeFromAppStore() {
    if (storeUnsubscriber) {
      storeUnsubscriber();
      storeUnsubscriber = nullptr;
    }
  }
};

} // namespace UI
} // namespace Sidechain
