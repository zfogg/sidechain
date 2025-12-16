.. _stores:

Store Pattern
=============

The Store pattern provides centralized, reactive state management with a single source of truth
for application state. All stores inherit from ``Store<TState>`` and follow Redux-like patterns.

Core Concepts
-------------

Stores are singleton or shared-instance classes that:

* Hold application state in a typed state struct
* Provide ``subscribe()`` for reactive updates
* Expose state via ``getState()``
* Update state through action methods
* Notify subscribers automatically on state changes

Store Architecture
------------------

The application uses a **consolidated store architecture** with entity-based stores:

.. code-block:: text

    ┌─────────────────────────────────────────────────────────────┐
    │                     Entity Stores                            │
    ├─────────────────────────────────────────────────────────────┤
    │  PostsStore        │ All posts: feeds, saved, archived      │
    │  UserStore         │ All users: current + cached + discovery│
    │  ChatStore         │ Channels & messages                    │
    ├─────────────────────────────────────────────────────────────┤
    │                     Domain Stores                            │
    ├─────────────────────────────────────────────────────────────┤
    │  NotificationStore │ Notifications                          │
    │  StoriesStore      │ Stories & highlights                   │
    │  CommentStore      │ Comments on posts                      │
    │  DraftStore        │ Draft posts                            │
    │  UploadStore       │ Upload state                           │
    │  ChallengeStore    │ MIDI challenges                        │
    │  PlaylistStore     │ Playlists                              │
    │  FollowersStore    │ Followers/following lists              │
    └─────────────────────────────────────────────────────────────┘

**Design Principle**: One store per entity type. Avoid creating separate stores for
different views of the same entity (e.g., don't have separate SavedPostsStore,
ArchivedPostsStore - use PostsStore for all post collections).

Entity Stores
-------------

PostsStore
~~~~~~~~~~

Manages all post collections: feeds, saved posts, archived posts, and aggregated feeds.

**Location**: ``plugin/src/stores/PostsStore.h``

**State Structure**:

.. code-block:: cpp

    struct PostsState {
        // Feed collections (multiple feed types)
        std::map<FeedType, FeedState> feeds;
        std::map<FeedType, AggregatedFeedState> aggregatedFeeds;
        FeedType currentFeedType = FeedType::Timeline;

        // User post collections
        SavedPostsState savedPosts;
        ArchivedPostsState archivedPosts;

        // Global state
        juce::String errorMessage;
        int64_t lastUpdated = 0;

        // Convenience accessors
        const FeedState& getCurrentFeed() const;
        const AggregatedFeedState& getCurrentAggregatedFeed() const;
    };

    enum class FeedType {
        Timeline,        // User's following feed
        Global,          // Global discover feed
        Trending,        // Trending feed
        ForYou,          // Personalized recommendations
        Popular,         // Popular posts (from Gorse)
        Latest,          // Latest posts (from Gorse)
        Discovery,       // Discovery feed
        // Aggregated variants
        TimelineAggregated,
        TrendingAggregated,
        NotificationAggregated,
        UserActivityAggregated
    };

**Key Methods**:

.. code-block:: cpp

    explicit PostsStore(NetworkClient* client);

    // Feed operations
    void loadFeed(FeedType feedType, bool forceRefresh = false);
    void refreshCurrentFeed();
    void loadMore();
    void switchFeedType(FeedType feedType);

    // Post interactions (optimistic updates)
    void toggleLike(const juce::String& postId);
    void toggleSave(const juce::String& postId);
    void toggleRepost(const juce::String& postId);
    void addReaction(const juce::String& postId, const juce::String& emoji);
    void toggleFollow(const juce::String& postId, bool willFollow);
    void toggleArchive(const juce::String& postId, bool archived);
    void togglePin(const juce::String& postId, bool pinned);

    // Saved posts management
    void loadSavedPosts();
    void loadMoreSavedPosts();
    void unsavePost(const juce::String& postId);

    // Archived posts management
    void loadArchivedPosts();
    void loadMoreArchivedPosts();
    void restorePost(const juce::String& postId);

    // Cache management
    void clearCache();
    void clearCache(FeedType feedType);

    // Real-time sync
    void enableRealtimeSync();
    void handleNewPostNotification(const juce::var& postData);

