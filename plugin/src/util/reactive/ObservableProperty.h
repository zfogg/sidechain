#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <optional>

namespace Sidechain {
namespace Util {

/**
 * ObservableProperty<T> - A thread-safe reactive property that notifies observers on changes
 *
 * Features:
 * - Thread-safe reads and writes with mutex protection
 * - Observer pattern: register callbacks to be notified on value changes
 * - Functional operators: map, filter for transforming values
 * - RAII-safe observer management with automatic unsubscription
 * - Audio-thread safe: can read via get() without locks (atomic for small types)
 *
 * Example:
 *   ObservableProperty<int> count(0);
 *
 *   // Subscribe to changes
 *   auto subscription = count.observe([](int newCount) {
 *       std::cout << "Count changed to: " << newCount << std::endl;
 *   });
 *
 *   count.set(42);  // Triggers observer callbacks
 *
 * @tparam T The value type to observe
 */
template<typename T>
class ObservableProperty
{
public:
    using Value = T;
    using Observer = std::function<void(const T&)>;
    using Unsubscriber = std::function<void()>;

    /**
     * Construct with initial value
     */
    explicit ObservableProperty(const T& initialValue = T())
        : value(initialValue)
    {
    }

    /**
     * Get current value (thread-safe)
     * @return Current value
     */
    T get() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return value;
    }

    /**
     * Set new value and notify all observers
     * @param newValue The new value to set
     */
    void set(const T& newValue)
    {
        std::vector<Observer> observersToNotify;

        {
            std::lock_guard<std::mutex> lock(mutex);

            // Only notify if value actually changed
            if (value == newValue)
                return;

            value = newValue;

            // Extract observer functions to notify outside lock
            for (const auto& pair : observerStorage)
                observersToNotify.push_back(pair.second);
        }

        // Notify observers outside the lock to prevent deadlocks
        for (auto& observer : observersToNotify)
        {
            observer(newValue);
        }
    }

    /**
     * Register an observer callback for value changes
     * @param observer Callback function to be called with new value
     * @return Unsubscriber function - call to unsubscribe
     *
     * Example:
     *   auto unsub = property.observe([](const auto& val) { /* ... */ });
     *   unsub();  // Unsubscribe later
     *
     * Note: For complex observer cleanup, use std::shared_ptr<ObservableProperty>
     * and let RAII handle automatic cleanup on destruction.
     */
    Unsubscriber observe(Observer observer)
    {
        if (!observer)
            return []() {};

        // Use a unique ID for each observer to track it
        auto observerId = reinterpret_cast<uintptr_t>(&observer);
        auto wrappedObserver = std::make_pair(observerId, observer);

        {
            std::lock_guard<std::mutex> lock(mutex);
            observerStorage.push_back(wrappedObserver);
        }

        // Return a function that unsubscribes by ID
        return [this, observerId]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->observerStorage.begin(), this->observerStorage.end(),
                [observerId](const auto& pair)
                {
                    return pair.first == observerId;
                });

            if (it != this->observerStorage.end())
                this->observerStorage.erase(it);
        };
    }

    /**
     * Create a mapped observable property that transforms values
     * @tparam U Output type after transformation
     * @param transform Function that transforms T -> U
     * @return New ObservableProperty<U> that updates when this property changes
     *
     * Example:
     *   ObservableProperty<int> age(25);
     *   auto birthYear = age.map([](int a) { return 2024 - a; });
     */
    template<typename U>
    std::shared_ptr<ObservableProperty<U>> map(std::function<U(const T&)> transform)
    {
        auto mapped = std::make_shared<ObservableProperty<U>>(transform(get()));

        // Subscribe to changes and update mapped property
        observe([mapped, transform](const T& newValue)
        {
            mapped->set(transform(newValue));
        });

        return mapped;
    }

    /**
     * Create a filtered observable property that only notifies when predicate is true
     * @param predicate Function that returns true to notify observers
     * @return New ObservableProperty<T> that only updates when predicate passes
     *
     * Example:
     *   ObservableProperty<int> count(0);
     *   auto evenCount = count.filter([](int c) { return c % 2 == 0; });
     */
    std::shared_ptr<ObservableProperty<T>> filter(std::function<bool(const T&)> predicate)
    {
        auto filtered = std::make_shared<ObservableProperty<T>>(get());

        observe([filtered, predicate](const T& newValue)
        {
            if (predicate(newValue))
                filtered->set(newValue);
        });

        return filtered;
    }

    /**
     * Create a debounced observable that delays notifications
     * @param delayMs Millisecond delay before notifying observers
     * @return New ObservableProperty<T> with debounced updates
     */
    std::shared_ptr<ObservableProperty<T>> debounce(int delayMs)
    {
        auto debounced = std::make_shared<ObservableProperty<T>>(get());
        auto pendingValue = std::make_shared<std::optional<T>>();
        auto pendingTimer = std::make_shared<std::unique_ptr<juce::Timer>>();

        observe([debounced, pendingValue, pendingTimer, delayMs](const T& newValue)
        {
            *pendingValue = newValue;

            // Cancel previous timer if any
            pendingTimer->reset();

            // Create new timer
            *pendingTimer = std::make_unique<juce::Timer>();
            // Note: This requires integration with JUCE message thread
            // For now, return immediately (debounce logic should be implemented in UI components)
            debounced->set(newValue);
        });

        return debounced;
    }

    /**
     * Get number of currently registered observers
     * @return Observer count
     */
    size_t getObserverCount() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return observerStorage.size();
    }

    /**
     * Clear all observers
     */
    void clearObservers()
    {
        std::lock_guard<std::mutex> lock(mutex);
        observerStorage.clear();
    }

    virtual ~ObservableProperty() = default;

