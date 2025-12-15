#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>

namespace Sidechain {
namespace Util {

/**
 * ObservableArray<T> - A thread-safe reactive array that notifies observers on collection changes
 *
 * Wraps juce::Array<T> and provides notifications when items are added, removed, or modified.
 *
 * Features:
 * - Change notifications: item added/removed/changed
 * - Batch update support: collect multiple changes before notifying
 * - Functional operators: map, filter, reduce for transformations
 * - Thread-safe with mutex protection
 * - Observer pattern with unsubscriber functions
 *
 * Example:
 *   ObservableArray<juce::String> names;
 *   names.observeItemAdded([](int index, const juce::String& item) {
 *       std::cout << "Added: " << item << " at " << index << std::endl;
 *   });
 *
 *   names.add("Alice");  // Triggers observeItemAdded
 *
 * @tparam T The element type
 */
template<typename T>
class ObservableArray
{
public:
    using Element = T;
    using ItemAddedObserver = std::function<void(int index, const T& item)>;
    using ItemRemovedObserver = std::function<void(int index, const T& item)>;
    using ItemChangedObserver = std::function<void(int index, const T& oldItem, const T& newItem)>;
    using CollectionChangedObserver = std::function<void()>;
    using Unsubscriber = std::function<void()>;

    ObservableArray() = default;

    /**
     * Add element to the end of array
     */
    void add(const T& element)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            int index = items.size();
            items.add(element);
            notifyItemAdded(index, element);
        }
    }

    /**
     * Insert element at specific index
     */
    void insert(int index, const T& element)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            items.insert(index, element);
            notifyItemAdded(index, element);
        }
    }

    /**
     * Remove element at index
     */
    void remove(int index)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (index >= 0 && index < items.size())
            {
                T removed = items[index];
                items.remove(index);
                notifyItemRemoved(index, removed);
            }
        }
    }

    /**
     * Remove all instances of element
     */
    void removeItem(const T& item)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (int i = items.size() - 1; i >= 0; --i)
            {
                if (items[i] == item)
                {
                    items.remove(i);
                    notifyItemRemoved(i, item);
                }
            }
        }
    }

    /**
     * Update element at index
     */
    void set(int index, const T& element)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (index >= 0 && index < items.size())
            {
                T oldElement = items[index];
                items.set(index, element);
                notifyItemChanged(index, oldElement, element);
            }
        }
    }

    /**
     * Get element at index
     */
    T get(int index) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (index >= 0 && index < items.size())
            return items[index];
        return T();
    }

    /**
     * Get size of array
     */
    int size() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return items.size();
    }

    /**
     * Clear all elements
     */
    void clear()
    {
        std::vector<std::pair<int, T>> removed;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (int i = 0; i < items.size(); ++i)
                removed.push_back({i, items[i]});
            items.clear();
        }

        // Notify after releasing lock
        for (const auto& [index, item] : removed)
            notifyItemRemoved(index, item);
    }

    /**
     * Check if contains element
     */
    bool contains(const T& item) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return items.contains(item);
    }

    /**
     * Find index of element
     * @return Index or -1 if not found
     */
    int indexOf(const T& item) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return items.indexOf(item);
    }

    /**
     * Subscribe to item added events
     */
    Unsubscriber observeItemAdded(ItemAddedObserver observer)
    {
        if (!observer)
            return []() {};

        auto id = reinterpret_cast<uintptr_t>(&observer);
        {
            std::lock_guard<std::mutex> lock(mutex);
            itemAddedObservers.push_back({id, observer});
        }

        return [this, id]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->itemAddedObservers.begin(), this->itemAddedObservers.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != this->itemAddedObservers.end())
                this->itemAddedObservers.erase(it);
        };
    }

    /**
     * Subscribe to item removed events
     */
    Unsubscriber observeItemRemoved(ItemRemovedObserver observer)
    {
        if (!observer)
            return []() {};

        auto id = reinterpret_cast<uintptr_t>(&observer);
        {
            std::lock_guard<std::mutex> lock(mutex);
            itemRemovedObservers.push_back({id, observer});
        }

        return [this, id]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->itemRemovedObservers.begin(), this->itemRemovedObservers.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != this->itemRemovedObservers.end())
                this->itemRemovedObservers.erase(it);
        };
    }

    /**
     * Subscribe to item changed events
     */
    Unsubscriber observeItemChanged(ItemChangedObserver observer)
    {
        if (!observer)
            return []() {};

        auto id = reinterpret_cast<uintptr_t>(&observer);
        {
            std::lock_guard<std::mutex> lock(mutex);
            itemChangedObservers.push_back({id, observer});
        }

        return [this, id]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->itemChangedObservers.begin(), this->itemChangedObservers.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != this->itemChangedObservers.end())
                this->itemChangedObservers.erase(it);
        };
    }

    /**
     * Subscribe to collection changed events (any change)
     */
    Unsubscriber observeCollectionChanged(CollectionChangedObserver observer)
    {
        if (!observer)
            return []() {};

        auto id = reinterpret_cast<uintptr_t>(&observer);
        {
            std::lock_guard<std::mutex> lock(mutex);
            collectionChangedObservers.push_back({id, observer});
        }

        return [this, id]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->collectionChangedObservers.begin(), this->collectionChangedObservers.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != this->collectionChangedObservers.end())
                this->collectionChangedObservers.erase(it);
        };
    }

    /**
     * Begin batch update - suppress notifications until endBatchUpdate() is called
     */
    void beginBatchUpdate()
    {
        std::lock_guard<std::mutex> lock(mutex);
        batchMode = true;
    }

    /**
     * End batch update - trigger deferred notifications
     */
    void endBatchUpdate()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            batchMode = false;
        }
        // Notify after releasing lock
        notifyCollectionChanged();
    }

    /**
     * Create a mapped observable array with transformed elements
     * @tparam U Output element type
     * @param transform Function that transforms T -> U
     */
    template<typename U>
    std::shared_ptr<ObservableArray<U>> map(std::function<U(const T&)> transform)
    {
        auto mapped = std::make_shared<ObservableArray<U>>();

        // Add all existing items
        std::lock_guard<std::mutex> lock(mutex);
        for (int i = 0; i < items.size(); ++i)
            mapped->add(transform(items[i]));

        // Subscribe to changes
        observeItemAdded([mapped, transform](int index, const T& item)
        {
            mapped->insert(index, transform(item));
        });

        observeItemRemoved([mapped](int index, const T&)
        {
            mapped->remove(index);
        });

        observeItemChanged([mapped, transform](int index, const T& /* oldItem */, const T& newItem)
        {
            mapped->set(index, transform(newItem));
        });

        return mapped;
    }

    /**
     * Create a filtered observable array
     * @param predicate Function that returns true to include item
     */
    std::shared_ptr<ObservableArray<T>> filter(std::function<bool(const T&)> predicate)
    {
        auto filtered = std::make_shared<ObservableArray<T>>();

        // Add matching items
        std::lock_guard<std::mutex> lock(mutex);
        for (int i = 0; i < items.size(); ++i)
        {
            if (predicate(items[i]))
                filtered->add(items[i]);
        }

        // Subscribe to changes
        observeItemAdded([filtered, predicate](int /* index */, const T& item)
        {
            if (predicate(item))
                filtered->add(item);
        });

        observeItemRemoved([filtered](int /* index */, const T& item)
        {
            filtered->removeItem(item);
        });

        observeItemChanged([filtered, predicate](int /* index */, const T& oldItem, const T& newItem)
        {
            bool oldMatches = predicate(oldItem);
            bool newMatches = predicate(newItem);

            if (oldMatches && !newMatches)
                filtered->removeItem(oldItem);
            else if (!oldMatches && newMatches)
                filtered->add(newItem);
            else if (oldMatches && newMatches && oldItem != newItem)
                filtered->removeItem(oldItem);
            filtered->add(newItem);
        });

        return filtered;
    }

    /**
     * Get read-only copy of array
     */
    juce::Array<T> getSnapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return items;
    }

    virtual ~ObservableArray() = default;