**Usage Example**:

.. code-block:: cpp

    auto postsStore = std::make_shared<PostsStore>(networkClient);

    // Subscribe to state changes
    auto unsubscribe = postsStore->subscribe([this](const PostsState& state) {
        const auto& currentFeed = state.getCurrentFeed();

        if (currentFeed.isLoading) {
            showLoadingState();
        } else if (!currentFeed.error.isEmpty()) {
            showError(currentFeed.error);
        } else {
            displayPosts(currentFeed.posts);
        }

        // Also update saved posts badge
        updateSavedCount(state.savedPosts.totalCount);
    });

    // Load timeline
    postsStore->loadFeed(FeedType::Timeline);

    // User interaction - optimistic update
    postsStore->toggleLike("post-123");
    // State updates → subscribers notified → UI updates automatically

UserStore
~~~~~~~~~

Manages all user data: current logged-in user, cached users, and user discovery.

**Location**: ``plugin/src/stores/UserStore.h``

**State Structure**:

.. code-block:: cpp

    struct UserState {
        // Current user identity
        juce::String userId;
        juce::String username;
        juce::String email;
        juce::String displayName;
        juce::String bio;
        juce::String profilePictureUrl;
        juce::Image profileImage;
        juce::String authToken;

        // Social metrics
        int followerCount = 0;
        int followingCount = 0;
        int postCount = 0;

        // Cached users (for profiles, mentions, cards)
        std::map<juce::String, CachedUser> userCache;

        // Discovery sections
        juce::Array<DiscoveredUser> trendingUsers;
        juce::Array<DiscoveredUser> featuredProducers;
        juce::Array<DiscoveredUser> suggestedUsers;
        juce::Array<DiscoveredUser> recommendedToFollow;

        // State flags
        bool isLoggedIn = false;
        bool isFetchingProfile = false;
        juce::String error;
    };

**Key Methods**:

.. code-block:: cpp

    static UserStore& getInstance();  // Singleton

    // Authentication
    void setAuthToken(const juce::String& token);
    void clearAuthToken();
    bool isLoggedIn() const;

    // Profile operations
    void fetchUserProfile(bool forceRefresh = false);
    void updateProfile(const juce::String& username, const juce::String& bio);
    void updateProfileComplete(/* all fields */);
    void uploadProfilePicture(const juce::File& imageFile);

    // User cache operations
    const CachedUser* getCachedUser(const juce::String& userId) const;
    void cacheUser(const CachedUser& user);
    void loadUserProfile(const juce::String& userId);

    // User discovery
    void loadDiscoveryData();
    void loadTrendingUsers();
    void loadFeaturedProducers();
    void loadSuggestedUsers();
    void loadRecommendedToFollow();

    // Social actions
    void followUser(const juce::String& userId);
    void unfollowUser(const juce::String& userId);
    void blockUser(const juce::String& userId);
    void muteUser(const juce::String& userId);

**Usage Example**:

.. code-block:: cpp

    auto& userStore = UserStore::getInstance();
    userStore.setNetworkClient(networkClient);

    auto unsubscribe = userStore.subscribe([this](const UserState& state) {
        if (state.isLoggedIn) {
            displayUserInfo(state.username, state.displayName);
            if (state.profileImage.isValid()) {
                displayAvatar(state.profileImage);
            }
        }

        // Discovery sections
        displayTrendingUsers(state.trendingUsers);
        displaySuggested(state.suggestedUsers);
    });

    // Load current user profile
    userStore.setAuthToken(savedToken);

    // Load discovery data
    userStore.loadDiscoveryData();

    // View another user's profile (cached)
    if (auto* user = userStore.getCachedUser("user-456")) {
        displayProfile(*user);
    } else {
        userStore.loadUserProfile("user-456");
    }

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

    // Real-time events
    void handleNewMessage(const StreamChatClient::Message& message);
    void handleTypingStart(const juce::String& userId);

Domain Stores
-------------

NotificationStore
~~~~~~~~~~~~~~~~~

Manages notification state and unread counts.

