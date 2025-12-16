.. _architecture-guide:

Complete Architecture Guide
============================

**Last Updated**: December 15, 2024

**Target Audience**: Developers, architects, new team members

**Scope**: Modern C++26 architecture, reactive patterns, state management, threading model

This is the comprehensive architecture guide for the Sidechain VST Plugin. It covers everything from high-level principles to practical implementation patterns.

Quick Start (5 Minutes)
=======================

Your First Store Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Want to understand the modern architecture immediately? Here's the complete flow:

**Step 1: Subscribe to a Store**

.. code-block:: cpp

    class MyFeedComponent : public ReactiveBoundComponent {
    private:
        std::shared_ptr<PostsStore> postsStore;
        std::function<void()> unsubscribe;

    public:
        void setPostsStore(std::shared_ptr<PostsStore> store) {
            postsStore = store;
            // Subscribe: This callback runs whenever state changes
            unsubscribe = postsStore->subscribe([this](const PostsState& state) {
                displayPosts(state.getCurrentFeed().posts);
                if (state.getCurrentFeed().isLoading) {
                    showLoadingSpinner();
                }
                repaint();
            });
        }

        ~MyFeedComponent() override {
            if (unsubscribe) unsubscribe();  // Always cleanup!
        }
    };

**Step 2: Trigger Actions via Store Methods**

.. code-block:: cpp

    void MyFeedComponent::onLikeButtonClicked(const FeedPost& post) {
        // Call store method - it handles everything:
        // 1. Optimistic update (UI changes immediately)
        // 2. Server sync (network call happens async)
        // 3. Error handling (automatic rollback if needed)
        postsStore->toggleLike(post.id);
        // The store will notify all subscribers, UI updates automatically
    }

**Step 3: Store Updates Happen Automatically**

.. code-block:: cpp

    // Inside PostsStore (implementation detail you don't need to write)
    void PostsStore::toggleLike(const juce::String& postId) {
        // 1. Optimistic update
        auto state = getState();
        if (auto* post = findPost(postId, state)) {
            post->isLiked = !post->isLiked;
            post->likeCount += post->isLiked ? 1 : -1;
        }
        setState(state);  // Notifies all subscribers (including MyFeedComponent!)

        // 2. Server sync (async, doesn't block UI)
        networkClient->likePost(postId, [this, postId](auto result) {
            if (!result.isOk()) {
                toggleLike(postId);  // Rollback if failed
            }
        });
    }

**That's it!** Your UI automatically updates because:

1. Store state changed → ``setState()`` called
2. All subscribers notified → subscription callback fires
3. Your component re-paints with new data

**No manual state management. No callback chains. Just reactive magic.** ✨

Core Principles
===============

The Sidechain plugin architecture is built on six core principles:

1. **Single Source of Truth**
    - One store per entity type (PostsStore, UserStore, ChatStore, etc.)
    - Not separate stores for different views of the same data
    - Prevents state desynchronization

2. **Reactive Updates**
    - Components subscribe to stores
    - Automatically re-render on state changes
    - No manual UI updates needed

3. **Immutable State**
    - State objects are copied and replaced, never mutated
    - Ensures predictability and thread safety

4. **Optimistic Updates**
    - UI updates immediately for instant feedback
    - Server sync happens asynchronously in background
    - Automatic rollback if server rejects

5. **Error Handling**
    - Errors stored in state, not thrown as exceptions
    - UI displays error messages from state
    - Automatic retry logic built into stores

6. **Type Safety**
    - Full C++26 typing throughout
    - Smart pointers (no raw ``new``/``delete``)
    - Strong-typed state structs

Architecture Layers
===================

The application follows a layered architecture:

.. code-block:: text

    ┌─────────────────────────────────────────┐
    │       UI Components                     │
    │   (PostsFeed, Profile, etc.)            │
    │   (ReactiveBoundComponent subclasses)   │
    └────────────────┬────────────────────────┘
                     │ subscribe to
                     ▼
    ┌─────────────────────────────────────────┐
    │       Stores (State Management)          │
    │   (PostsStore, UserStore, ChatStore)    │
    │   (Store<T> + Reactive Bindings)        │
    └────────────────┬────────────────────────┘
                     │ read/write via
                     ▼
    ┌─────────────────────────────────────────┐
    │       NetworkClient                     │
    │   (HTTP + WebSocket)                    │
    │   (Rate limiting, error tracking)       │
    └────────────────┬────────────────────────┘
                     │ sends/receives
                     ▼
    ┌─────────────────────────────────────────┐
    │       Backend API                       │
    │   (Go server + getstream.io)            │
    │   (REST endpoints + WebSocket)          │
    └─────────────────────────────────────────┘

The 9 Core Stores
=================

The application uses 9 centralized stores for entity management:

