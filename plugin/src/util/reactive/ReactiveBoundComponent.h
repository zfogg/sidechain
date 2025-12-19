#pragma once

#include "ObservableCollection.h"
#include "ObservableProperty.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Sidechain {
namespace Util {

/**
 * ReactiveBoundComponent - Base class for components with reactive data binding
 *
 * Automatically syncs observable properties to UI by calling repaint() when
 * properties change. Eliminates boilerplate observer registration/cleanup.
 *
 * Features:
 * - Automatic repaint on property changes
 * - RAII-based observer cleanup on destruction
 * - Type-safe property binding
 * - Zero-copy value passing in paint()
 * - Computed properties support
 * - Template-based for maximum flexibility
 *
 * Usage:
 *   class MyComponent : public ReactiveBoundComponent<MyComponent>
 *   {
 *   public:
 *       MyComponent() {
 *           BIND_PROPERTY(username, this);
 *           BIND_PROPERTY(isLoading, this);
 *       }
 *
 *       void paint(juce::Graphics& g) override {
 *           auto name = username.get();
 *           auto loading = isLoading.get();
 *           // ... render using current property values
 *       }
 *
 *   private:
 *       ObservableProperty<juce::String> username{"Guest"};
 *       AtomicObservableProperty<bool> isLoading{false};
 *   };
 *
 *   // Later, when properties change:
 *   component->username.set("Alice");  // Automatically calls repaint!
 *
 * Implementation notes:
 * - BIND_PROPERTY should be called in constructor
 * - Each property binding registers an observer
 * - Observers automatically unregister on component destruction
 * - repaint() is called on message thread (safe for JUCE)
 */
class ReactiveBoundComponent : public juce::Component {
public:
  ReactiveBoundComponent() = default;

  virtual ~ReactiveBoundComponent() override {
    clearBindings();
  }

  /**
   * Unregister all property bindings (called automatically in destructor)
   */
  void clearBindings() {
    propertyUnsubscribers.clear();
  }

  /**
   * Get number of active property bindings
   */
  size_t getBindingCount() const {
    return propertyUnsubscribers.size();
  }

protected:
  /**
   * Register a property binding to this component
   *
   * @tparam T The property value type
   * @param property The observable property to bind
   *
   * When the property changes, repaint() will be called automatically.
   * The binding is automatically cleaned up when the component is destroyed.
   */
  template <typename T> void bindProperty(std::shared_ptr<ObservableProperty<T>> property) {
    if (!property)
      return;

    auto unsubscriber = property->observe([this](const T &) {
      // Repaint on message thread
      if (auto *mm = juce::MessageManager::getInstanceWithoutCreating()) {
        mm->callAsync([this]() { repaint(); });
      } else {
        repaint(); // Fallback if message manager unavailable
      }
    });

    propertyUnsubscribers.push_back(unsubscriber);
  }

  /**
   * Bind a regular (non-mapped/filtered) property
   *
   * @tparam T The property value type
   * @param property The observable property to bind
   */
  template <typename T> void bindProperty(ObservableProperty<T> &property) {
    auto unsubscriber = property.observe([this](const T &) {
      if (auto *mm = juce::MessageManager::getInstanceWithoutCreating()) {
        mm->callAsync([this]() { repaint(); });
      } else {
        repaint();
      }
    });

    propertyUnsubscribers.push_back(unsubscriber);
  }

  /**
   * Bind an atomic property (for small types with lock-free reads)
   *
   * @tparam T The property value type (bool, int, float, etc)
   * @param property The atomic observable property to bind
   */
  template <typename T> void bindAtomicProperty(AtomicObservableProperty<T> &property) {
    auto unsubscriber = property.observe([this](const T &) {
      if (auto *mm = juce::MessageManager::getInstanceWithoutCreating()) {
        mm->callAsync([this]() { repaint(); });
      } else {
        repaint();
      }
    });

    propertyUnsubscribers.push_back(unsubscriber);
  }

  /**
   * Bind an observable array
   *
   * @tparam T The array element type
   * @param array The observable array to bind
   */
  template <typename T> void bindArray(ObservableArray<T> &array) {
    // Bind to item added notifications
    auto unsub1 = array.observeItemAdded([this](int, const T &) { scheduleRepaint(); });

    // Bind to item removed notifications
    auto unsub2 = array.observeItemRemoved([this](int, const T &) { scheduleRepaint(); });

    // Bind to item changed notifications
    auto unsub3 = array.observeItemChanged([this](int, const T &, const T &) { scheduleRepaint(); });

    propertyUnsubscribers.push_back(unsub1);
    propertyUnsubscribers.push_back(unsub2);
    propertyUnsubscribers.push_back(unsub3);
  }

  /**
   * Bind an observable map
   *
   * @tparam K The key type
   * @tparam V The value type
   * @param map The observable map to bind
   */
  template <typename K, typename V> void bindMap(ObservableMap<K, V> &map) {
    auto unsub1 = map.observeItemAdded([this](const K &, const V &) { scheduleRepaint(); });

    auto unsub2 = map.observeItemRemoved([this](const K &) { scheduleRepaint(); });

    auto unsub3 = map.observeItemChanged([this](const K &, const V &, const V &) { scheduleRepaint(); });

    propertyUnsubscribers.push_back(unsub1);
    propertyUnsubscribers.push_back(unsub2);
    propertyUnsubscribers.push_back(unsub3);
  }

  /**
   * Create a computed property that derives from another property
   *
   * Useful for UI computations that depend on underlying data.
   *
   * @tparam T The source property type
   * @tparam U The computed result type
   * @param source The property to compute from
   * @param transform Function that computes the result
   * @return New observable property with transformed values
   *
   * Example:
   *   ObservableProperty<int> age{25};
   *   auto birthYear = computed<int, int>(age,
   *       [](int a) { return 2024 - a; });
   *   bindProperty(birthYear);
   */
  template <typename T, typename U>
  std::shared_ptr<ObservableProperty<U>> computed(ObservableProperty<T> &source,
                                                  std::function<U(const T &)> transform) {
    auto result = source.template map<U>(transform);
    bindProperty(result);
    return result;
  }

private:
  // Vector of unsubscriber functions
  std::vector<std::function<void()>> propertyUnsubscribers;

  /**
   * Schedule a repaint on the message thread
   */
  void scheduleRepaint() {
    if (auto *mm = juce::MessageManager::getInstanceWithoutCreating()) {
      mm->callAsync([this]() { repaint(); });
    } else {
      repaint();
    }
  }

  // Prevent copying
  ReactiveBoundComponent(const ReactiveBoundComponent &) = delete;
  ReactiveBoundComponent &operator=(const ReactiveBoundComponent &) = delete;
};

} // namespace Util
} // namespace Sidechain

/**
 * BIND_PROPERTY macro - Convenient property binding in constructors
 *
 * Example:
 *   MyComponent::MyComponent() {
 *       BIND_PROPERTY(username, this);
 *       BIND_PROPERTY(isLoading, this);
 *       BIND_PROPERTY(userData, this);
 *   }
 *
 * Expands to:
 *   this->bindProperty(username);
 *   this->bindProperty(isLoading);
 *   this->bindProperty(userData);
 *
 * Note: Can only be used in classes derived from ReactiveBoundComponent
 */
#define BIND_PROPERTY(prop, component) (component)->bindProperty((prop))

#define BIND_ATOMIC_PROPERTY(prop, component) (component)->bindAtomicProperty((prop))

#define BIND_ARRAY(arr, component) (component)->bindArray((arr))

#define BIND_MAP(map, component) (component)->bindMap((map))