**Location**: ``plugin/src/stores/NotificationStore.h``

.. code-block:: cpp

    struct NotificationState {
        juce::Array<Notification> notifications;
        int unreadCount = 0;
        bool isLoading = false;
        juce::String error;
    };

    // Key methods
    void loadNotifications();
    void markAsRead(const juce::String& notificationId);
    void markAllRead();

StoriesStore
~~~~~~~~~~~~

Manages stories feed, user stories, and highlights.

**Location**: ``plugin/src/stores/StoriesStore.h``

.. code-block:: cpp

    struct StoriesState {
        juce::Array<UserStoriesGroup> feedUserStories;
        juce::Array<StoryData> myStories;
        juce::Array<Highlight> highlights;
        bool isFeedLoading = false;
        bool isMyStoriesLoading = false;
    };

    // Key methods
    void loadStoriesFeed();
    void loadMyStories();
    void markStoryAsViewed(const juce::String& storyId);
    void deleteStory(const juce::String& storyId);

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
        // Returns unsubscribe function
        std::function<void()> subscribe(
            std::function<void(const TState&)> listener
        );

    protected:
        // Update state and notify subscribers
        void setState(const TState& newState);

    private:
        TState state;
        std::vector<std::function<void(const TState&)>> subscribers;
        std::mutex mutex;  // Thread safety
    };

Subscription Lifecycle
----------------------

Subscriptions return an unsubscribe function for cleanup:

.. code-block:: cpp

    class MyComponent : public juce::Component {
    public:
        MyComponent() {
            // Subscribe in constructor
            unsubscribe = postsStore->subscribe([this](const auto& state) {
                // Handle state changes
                repaint();
            });
        }

        ~MyComponent() override {
            // IMPORTANT: Unsubscribe in destructor
            if (unsubscribe)
                unsubscribe();
        }

    private:
        std::function<void()> unsubscribe;
    };

**Using SafePointer for async safety**:

.. code-block:: cpp

    void MyComponent::bindToStore(std::shared_ptr<PostsStore> store) {
        postsStore = store;

        // Use SafePointer to handle component deletion during async callbacks
        auto safeThis = juce::Component::SafePointer<MyComponent>(this);

        storeUnsubscriber = postsStore->subscribe([safeThis](const PostsState& state) {
            if (safeThis) {  // Check if component still exists
                safeThis->handleStateChanged(state);
            }
        });
    }

Best Practices
--------------

1. **Single Source of Truth**

   Never duplicate store state in components. Always read from ``getState()``.

2. **Entity-Based Stores**

   One store per entity type. Don't create separate stores for different views
   of the same data (e.g., use PostsStore for all post collections).

3. **Immutable Updates**

   Create new state objects rather than mutating existing state:

   .. code-block:: cpp

       // Good
       auto newState = getState();
       newState.posts.add(newPost);
       setState(newState);

       // Bad - mutating state directly
       state.posts.add(newPost);

4. **Optimistic Updates**

   Update UI immediately, then sync with server:

   .. code-block:: cpp

       void PostsStore::toggleLike(const juce::String& postId) {
           // Optimistic update
           auto state = getState();
           if (auto* post = findPost(postId, state)) {
               post->isLiked = !post->isLiked;
               post->likeCount += post->isLiked ? 1 : -1;
           }
           setState(state);

           // Server sync (fire and forget or with rollback)
           networkClient->likePost(postId, [](auto result) {
               if (!result.isOk()) {
                   // Could rollback here
               }
           });
       }

5. **Error Handling in State**

   Store errors in state rather than throwing:

   .. code-block:: cpp

       auto newState = getState();
       newState.error = "Failed to load feed";
       newState.isLoading = false;
       setState(newState);

Thread Safety
-------------

Stores are primarily accessed from the JUCE message thread:

* All ``setState()`` calls dispatch to message thread automatically
* Network callbacks are marshalled to message thread
* Subscribers are called on message thread
* Components receive updates on message thread (safe for UI)

See Also
--------

* :doc:`observables` - Property-level reactive bindings
* :doc:`reactive-components` - UI component integration
* :doc:`data-flow` - Complete data flow examples
* :doc:`threading` - Threading model and constraints