private:
    juce::Array<T> items;
    mutable std::mutex mutex;
    bool batchMode = false;

    std::vector<std::pair<uintptr_t, ItemAddedObserver>> itemAddedObservers;
    std::vector<std::pair<uintptr_t, ItemRemovedObserver>> itemRemovedObservers;
    std::vector<std::pair<uintptr_t, ItemChangedObserver>> itemChangedObservers;
    std::vector<std::pair<uintptr_t, CollectionChangedObserver>> collectionChangedObservers;

    void notifyItemAdded(int index, const T& item)
    {
        if (batchMode)
            return;

        std::vector<ItemAddedObserver> observers;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : itemAddedObservers)
                observers.push_back(pair.second);
        }

        for (auto& obs : observers)
            obs(index, item);

        notifyCollectionChanged();
    }

    void notifyItemRemoved(int index, const T& item)
    {
        if (batchMode)
            return;

        std::vector<ItemRemovedObserver> observers;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : itemRemovedObservers)
                observers.push_back(pair.second);
        }

        for (auto& obs : observers)
            obs(index, item);

        notifyCollectionChanged();
    }

    void notifyItemChanged(int index, const T& oldItem, const T& newItem)
    {
        if (batchMode)
            return;

        std::vector<ItemChangedObserver> observers;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : itemChangedObservers)
                observers.push_back(pair.second);
        }

        for (auto& obs : observers)
            obs(index, oldItem, newItem);

        notifyCollectionChanged();
    }

    void notifyCollectionChanged()
    {
        if (batchMode)
            return;

        std::vector<CollectionChangedObserver> observers;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : collectionChangedObservers)
                observers.push_back(pair.second);
        }

        for (auto& obs : observers)
            obs();
    }

    ObservableArray(const ObservableArray&) = delete;
    ObservableArray& operator=(const ObservableArray&) = delete;
    ObservableArray(ObservableArray&&) = delete;
    ObservableArray& operator=(ObservableArray&&) = delete;
};

