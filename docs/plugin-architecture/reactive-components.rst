.. _reactive-components:

Reactive Components
===================

``ReactiveBoundComponent`` is the base class for UI components that automatically update when
reactive data changes. It integrates Observable and Store patterns with JUCE components,
eliminating manual ``repaint()`` calls and ensuring UI stays in sync with state.

Overview
--------

ReactiveBoundComponent provides:

* Automatic ``repaint()`` when bound properties/stores change
* RAII-based observer cleanup on destruction
* Type-safe property binding with templates
* Message thread marshalling for UI safety
* Zero-copy value passing in ``paint()``

**Location**: ``plugin/src/util/reactive/ReactiveBoundComponent.h``

Basic Usage
-----------

Inherit from ReactiveBoundComponent
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include "util/reactive/ReactiveBoundComponent.h"

    class MyComponent : public Sidechain::Util::ReactiveBoundComponent
    {
    public:
        MyComponent() {
            // Bind properties - component will repaint when they change
            bindProperty(username);
            bindAtomicProperty(isLoading);
        }

        void paint(juce::Graphics& g) override {
            // Read current values (no subscription needed!)
            auto name = username.get();
            auto loading = isLoading.get();

            if (loading) {
                g.drawText("Loading...", getBounds(), juce::Justification::centred);
            } else {
                g.drawText("Hello, " + name, getBounds(), juce::Justification::centred);
            }
        }

    private:
        ObservableProperty<juce::String> username{"Guest"};
        AtomicObservableProperty<bool> isLoading{false};
    };

    // Later, when properties change:
    myComponent.username.set("Alice");  // Automatically triggers repaint()!

Store Subscriptions
~~~~~~~~~~~~~~~~~~~

Components commonly subscribe to stores for centralized state:

.. code-block:: cpp

    class PostsFeed : public Sidechain::Util::ReactiveBoundComponent,
                      public juce::Timer
    {
    public:
        PostsFeed() {
            // Get FeedStore singleton
            feedStore = &FeedStore::getInstance();

            // Subscribe - component will repaint when store state changes
            storeUnsubscribe = feedStore->subscribe([this](const FeedStoreState& state) {
                // No manual repaint() needed - ReactiveBoundComponent handles it!
            });
        }

        ~PostsFeed() override {
            // Cleanup subscription
            if (storeUnsubscribe)
                storeUnsubscribe();
        }

        void paint(juce::Graphics& g) override {
            if (!feedStore)
                return;

            const auto& state = feedStore->getState();
            const auto& currentFeed = state.getCurrentFeed();

            if (currentFeed.isLoading) {
                drawLoadingState(g);
            } else if (!currentFeed.error.isEmpty()) {
                drawErrorState(g, currentFeed.error);
            } else {
                drawPosts(g, currentFeed.posts);
            }
        }

    private:
        FeedStore* feedStore = nullptr;
        std::function<void()> storeUnsubscribe;
    };

Binding Methods
---------------

bindProperty()
~~~~~~~~~~~~~~

Bind a regular ObservableProperty:

.. code-block:: cpp

    ObservableProperty<juce::String> text{"Hello"};
    bindProperty(text);

bindAtomicProperty()
~~~~~~~~~~~~~~~~~~~~

Bind an AtomicObservableProperty (lock-free):

.. code-block:: cpp

    AtomicObservableProperty<bool> enabled{true};
    bindAtomicProperty(enabled);

bindArray()
~~~~~~~~~~~

Bind an ObservableArray with add/remove/change notifications:

.. code-block:: cpp

    ObservableArray<FeedPost> posts;
    bindArray(posts);

    // Component repaints on:
    // - posts.add()
    // - posts.remove()
    // - posts.set()

bindMap()
~~~~~~~~~

Bind an ObservableMap:

.. code-block:: cpp

    ObservableMap<juce::String, User> userCache;
    bindMap(userCache);

computed()
~~~~~~~~~~

Create and bind derived/computed properties:

.. code-block:: cpp

    ObservableProperty<int> age{25};

    // Derived property: birthYear = 2024 - age
    auto birthYear = computed<int, int>(age, [](int a) {
        return 2024 - a;
    });

    // birthYear is automatically bound to component

Lifecycle & Thread Safety
--------------------------

Automatic Cleanup
~~~~~~~~~~~~~~~~~

All bindings are automatically cleaned up when the component is destroyed:

.. code-block:: cpp

    class MyComponent : public ReactiveBoundComponent {
    public:
        MyComponent() {
            bindProperty(prop1);
            bindProperty(prop2);
            bindArray(array1);
            // ... many bindings
        }

        ~MyComponent() override {
            // All bindings automatically unsubscribed via clearBindings()
        }
    };