.. list-table::
   :header-rows: 1
   :widths: 20, 40, 40

   * - Store
     - Purpose
     - Key State
   * - **PostsStore**
     - All posts: feeds, saved, archived
     - ``PostsState``
   * - **UserStore**
     - All users: current, cached, discovery
     - ``UserState``
   * - **ChatStore**
     - Messages and channels
     - ``ChatStoreState``
   * - **DraftStore**
     - Draft posts (singleton)
     - ``DraftState[]``
   * - **CommentStore**
     - Comments on posts
     - ``CommentState``
   * - **NotificationStore**
     - Notifications and unread counts
     - ``NotificationState``
   * - **StoriesStore**
     - Stories and highlights
     - ``StoriesState``
   * - **UploadStore**
     - Upload progress and state
     - ``UploadState``
   * - **FollowersStore**
     - Follower/following lists
     - ``FollowListState``

See :doc:`stores` for detailed documentation on the store pattern and how each store works.

Observable Collections
======================

The architecture uses reactive collections for fine-grained updates:

**ObservableProperty<T>**: Reactive wrapper for single values

.. code-block:: cpp

    auto property = std::make_shared<ObservableProperty<int>>(0);

    // Subscribe to changes
    auto unsubscribe = property->subscribe([](int newValue) {
        std::cout << "Value changed to: " << newValue << std::endl;
    });

    // Setting value notifies subscribers
    property->setValue(42);

**ObservableArray<T>**: Reactive array with batch notifications

.. code-block:: cpp

    auto posts = std::make_shared<ObservableArray<FeedPost>>();

    // Subscribe to array changes
    posts->subscribe([](const ArrayChangeEvent<FeedPost>& event) {
        switch (event.type) {
            case ChangeType::ItemAdded:
                std::cout << "Post added at index " << event.index << std::endl;
                break;
            case ChangeType::BatchUpdate:
                std::cout << "Batch update with " << event.items.size() << " items" << std::endl;
                break;
        }
    });

**ObservableMap<K, V>**: Reactive key-value store

.. code-block:: cpp

    auto userCache = std::make_shared<ObservableMap<juce::String, User>>();

    userCache->subscribe([](const MapChangeEvent<juce::String, User>& event) {
        if (event.type == ChangeType::ItemAdded) {
            std::cout << "User " << event.key << " cached" << std::endl;
        }
    });

See :doc:`observables` for detailed documentation on reactive collections.

Data Flow Patterns
==================

Feed Loading Flow
~~~~~~~~~~~~~~~~~

The feed loading follows this flow:

.. code-block:: text

    UI calls loadFeed()
           ↓
    PostsStore marks isLoading=true
    Notifies subscribers
           ↓
    UI shows loading spinner

    [Meanwhile, async...]

    NetworkClient fetches from server
           ↓
    Server responds with posts
           ↓
    PostsStore updates posts array
    Sets isLoading=false
    Notifies subscribers
           ↓
    UI renders posts

Optimistic Update Flow
~~~~~~~~~~~~~~~~~~~~~~

User interactions use optimistic updates:

.. code-block:: text

    User clicks "Like"
           ↓
    Store updates immediately:
    - isLiked = true
    - likeCount++
    Notifies subscribers
           ↓
    UI updates instantly

    [Meanwhile, async...]

    NetworkClient sends to server
           ↓
    If server fails:
      Rollback by toggling again
    If server succeeds:
      State persists

See :doc:`data-flow` for detailed data flow diagrams and patterns.

Threading Model
===============

The plugin uses a multi-threaded architecture with strict constraints:

Audio Thread (Real-Time, Lock-Free)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

✅ **Allowed**:

* Read from ``atomic<>`` variables
* Pre-allocated buffers
* Ring buffers for lock-free IPC

❌ **Forbidden**:

* Any locks (mutex, semaphore)
* Memory allocation (``new``, ``malloc``)
* File I/O
* Network calls
* Anything that could block

Message Thread (UI Thread, JUCE)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

✅ **Allowed**:

* Update components
* Call ``repaint()``
* Synchronous operations

❌ **Forbidden**:

* Network calls (use ``Async``)
* File I/O (use ``Async``)
* Long computations

Background Threads (Network, File I/O)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

✅ **Allowed**:

* Network requests
* File I/O
* Long computations

❌ **Forbidden**:

* Direct UI updates (use ``callAsync``)
* Audio buffer manipulation

Async Pattern
~~~~~~~~~~~~~

All network operations use the ``Async`` helper:

.. code-block:: cpp

    // Background work → message thread callback
    Async::run<juce::Image>(
        // Background work
        [this]() {
            return downloadImage(url);  // On background thread
        },
        // UI callback
        [this](const auto& image) {
            setImage(image);  // On message thread
            repaint();
        }
    );

See :doc:`threading` for detailed threading constraints and patterns.

Component Patterns
==================

Container/Presentation Pattern
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Components use a two-tier pattern:

**Container Component** (Smart, manages state):

