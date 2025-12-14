.. _stores:

Store Pattern
=============

The Store pattern provides centralized, reactive state management with a single source of truth
for application state. All stores inherit from ``Store<TState>`` and follow Redux-like patterns.

Core Concepts
-------------

Stores are singleton classes that:

* Hold application state in a typed state struct
* Provide ``subscribe()`` for reactive updates
* Expose state via ``getState()``
* Update state through action methods
* Notify subscribers automatically on state changes

Available Stores
----------------

FeedStore
~~~~~~~~~

Manages the social feed state including posts, pagination, and real-time updates.

**Location**: ``plugin/src/stores/FeedStore.h``

**State Structure**:

.. code-block:: cpp

    struct FeedStoreState {
        // Feed data by type
        std::map<FeedType, FeedState> feeds;
        FeedType currentFeedType = FeedType::Timeline;

        // Accessors
        const FeedState& getCurrentFeed() const;
        FeedType getCurrentFeedType() const;
    };

    struct FeedState {
        std::vector<FeedPost> posts;
        bool isLoading = false;
        bool isRefreshing = false;
        bool hasMore = true;
        juce::String error;
        juce::String cursor;  // Pagination cursor
    };

**Key Methods**:

.. code-block:: cpp

    // Singleton access
    static FeedStore& getInstance();

    // Feed operations
    void loadFeed(FeedType type, bool refresh = false);
    void switchFeedType(FeedType type);
    void refreshCurrentFeed();
    void loadMore();

    // Post interactions
    void toggleLike(const juce::String& postId);
    void toggleSave(const juce::String& postId);
    void toggleRepost(const juce::String& postId);
    void addEmojiReaction(const juce::String& postId, const juce::String& emoji);
    void toggleFollow(const juce::String& userId);
    void togglePin(const juce::String& postId);

    // Reactive subscription
    std::function<void()> subscribe(
        std::function<void(const FeedStoreState&)> listener
    );

**Usage Example**:

.. code-block:: cpp

    // Get singleton instance
    auto& feedStore = FeedStore::getInstance();
    feedStore.setNetworkClient(networkClient);

    // Subscribe to state changes
    auto unsubscribe = feedStore.subscribe([this](const FeedStoreState& state) {
        const auto& currentFeed = state.getCurrentFeed();

        if (currentFeed.isLoading) {
            showLoadingState();
        } else if (!currentFeed.error.isEmpty()) {
            showError(currentFeed.error);
        } else {
            displayPosts(currentFeed.posts);
        }

        // Component automatically repaints (via ReactiveBoundComponent)
    });

    // Load feed
    feedStore.loadFeed(FeedType::Timeline);

    // User interaction
    feedStore.toggleLike("post-123");
    // State updates → subscribers notified → UI updates automatically

ChatStore
~~~~~~~~~

Manages messaging/chat state including channels, messages, and typing indicators.

**Location**: ``plugin/src/stores/ChatStore.h``

**State Structure**:

.. code-block:: cpp

    struct ChatStoreState {
        // Channels mapped by ID
        std::map<juce::String, ChannelState> channels;
        std::vector<juce::String> channelOrder;
        juce::String currentChannelId;

        // Connection status
        bool isLoadingChannels = false;
        bool isConnecting = false;
        StreamChatClient::ConnectionStatus connectionStatus;

        // Authentication
        bool isAuthenticated = false;
        juce::String userId;

        // Accessor
        const ChannelState* getCurrentChannel() const;
    };

    struct ChannelState {
        juce::String id;
        juce::String name;
        std::vector<StreamChatClient::Message> messages;
        std::vector<juce::String> usersTyping;
        bool isLoadingMessages = false;
        int unreadCount = 0;
    };

**Key Methods**:

.. code-block:: cpp

    static ChatStore& getInstance();

    // Channel operations
    void loadChannels();
    void selectChannel(const juce::String& channelId);
    void loadMessages(const juce::String& channelId, int limit = 50);

    // Messaging
    void sendMessage(const juce::String& channelId, const juce::String& text);
    void startTyping(const juce::String& channelId);
    void stopTyping(const juce::String& channelId);

    // Subscription
    std::function<void()> subscribe(
        std::function<void(const ChatStoreState&)> listener
    );