Message Thread Marshalling
~~~~~~~~~~~~~~~~~~~~~~~~~~~

ReactiveBoundComponent ensures ``repaint()`` is always called on the message thread:

.. code-block:: cpp

    // Property updated from background thread
    Async::run([]() {
        return downloadData();
    }, [this](const Data& data) {
        myProperty.set(data);  // Triggers observer on background thread
    });

    // ReactiveBoundComponent automatically marshalls to message thread:
    void bindProperty(ObservableProperty<T>& property) {
        auto unsubscriber = property.observe([this](const T&) {
            if (auto* mm = juce::MessageManager::getInstanceWithoutCreating()) {
                mm->callAsync([this]() {
                    repaint();  // SAFE - on message thread
                });
            } else {
                repaint();  // Fallback
            }
        });
        propertyUnsubscribers.push_back(unsubscriber);
    }

Real-World Examples
-------------------

PostCard Component
~~~~~~~~~~~~~~~~~~

PostCard displays a single feed post and subscribes to FeedStore for automatic updates:

.. code-block:: cpp

    class PostCard : public Sidechain::Util::ReactiveBoundComponent
    {
    public:
        PostCard() {
            // ... initialization
        }

        void setFeedStore(FeedStore* store) {
            feedStore = store;

            if (feedStore) {
                storeUnsubscribe = feedStore->subscribe([this](const FeedStoreState& state) {
                    // Find updated post data in store
                    const auto& currentFeed = state.getCurrentFeed();
                    for (const auto& p : currentFeed.posts) {
                        if (p.id == post.id) {
                            post = p;  // Update local copy
                            break;     // ReactiveBoundComponent triggers repaint
                        }
                    }
                });
            }
        }

        void paint(juce::Graphics& g) override {
            // Draw post with current data
            drawLikeButton(g, post.likeCount, post.isLiked);
            drawComments(g, post.commentCount);
            // ... etc
        }

    private:
        FeedPost post;
        FeedStore* feedStore = nullptr;
        std::function<void()> storeUnsubscribe;
    };

    // Usage:
    // User clicks like → FeedStore.toggleLike() → State changes →
    // All PostCards with that post ID update automatically

MessageThread Component
~~~~~~~~~~~~~~~~~~~~~~~

MessageThread displays chat messages and typing indicators reactively:

.. code-block:: cpp

    class MessageThread : public Sidechain::Util::ReactiveBoundComponent,
                          public juce::Timer
    {
    public:
        MessageThread() {
            chatStore = &ChatStore::getInstance();

            chatStoreUnsubscribe = chatStore->subscribe([this](const ChatStoreState& state) {
                // Trigger resized() to update scroll bounds
                if (const auto* channel = state.getCurrentChannel()) {
                    juce::MessageManager::callAsync([this]() {
                        resized();
                        // repaint() called automatically by ReactiveBoundComponent
                    });
                }
            });
        }

        void paint(juce::Graphics& g) override {
            if (!chatStore)
                return;

            const auto* channel = chatStore->getState().getCurrentChannel();
            if (!channel)
                return;

            // Draw messages
            for (const auto& message : channel->messages) {
                drawMessageBubble(g, message);
            }

            // Draw typing indicator (real-time from store)
            if (!channel->usersTyping.empty()) {
                juce::String typingText = channel->usersTyping[0] + " is typing...";
                g.drawText(typingText, typingBounds, juce::Justification::centred);
            }
        }

    private:
        ChatStore* chatStore = nullptr;
        std::function<void()> chatStoreUnsubscribe;
    };

    // Typing indicators update automatically via ChatStore.usersTyping!

Performance Considerations
--------------------------

Repaint Throttling
~~~~~~~~~~~~~~~~~~

ReactiveBoundComponent doesn't throttle repaints automatically. For high-frequency updates,
consider throttling in the store or using a timer:

.. code-block:: cpp

    class MyComponent : public ReactiveBoundComponent, public juce::Timer {
    public:
        MyComponent() {
            bindProperty(highFrequencyProp);
            startTimer(16);  // ~60fps
        }

        void timerCallback() override {
            // Only repaint on timer, not on every property change
            // (subscribers still fire, but we batch repaints)
        }

    private:
        // Override to prevent automatic repaint
        void bindProperty(ObservableProperty<T>& property) {
            auto unsubscriber = property.observe([this](const T&) {
                isDirty = true;  // Mark dirty instead of repaint
            });
            propertyUnsubscribers.push_back(unsubscriber);
        }

        bool isDirty = false;
    };

Selective Updates
~~~~~~~~~~~~~~~~~

For large components, update only changed regions:

