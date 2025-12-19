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
 * - Providing explicit initialize() for safe virtual method calls after construction
 * - Automatically unsubscribing in destructor
 * - Providing thread-safe state callbacks via MessageManager
 *
 * Subclasses must:
 * 1. Inherit from AppStoreComponent<StateType>
 * 2. Override subscribeToAppStore() with their specific subscription
 * 3. Override onAppStateChanged() to handle state updates
 * 4. Call initialize() after construction is complete
 *
 * Usage:
 *   class MyComponent : public AppStoreComponent<ChallengeState> {
 *   public:
 *     explicit MyComponent(AppStore *store = nullptr) : AppStoreComponent(store) {
 *       // All initialization here before calling initialize()
 *       initialize();  // Safe to call virtual methods now
 *     }
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
   * Does NOT subscribe automatically - call initialize() after construction
   */
  explicit AppStoreComponent(Stores::AppStore *store = nullptr) : appStore(store), isInitialized(false) {}

  /**
   * Initialize store subscription after construction
   * Must be called exactly once after derived class construction is complete
   * Safe to call virtual methods (subscribeToAppStore) at this point
   */
  void initialize() {
    if (isInitialized) {
      Log::error("AppStoreComponent::initialize() called more than once");
      return;
    }

    isInitialized = true;
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
    if (appStore && isInitialized) {
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
  bool isInitialized;

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