**Usage Example**:

.. code-block:: cpp

    auto& chatStore = ChatStore::getInstance();
    chatStore.setStreamChatClient(streamChatClient);

    auto unsubscribe = chatStore.subscribe([this](const ChatStoreState& state) {
        if (const auto* channel = state.getCurrentChannel()) {
            displayMessages(channel->messages);

            // Typing indicators update automatically
            if (!channel->usersTyping.empty()) {
                showTypingIndicator(channel->usersTyping[0]);
            }
        }
    });

    chatStore.selectChannel("channel-abc");
    chatStore.loadMessages("channel-abc", 50);

UserStore
~~~~~~~~~

Manages user profile state, authentication, and user-related operations.

**Location**: ``plugin/src/stores/UserStore.h``

**State Structure**:

.. code-block:: cpp

    struct UserStoreState {
        // Current user
        User currentUser;
        bool isAuthenticated = false;

        // Cached users (for profiles, mentions, etc.)
        std::map<juce::String, User> userCache;

        // Loading states
        bool isLoadingProfile = false;
        juce::String error;
    };

**Key Methods**:

.. code-block:: cpp

    static UserStore& getInstance();

    // Authentication
    void login(const juce::String& email, const juce::String& password);
    void logout();

    // Profile operations
    void loadProfile(const juce::String& userId);
    void updateProfile(const User& updates);

    // User cache
    const User* getCachedUser(const juce::String& userId) const;

Store Base Class
----------------

All stores inherit from ``Store<TState>`` which provides the reactive foundation.

**Location**: ``plugin/src/stores/Store.h``

**Interface**:

.. code-block:: cpp

    template<typename TState>
    class Store {
    public:
        // Get current state (read-only)
        const TState& getState() const;

        // Subscribe to state changes
        std::function<void()> subscribe(
            std::function<void(const TState&)> listener
        );

    protected:
        // Update state and notify subscribers
        void setState(const TState& newState);

    private:
        TState state;
        std::vector<std::function<void(const TState&)>> subscribers;
    };

Subscription Lifecycle
----------------------

Subscriptions return an unsubscribe function for cleanup:

.. code-block:: cpp

    class MyComponent : public ReactiveBoundComponent {
    public:
        MyComponent() {
            // Subscribe in constructor
            unsubscribe = feedStore->subscribe([this](const auto& state) {
                // Handle state changes
            });
        }

        ~MyComponent() override {
            // Unsubscribe in destructor
            if (unsubscribe)
                unsubscribe();
        }

    private:
        std::function<void()> unsubscribe;
    };

Best Practices
--------------

1. **Single Source of Truth**

   Never duplicate store state in components. Always read from ``getState()``.

2. **Immutable Updates**

   Create new state objects rather than mutating existing state:

   .. code-block:: cpp

       // Good
       auto newState = state;
       newState.posts.push_back(newPost);
       setState(newState);

       // Bad
       state.posts.push_back(newPost);  // Mutating state directly

3. **Atomic Operations**

   Keep state updates atomic - one logical change per setState() call.

4. **Error Handling**

   Store errors in state rather than throwing:

   .. code-block:: cpp

       auto newState = state;
       newState.error = "Failed to load feed";
       newState.isLoading = false;
       setState(newState);

5. **Network Calls in Actions**

   Action methods should handle async operations:

   .. code-block:: cpp

       void FeedStore::loadFeed(FeedType type) {
           // Set loading state
           auto newState = state;
           newState.feeds[type].isLoading = true;
           setState(newState);

           // Async network call
           networkClient->getFeed(type, [this, type](Outcome<FeedResponse> result) {
               if (result.isOk()) {
                   handleFeedLoaded(type, result.getValue());
               } else {
                   handleFeedError(type, result.getError());
               }
           });
       }

Thread Safety
-------------

Stores are accessed from the JUCE message thread:

* All ``setState()`` calls must be on message thread
* Network callbacks are marshalled to message thread automatically
* Subscribers are called on message thread
* Components receive updates on message thread (safe for UI)

See Also
--------

* :doc:`observables` - Property-level reactive bindings
* :doc:`reactive-components` - UI component integration
* :doc:`data-flow` - Complete data flow examples
* :doc:`threading` - Threading model and constraints