.. code-block:: cpp

    void paint(juce::Graphics& g) override {
        const auto& state = store->getState();

        if (state.headerChanged) {
            g.saveState();
            g.reduceClipRegion(headerBounds);
            drawHeader(g);
            g.restoreState();
        }

        if (state.contentChanged) {
            g.saveState();
            g.reduceClipRegion(contentBounds);
            drawContent(g);
            g.restoreState();
        }
    }

Best Practices
--------------

1. **Subscribe in Constructor, Unsubscribe in Destructor**

   .. code-block:: cpp

       MyComponent() {
           unsubscribe = store->subscribe([this](const auto& state) {
               // Handle changes
           });
       }

       ~MyComponent() override {
           if (unsubscribe)
               unsubscribe();
       }

2. **Don't Store Derived State**

   Always compute from store state in ``paint()``:

   .. code-block:: cpp

       // BAD: Derived state can get out of sync
       int cachedLikeCount = 0;

       void onStoreChange(const FeedStoreState& state) {
           cachedLikeCount = state.getCurrentFeed().posts.size();
       }

       // GOOD: Compute in paint()
       void paint(juce::Graphics& g) override {
           int likeCount = feedStore->getState().getCurrentFeed().posts.size();
           g.drawText(juce::String(likeCount), bounds, juce::Justification::centred);
       }

3. **Avoid Heavy Work in Subscriptions**

   Subscriptions should trigger UI updates only. Heavy work should be in stores/services:

   .. code-block:: cpp

       // BAD: Heavy work in subscription
       store->subscribe([this](const auto& state) {
           performExpensiveCalculation(state.data);  // Blocks UI!
           repaint();
       });

       // GOOD: Heavy work in store action
       void Store::loadData() {
           Async::run([]() {
               return performExpensiveCalculation();
           }, [this](const auto& result) {
               setState({result});  // Light state update
           });
       }

4. **Use Atomic Properties for Audio Thread**

   Never bind regular properties that are read on audio thread:

   .. code-block:: cpp

       // BAD: Mutex lock in audio thread
       ObservableProperty<float> gain{1.0f};
       bindProperty(gain);

       void processBlock(AudioBuffer<float>& buffer) {
           buffer.applyGain(gain.get());  // DEADLOCK RISK!
       }

       // GOOD: Atomic property for audio thread
       AtomicObservableProperty<float> gain{1.0f};
       bindAtomicProperty(gain);

       void processBlock(AudioBuffer<float>& buffer) {
           buffer.applyGain(gain.get());  // Lock-free!
       }

Migration Guide
---------------

From Manual Repaint to Reactive
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before (manual state management):

.. code-block:: cpp

    class OldComponent : public juce::Component {
    public:
        void setData(const Data& newData) {
            data = newData;
            repaint();  // Manual repaint
        }

        void paint(juce::Graphics& g) override {
            g.drawText(data.text, getBounds(), juce::Justification::centred);
        }

    private:
        Data data;
    };

After (reactive):

.. code-block:: cpp

    class NewComponent : public ReactiveBoundComponent {
    public:
        NewComponent() {
            bindProperty(data);  // Automatic repaint on change
        }

        void setData(const Data& newData) {
            data.set(newData);  // Triggers repaint automatically
        }

        void paint(juce::Graphics& g) override {
            g.drawText(data.get().text, getBounds(), juce::Justification::centred);
        }

    private:
        ObservableProperty<Data> data;
    };

From Callbacks to Store Subscriptions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before (callback hell):

.. code-block:: cpp

    class OldComponent : public juce::Component {
    public:
        void loadData() {
            networkClient->fetchData([this](const Data& data) {
                this->data = data;
                repaint();
            });
        }

    private:
        Data data;
        NetworkClient* networkClient;
    };

After (reactive store):

.. code-block:: cpp

    class NewComponent : public ReactiveBoundComponent {
    public:
        NewComponent() {
            store = &DataStore::getInstance();
            unsubscribe = store->subscribe([this](const DataStoreState& state) {
                // Automatic repaint via ReactiveBoundComponent
            });
        }

        void loadData() {
            store->loadData();  // Store handles async, notifies on complete
        }

        void paint(juce::Graphics& g) override {
            const auto& state = store->getState();
            if (state.isLoading) {
                drawSpinner(g);
            } else {
                drawData(g, state.data);
            }
        }

    private:
        DataStore* store = nullptr;
        std::function<void()> unsubscribe;
    };

See Also
--------

* :doc:`stores` - Store pattern and state management
* :doc:`observables` - Observable properties and collections
* :doc:`data-flow` - Complete data flow examples with components
* :doc:`threading` - Thread safety and message thread marshalling