.. code-block:: cpp

    class PostsFeed : public ReactiveBoundComponent {
    private:
        std::shared_ptr<PostsStore> postsStore;
        std::function<void()> unsubscribe;

    public:
        void setPostsStore(std::shared_ptr<PostsStore> store) {
            postsStore = store;
            unsubscribe = postsStore->subscribe([this](const PostsState& state) {
                updatePostsList(state.getCurrentFeed().posts);
                repaint();
            });
        }

        void onPostCardLikeClicked(const FeedPost& post) {
            postsStore->toggleLike(post.id);  // Store handles everything
        }

        ~PostsFeed() override {
            if (unsubscribe) unsubscribe();
        }
    };

**Presentation Component** (Dumb, receives props):

.. code-block:: cpp

    class PostCard : public Component {
    private:
        FeedPost post;  // Received as prop, not managed

    public:
        void setPost(const FeedPost& newPost) {
            post = newPost;
            repaint();
        }

        // Callbacks bubble up to parent
        std::function<void(const FeedPost&)> onLikeClicked;
        std::function<void(const FeedPost&)> onCommentClicked;
    };

See :doc:`reactive-components` for detailed component patterns.

Testing Strategies
==================

Unit Testing Stores
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    class PostsStoreTest : public juce::UnitTest {
    public:
        void runTest() override {
            // Setup: Create store with mock client
            auto mockClient = std::make_shared<MockNetworkClient>();
            auto store = std::make_shared<PostsStore>(mockClient.get());

            // Record state changes
            juce::Array<PostsState> stateHistory;
            store->subscribe([&](const PostsState& state) {
                stateHistory.add(state);
            });

            // Act: Trigger action
            store->toggleLike("post-123");

            // Assert: Check state changed correctly
            expect(stateHistory.size() > 0);
            expect(stateHistory.back().getCurrentFeed().posts[0].isLiked == true);
        }
    };

Common Best Practices
=====================

Immutable Updates
~~~~~~~~~~~~~~~~~

✅ **Good**: Create new state, replace entirely

.. code-block:: cpp

    {
        auto state = getState();  // Copy
        state.posts.add(newPost);  // Modify copy
        setState(state);  // Replace entire state
    }

❌ **Bad**: Mutate state directly

.. code-block:: cpp

    {
        auto& state = getState();  // Direct reference (wrong!)
        state.posts.add(newPost);  // Mutating original (wrong!)
    }

Error Handling
~~~~~~~~~~~~~~

Store errors in state, don't throw:

.. code-block:: cpp

    {
        auto state = getState();
        state.error = "Failed to load feed";
        state.isLoading = false;
        setState(state);
    }

UI subscribes and displays error:

.. code-block:: cpp

    store->subscribe([this](const PostsState& state) {
        if (!state.error.isEmpty()) {
            showErrorNotification(state.error);
        }
    });

Subscription Cleanup
~~~~~~~~~~~~~~~~~~~~

Always cleanup in destructor:

.. code-block:: cpp

    class MyComponent : public Component {
    private:
        std::function<void()> unsubscribe;

    public:
        void bindToStore(std::shared_ptr<PostsStore> store) {
            unsubscribe = store->subscribe([this](const PostsState& state) {
                // Handle state changes
            });
        }

        ~MyComponent() override {
            if (unsubscribe) {
                unsubscribe();
            }
        }
    };

Troubleshooting
===============

Component Not Updating When Store Changes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Problem**: Subscribe to store but UI doesn't update

**Causes**:

1. Didn't save unsubscribe function
2. Component was deleted before state changed
3. Not calling ``repaint()`` in subscription

**Fix**:

.. code-block:: cpp

    auto unsubscribe = store->subscribe([this](const PostsState& state) {
        updateUI(state);
        repaint();  // Important!
    });
    // ... later ...
    if (unsubscribe) unsubscribe();  // Cleanup!

State Changes But Nobody Notified
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Problem**: Called ``setState()`` but subscribers didn't get called

**Causes**:

1. Mutated state instead of creating new one
2. Comparing same object (not deep copy)

**Fix**:

.. code-block:: cpp

    // Create copy before modifying
    auto state = getState();  // Makes copy
    state.posts.add(newPost);  // Modify copy
    setState(state);  // Notify subscribers

Memory Leaks from Store Subscriptions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Problem**: Component destroyed but never unsubscribed

**Fix**:

.. code-block:: cpp

    class MyComponent {
    private:
        std::function<void()> unsubscribe;

    public:
        ~MyComponent() override {
            // Call in destructor!
            if (unsubscribe) unsubscribe();
        }
    };

See Also
========

* :doc:`stores` - Store pattern and all 9 stores
* :doc:`observables` - Observable collections reference
* :doc:`reactive-components` - Component integration patterns
* :doc:`data-flow` - Data flow diagrams and patterns
* :doc:`threading` - Threading model and constraints
* :doc:`services` - Business logic layer
* `/MODERNIZATION_EVALUATION_REPORT.md <../../notes/MODERNIZATION_EVALUATION_REPORT.md>`_ - Complete project assessment
* `/CLAUDE.md <../../CLAUDE.md>`_ - Development commands and guidelines