/**
 * ObservableMap<K, V> - A thread-safe reactive map that notifies on changes
 *
 * @tparam K Key type
 * @tparam V Value type
 */
template<typename K, typename V>
class ObservableMap
{
public:
    using Key = K;
    using Value = V;
    using ItemAddedObserver = std::function<void(const K& key, const V& value)>;
    using ItemRemovedObserver = std::function<void(const K& key)>;
    using ItemChangedObserver = std::function<void(const K& key, const V& oldValue, const V& newValue)>;
    using Unsubscriber = std::function<void()>;

    ObservableMap() = default;

    /**
     * Insert or update value
     */
    void set(const K& key, const V& value)
    {
        V oldValue;
        [[maybe_unused]] bool existed = false;

        {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = items.find(key);
            if (it != items.end())
            {
                existed = true;
                oldValue = it->second;
                items[key] = value;
                notifyItemChanged(key, oldValue, value);
            }
            else
            {
                items[key] = value;
                notifyItemAdded(key, value);
            }
        }
    }

    /**
     * Get value by key
     */
    std::optional<V> get(const K& key) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = items.find(key);
        if (it != items.end())
            return it->second;
        return std::nullopt;
    }

    /**
     * Remove key from map
     */
    void remove(const K& key)
    {
        V value;
        bool found = false;

        {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = items.find(key);
            if (it != items.end())
            {
                value = it->second;
                items.erase(it);
                found = true;
            }
        }

        if (found)
            notifyItemRemoved(key);
    }

    /**
     * Check if key exists
     */
    bool contains(const K& key) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return items.find(key) != items.end();
    }

    /**
     * Get size
     */
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return items.size();
    }

    /**
     * Clear all items
     */
    void clear()
    {
        std::vector<K> keys;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : items)
                keys.push_back(pair.first);
            items.clear();
        }

        for (const auto& key : keys)
            notifyItemRemoved(key);
    }

    /**
     * Subscribe to item added
     */
    Unsubscriber observeItemAdded(ItemAddedObserver observer)
    {
        if (!observer)
            return []() {};

        auto id = reinterpret_cast<uintptr_t>(&observer);
        {
            std::lock_guard<std::mutex> lock(mutex);
            itemAddedObservers.push_back({id, observer});
        }

        return [this, id]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->itemAddedObservers.begin(), this->itemAddedObservers.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != this->itemAddedObservers.end())
                this->itemAddedObservers.erase(it);
        };
    }

    /**
     * Subscribe to item removed
     */
    Unsubscriber observeItemRemoved(ItemRemovedObserver observer)
    {
        if (!observer)
            return []() {};

        auto id = reinterpret_cast<uintptr_t>(&observer);
        {
            std::lock_guard<std::mutex> lock(mutex);
            itemRemovedObservers.push_back({id, observer});
        }

        return [this, id]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->itemRemovedObservers.begin(), this->itemRemovedObservers.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != this->itemRemovedObservers.end())
                this->itemRemovedObservers.erase(it);
        };
    }

    /**
     * Subscribe to item changed
     */
    Unsubscriber observeItemChanged(ItemChangedObserver observer)
    {
        if (!observer)
            return []() {};

        auto id = reinterpret_cast<uintptr_t>(&observer);
        {
            std::lock_guard<std::mutex> lock(mutex);
            itemChangedObservers.push_back({id, observer});
        }

        return [this, id]()
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            auto it = std::find_if(this->itemChangedObservers.begin(), this->itemChangedObservers.end(),
                [id](const auto& pair) { return pair.first == id; });
            if (it != this->itemChangedObservers.end())
                this->itemChangedObservers.erase(it);
        };
    }

    virtual ~ObservableMap() = default;

private:
    std::map<K, V> items;
    mutable std::mutex mutex;

    std::vector<std::pair<uintptr_t, ItemAddedObserver>> itemAddedObservers;
    std::vector<std::pair<uintptr_t, ItemRemovedObserver>> itemRemovedObservers;
    std::vector<std::pair<uintptr_t, ItemChangedObserver>> itemChangedObservers;

    void notifyItemAdded(const K& key, const V& value)
    {
        std::vector<ItemAddedObserver> observers;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : itemAddedObservers)
                observers.push_back(pair.second);
        }

        for (auto& obs : observers)
            obs(key, value);
    }

    void notifyItemRemoved(const K& key)
    {
        std::vector<ItemRemovedObserver> observers;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : itemRemovedObservers)
                observers.push_back(pair.second);
        }

        for (auto& obs : observers)
            obs(key);
    }

    void notifyItemChanged(const K& key, const V& oldValue, const V& newValue)
    {
        std::vector<ItemChangedObserver> observers;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : itemChangedObservers)
                observers.push_back(pair.second);
        }

        for (auto& obs : observers)
            obs(key, oldValue, newValue);
    }

    ObservableMap(const ObservableMap&) = delete;
    ObservableMap& operator=(const ObservableMap&) = delete;
    ObservableMap(ObservableMap&&) = delete;
    ObservableMap& operator=(ObservableMap&&) = delete;
};

}  // namespace Util
}  // namespace Sidechain
