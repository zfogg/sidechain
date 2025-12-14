.. _observables:

Observable Pattern
==================

The Observable pattern provides fine-grained reactive data bindings at the property level.
Observables automatically notify subscribers when values change, enabling reactive composition
chains and automatic UI updates.

Core Observable Types
---------------------

ObservableProperty<T>
~~~~~~~~~~~~~~~~~~~~~

Thread-safe observable for any copyable type with mutex-based synchronization.

**Location**: ``plugin/src/util/reactive/ObservableProperty.h``

**Basic Usage**:

.. code-block:: cpp

    #include "util/reactive/ObservableProperty.h"

    // Create observable property
    ObservableProperty<juce::String> username{"Guest"};
    ObservableProperty<int> age{25};

    // Subscribe to changes
    auto unsubscribe = username.observe([](const juce::String& newValue) {
        DBG("Username changed to: " + newValue);
    });

    // Update value (triggers observer)
    username.set("Alice");  // Prints: "Username changed to: Alice"

    // Read current value
    juce::String current = username.get();

    // Cleanup (RAII pattern)
    unsubscribe();  // Stop observing

**Features**:

* Thread-safe get/set with std::mutex
* Multiple observers per property
* Automatic observer cleanup via RAII
* Value comparison to prevent unnecessary notifications
* Template-based for any copyable type

AtomicObservableProperty<T>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Lock-free observable for small types (bool, int, float, pointers) with atomic operations.

**When to Use**:

* Audio thread-safe reads (no locks!)
* High-frequency updates
* Small trivially copyable types
* Performance-critical paths

**Example**:

.. code-block:: cpp

    // Lock-free observable (safe for audio thread reads)
    AtomicObservableProperty<bool> isRecording{false};
    AtomicObservableProperty<float> gain{1.0f};

    // Audio thread can read without blocking
    void processBlock(AudioBuffer<float>& buffer) {
        if (isRecording.get()) {  // Lock-free read!
            // Apply gain
            buffer.applyGain(gain.get());
        }
    }

    // UI thread can update
    void onRecordButtonClicked() {
        isRecording.set(true);  // Triggers observers on message thread
    }

ObservableArray<T>
~~~~~~~~~~~~~~~~~~

Observable vector/array with insertion, removal, and modification notifications.

**Location**: ``plugin/src/util/reactive/ObservableArray.h``

**Usage**:

.. code-block:: cpp

    ObservableArray<FeedPost> posts;

    // Subscribe to item additions
    auto unsubAdd = posts.observeItemAdded([](int index, const FeedPost& post) {
        DBG("Post added at index " + juce::String(index));
    });

    // Subscribe to item removals
    auto unsubRemove = posts.observeItemRemoved([](int index, const FeedPost& post) {
        DBG("Post removed from index " + juce::String(index));
    });

    // Subscribe to item changes
    auto unsubChange = posts.observeItemChanged(
        [](int index, const FeedPost& oldPost, const FeedPost& newPost) {
            DBG("Post at index " + juce::String(index) + " changed");
        }
    );

    // Modify array
    posts.add(newPost);           // Triggers observeItemAdded
    posts.remove(0);              // Triggers observeItemRemoved
    posts.set(1, updatedPost);    // Triggers observeItemChanged

ObservableMap<K, V>
~~~~~~~~~~~~~~~~~~~

Observable dictionary/map with key-value change notifications.

**Location**: ``plugin/src/util/reactive/ObservableMap.h``

**Usage**:

.. code-block:: cpp

    ObservableMap<juce::String, User> userCache;

    // Subscribe to additions
    auto unsub = userCache.observeItemAdded([](const juce::String& key, const User& user) {
        DBG("User cached: " + key);
    });

    // Subscribe to changes
    userCache.observeItemChanged(
        [](const juce::String& key, const User& oldUser, const User& newUser) {
            DBG("User updated: " + key);
        }
    );

    // Modify map
    userCache.set("user-123", user);  // Triggers observeItemAdded or observeItemChanged
    userCache.remove("user-123");     // Triggers observeItemRemoved

Reactive Composition
--------------------

Observables support functional composition with map() and filter() operations.

map() - Transform Values
~~~~~~~~~~~~~~~~~~~~~~~~~

Create derived observables that transform values:

.. code-block:: cpp

    ObservableProperty<int> age{25};

    // Derived property: birthYear = currentYear - age
    auto birthYear = age.map<int>([](int a) {
        return 2024 - a;
    });

    // Subscribe to derived property
    birthYear->observe([](int year) {
        DBG("Birth year: " + juce::String(year));
    });

    age.set(30);  // Triggers: "Birth year: 1994"

**Common Use Cases**:

.. code-block:: cpp

    // String formatting
    ObservableProperty<int> count{0};
    auto countText = count.map<juce::String>([](int n) {
        return juce::String(n) + " items";
    });

    // Validation
    ObservableProperty<juce::String> email;
    auto isValid = email.map<bool>([](const juce::String& e) {
        return e.contains("@") && e.contains(".");
    });

    // Computation chains
    ObservableProperty<float> temperature{20.0f};
    auto fahrenheit = temperature.map<float>([](float c) {
        return c * 9.0f / 5.0f + 32.0f;
    });

filter() - Conditional Updates
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create observables that only update when predicate is true:

.. code-block:: cpp

    ObservableProperty<int> value{0};

    // Only positive values
    auto positiveOnly = value.filter([](int v) {
        return v > 0;
    });

    positiveOnly->observe([](int v) {
        DBG("Positive value: " + juce::String(v));
    });

    value.set(-5);  // No notification (filtered out)
    value.set(10);  // Triggers: "Positive value: 10"

Combining Observables
~~~~~~~~~~~~~~~~~~~~~

