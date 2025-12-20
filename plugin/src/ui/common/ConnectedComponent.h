#pragma once

#include "../../stores/RootStore.h"
#include "../../util/Log.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>

namespace Sidechain {
namespace UI {

/**
 * ConnectedComponent<StateType> - Simplified store binding for components
 *
 * Replaces the complex AppStoreComponent pattern with a cleaner approach:
 * - No need to override subscribeToAppStore()
 * - No need to override onAppStateChanged()
 * - Constructor handles all subscription setup
 * - Automatic unsubscription in destructor
 *
 * Usage:
 *   class MyComponent : public ConnectedComponent<FeedState> {
 *   public:
 *     explicit MyComponent(Stores::RootStore* store)
 *       : ConnectedComponent(
 *           store,
 *           [this](Stores::RootStore* s) { return s->subscribeToPosts([this](const auto& state) {
 *             updateUI(state);
 *           }); }
 *         ) {}
 *   };
 *
 * Or with selector pattern:
 *   class MyComponent : public ConnectedComponent<bool> {
 *   public:
 *     explicit MyComponent(Stores::RootStore* store)
 *       : ConnectedComponent(
 *           store,
 *           [this](Stores::RootStore* s) { return s->subscribeToLoginStatus([this](bool isLoggedIn) {
 *             updateUI(isLoggedIn);
 *           }); }
 *         ) {}
 *   };
 *
 * Benefits:
 * - No virtual method overrides needed
 * - Subscription setup in constructor
 * - Clear intent: "this component is connected to X part of store"
 * - Same memory safety as AppStoreComponent
 * - Automatic RAII cleanup
 */
template <typename StateType> class ConnectedComponent : public juce::Component {
public:
  /**
   * Constructor that sets up store binding
   * @param store RootStore instance (can be nullptr for optional binding)
   * @param subscribe Function that performs subscription and returns unsubscriber
   *
   * The subscribe function receives the RootStore and should:
   * - Call the appropriate rootStore->subscribeTo*() method
   * - Update the component when state changes
   * - Return an unsubscriber function
   *
   * Unsubscription happens automatically in destructor
   */
  explicit ConnectedComponent(Stores::RootStore *store,
                              std::function<Stores::RootStore::Unsubscriber(Stores::RootStore *)> subscribe)
      : store_(store) {
    if (store_) {
      try {
        unsubscriber_ = subscribe(store_);
      } catch (const std::exception &e) {
        Log::error("ConnectedComponent: Failed to subscribe to store: " + juce::String(e.what()));
      }
    }
  }

  /**
   * Alternative constructor with selector callback
   * Useful when component only needs a specific state field
   */
  template <typename RootStateType, typename SelectedType>
  explicit ConnectedComponent(Stores::RootStore *store, std::function<SelectedType(const RootStateType &)> selector,
                              std::function<void(const SelectedType &)> onStateChange)
      : store_(store) {
    if (store_) {
      try {
        unsubscriber_ = store_->subscribeToSelection(selector, onStateChange);
      } catch (const std::exception &e) {
        Log::error("ConnectedComponent: Failed to subscribe to store: " + juce::String(e.what()));
      }
    }
  }

  /**
   * Destructor - automatically unsubscribes
   */
  ~ConnectedComponent() override {
    if (unsubscriber_) {
      try {
        unsubscriber_();
      } catch (const std::exception &e) {
        Log::error("ConnectedComponent: Exception during unsubscribe: " + juce::String(e.what()));
      }
      unsubscriber_ = nullptr;
    }
  }

  /**
   * Check if component is properly connected to store
   */
  bool isConnected() const {
    return store_ != nullptr && unsubscriber_ != nullptr;
  }

  /**
   * Get the RootStore instance
   */
  Stores::RootStore *getStore() const {
    return store_;
  }

  /**
   * Reconnect to store (unsubscribes from old, subscribes to new)
   * @param newStore New RootStore to connect to
   * @param resubscribe Function to set up subscription with new store
   */
  void reconnect(Stores::RootStore *newStore,
                 std::function<Stores::RootStore::Unsubscriber(Stores::RootStore *)> resubscribe) {
    // Unsubscribe from old store
    if (unsubscriber_) {
      try {
        unsubscriber_();
      } catch (const std::exception &e) {
        Log::error("ConnectedComponent: Exception during unsubscribe: " + juce::String(e.what()));
      }
      unsubscriber_ = nullptr;
    }

    store_ = newStore;

    // Subscribe to new store
    if (store_) {
      try {
        unsubscriber_ = resubscribe(store_);
      } catch (const std::exception &e) {
        Log::error("ConnectedComponent: Failed to subscribe to new store: " + juce::String(e.what()));
      }
    }
  }

  /**
   * Disconnect from store (but don't destroy the component)
   */
  void disconnect() {
    if (unsubscriber_) {
      try {
        unsubscriber_();
      } catch (const std::exception &e) {
        Log::error("ConnectedComponent: Exception during unsubscribe: " + juce::String(e.what()));
      }
      unsubscriber_ = nullptr;
    }
    store_ = nullptr;
  }

protected:
  Stores::RootStore *store_ = nullptr;
  std::function<void()> unsubscriber_;

  /**
   * Helper to safely call callback on message thread
   * Useful for components that need to update UI when state changes
   */
  template <typename CallbackType> static auto createMessageThreadCallback(CallbackType callback) {
    return [callback = std::move(callback)](const auto &state) {
      juce::MessageManager::callAsync([callback, state]() { callback(state); });
    };
  }

private:
  // Prevent copying
  ConnectedComponent(const ConnectedComponent &) = delete;
  ConnectedComponent &operator=(const ConnectedComponent &) = delete;
  ConnectedComponent(ConnectedComponent &&) = delete;
  ConnectedComponent &operator=(ConnectedComponent &&) = delete;
};

/**
 * Helper factory for creating connected components
 *
 * Usage:
 *   auto component = connect<FeedState>(
 *       store,
 *       [](auto* s) { return s->subscribeToPosts([](const auto& state) { ... }); }
 *   );
 */
template <typename StateType>
inline std::unique_ptr<ConnectedComponent<StateType>>
connect(Stores::RootStore *store, std::function<Stores::RootStore::Unsubscriber(Stores::RootStore *)> subscribe) {
  return std::make_unique<ConnectedComponent<StateType>>(store, subscribe);
}

} // namespace UI
} // namespace Sidechain