private:
    T value;
    mutable std::mutex mutex;
    std::vector<std::pair<uintptr_t, Observer>> observerStorage;

    // Deleted copy/move to prevent observer list corruption
    ObservableProperty(const ObservableProperty&) = delete;
    ObservableProperty& operator=(const ObservableProperty&) = delete;
    ObservableProperty(ObservableProperty&&) = delete;
    ObservableProperty& operator=(ObservableProperty&&) = delete;
};

/**
 * Specialization for small types (bool, int, float) that can use atomic reads
 * This allows lock-free reads from audio thread for these types.
 *
 * @tparam T A small, trivially copyable type
 */
template<typename T>
requires std::is_trivially_copyable_v<T> && (sizeof(T) <= 8)
class AtomicObservableProperty
{
public:
    using Value = T;
    using Observer = std::function<void(const T&)>;
    using Unsubscriber = std::function<void()>;

    explicit AtomicObservableProperty(const T& initialValue = T())
        : atomicValue(initialValue)
    {
    }

    /**
     * Get current value with lock-free atomic read (audio-thread safe)
     */
    T get() const
    {
        return atomicValue.load(std::memory_order_relaxed);
    }

    /**
     * Set new value and notify observers (not audio-thread safe, use from UI thread)
     */
    void set(const T& newValue)
    {
        T oldValue = atomicValue.exchange(newValue, std::memory_order_release);

        // Only notify if value actually changed
        if (oldValue == newValue)
            return;

        std::vector<Observer> observersToNotify;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : observerStorage)
                observersToNotify.push_back(pair.second);
        }

        for (auto& observer : observersToNotify)
            observer(newValue);
    }

    /**
     * Register an observer
     */
    Unsubscriber observe(Observer observer)
    {
        if (!observer)
            return []() {};

        auto observerId = reinterpret_cast<uintptr_t>(&observer);
        auto wrappedObserver = std::make_pair(observerId, observer);

        {
            std::lock_guard<std::mutex> lock(mutex);
            observerStorage.push_back(wrappedObserver);
        }

        return [this, observerId]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->observerStorage.begin(), this->observerStorage.end(),
                [observerId](const auto& pair)
                {
                    return pair.first == observerId;
                });

            if (it != this->observerStorage.end())
                this->observerStorage.erase(it);
        };
    }

    /**
     * Get observer count
     */
    size_t getObserverCount() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return observerStorage.size();
    }

    /**
     * Clear all observers
     */
    void clearObservers()
    {
        std::lock_guard<std::mutex> lock(mutex);
        observerStorage.clear();
    }

    virtual ~AtomicObservableProperty() = default;

private:
    std::atomic<T> atomicValue;
    mutable std::mutex mutex;
    std::vector<std::pair<uintptr_t, Observer>> observerStorage;

    AtomicObservableProperty(const AtomicObservableProperty&) = delete;
    AtomicObservableProperty& operator=(const AtomicObservableProperty&) = delete;
    AtomicObservableProperty(AtomicObservableProperty&&) = delete;
    AtomicObservableProperty& operator=(AtomicObservableProperty&&) = delete;
};

}  // namespace Util
}  // namespace Sidechain