Combine multiple observables into computed properties:

.. code-block:: cpp

    ObservableProperty<juce::String> firstName{"John"};
    ObservableProperty<juce::String> lastName{"Doe"};

    // Computed full name
    auto fullName = firstName.map<juce::String>([&lastName](const juce::String& first) {
        return first + " " + lastName.get();
    });

    fullName->observe([](const juce::String& name) {
        DBG("Full name: " + name);
    });

    firstName.set("Jane");  // Triggers: "Full name: Jane Doe"

Performance Characteristics
---------------------------

ObservableProperty<T>
~~~~~~~~~~~~~~~~~~~~~

* **Set**: O(n) - notifies all subscribers
* **Get**: O(1) with mutex lock
* **Memory**: Mutex + value + vector of observers
* **Thread Safety**: Full (std::mutex)

AtomicObservableProperty<T>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Set**: O(n) - atomic write + notify subscribers
* **Get**: O(1) lock-free read
* **Memory**: std::atomic<T> + vector of observers
* **Thread Safety**: Lock-free reads, synchronized writes

ObservableArray<T>
~~~~~~~~~~~~~~~~~~

* **Add**: O(n) for vector + O(m) for subscribers
* **Remove**: O(n) for vector + O(m) for subscribers
* **Memory**: std::vector<T> + observer vectors
* **Thread Safety**: Mutex protected

ObservableMap<K, V>
~~~~~~~~~~~~~~~~~~~

* **Set**: O(log n) for map + O(m) for subscribers
* **Get**: O(log n)
* **Memory**: std::map<K,V> + observer vectors
* **Thread Safety**: Mutex protected

Best Practices
--------------

1. **Use RAII for Unsubscribe**

   Always store unsubscribe functions and call in destructor:

   .. code-block:: cpp

       class MyComponent {
       public:
           MyComponent() {
               unsubscribe = property.observe([this](const auto& value) {
                   // Handle change
               });
           }

           ~MyComponent() {
               if (unsubscribe)
                   unsubscribe();
           }

       private:
           std::function<void()> unsubscribe;
       };

2. **Avoid Circular Dependencies**

   Don't create observer cycles:

   .. code-block:: cpp

       // BAD: Circular dependency
       prop1.observe([&prop2](auto v) { prop2.set(v + 1); });
       prop2.observe([&prop1](auto v) { prop1.set(v - 1); });  // Infinite loop!

3. **Use Atomic for Audio Thread**

   Never use regular ObservableProperty on audio thread:

   .. code-block:: cpp

       // BAD: Mutex lock in audio thread
       ObservableProperty<float> gain;
       void processBlock(AudioBuffer<float>& buffer) {
           buffer.applyGain(gain.get());  // BLOCKS!
       }

       // GOOD: Lock-free atomic
       AtomicObservableProperty<float> gain;
       void processBlock(AudioBuffer<float>& buffer) {
           buffer.applyGain(gain.get());  // Lock-free!
       }

4. **Minimize Observer Work**

   Keep observers lightweight - heavy work should be async:

   .. code-block:: cpp

       // BAD: Heavy work in observer
       property.observe([](const auto& value) {
           performExpensiveComputation(value);  // Blocks all other observers!
       });

       // GOOD: Async heavy work
       property.observe([](const auto& value) {
           Async::run([value]() {
               return performExpensiveComputation(value);
           }, [](const auto& result) {
               updateUI(result);
           });
       });

5. **Prefer Store-Level Subscriptions**

   For large state changes, subscribe to stores rather than individual properties:

   .. code-block:: cpp

       // Less efficient: Many property subscriptions
       username.observe([](auto v) { repaint(); });
       email.observe([](auto v) { repaint(); });
       avatar.observe([](auto v) { repaint(); });

       // Better: Single store subscription
       userStore.subscribe([](const UserStoreState& state) {
           // Update all at once
           repaint();
       });

Common Patterns
---------------

Loading States
~~~~~~~~~~~~~~

.. code-block:: cpp

    ObservableProperty<bool> isLoading{false};
    ObservableProperty<juce::String> errorMessage{""};
    ObservableProperty<std::vector<Item>> items;

    isLoading.observe([this](bool loading) {
        if (loading) showSpinner();
        else hideSpinner();
    });

    errorMessage.observe([this](const juce::String& error) {
        if (error.isNotEmpty()) showError(error);
    });

Form Validation
~~~~~~~~~~~~~~~

.. code-block:: cpp

    ObservableProperty<juce::String> email;
    ObservableProperty<juce::String> password;

    auto emailValid = email.map<bool>([](const juce::String& e) {
        return e.contains("@");
    });

    auto passwordValid = password.map<bool>([](const juce::String& p) {
        return p.length() >= 8;
    });

    // Enable submit button when both valid
    auto canSubmit = emailValid->map<bool>([&passwordValid](bool eValid) {
        return eValid && passwordValid->get();
    });

    canSubmit->observe([this](bool enabled) {
        submitButton.setEnabled(enabled);
    });

Real-Time Sync
~~~~~~~~~~~~~~

.. code-block:: cpp

    ObservableProperty<int> localLikeCount{0};

    // WebSocket update
    websocket.on("like_update", [&localLikeCount](const juce::var& data) {
        localLikeCount.set(data["count"]);  // Triggers UI update
    });

    // Observer updates UI automatically
    localLikeCount.observe([this](int count) {
        likeCountLabel.setText(juce::String(count), juce::dontSendNotification);
        repaint();
    });

See Also
--------

* :doc:`stores` - Store-level state management
* :doc:`reactive-components` - ReactiveBoundComponent integration
* :doc:`threading` - Thread safety and constraints
* :doc:`data-flow` - Complete reactive data flow examples
