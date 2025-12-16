# Sidechain VST Plugin Architecture Guide

**Last Updated**: December 15, 2024
**Target Audience**: Developers, architects, new team members
**Scope**: Modern C++ architecture, reactive patterns, state management

## Table of Contents

1. [Quick Overview](#quick-overview)
2. [Store Pattern](#store-pattern)
3. [Observable Collections](#observable-collections)
4. [Data Flow Architecture](#data-flow-architecture)
5. [Threading Model](#threading-model)
6. [Component Patterns](#component-patterns)
7. [Testing Strategies](#testing-strategies)
8. [Common Patterns & Best Practices](#common-patterns--best-practices)
9. [Troubleshooting](#troubleshooting)

---

## Quick Overview

### Core Architecture Principles

- **Single Source of Truth**: One store per entity type (PostsStore, UserStore, ChatStore, etc.)
- **Reactive Updates**: Components subscribe to stores, automatically re-render on state changes
- **Immutable State**: State objects are copied and replaced, never mutated
- **Optimistic Updates**: UI updates immediately, server sync happens asynchronously
- **Error Handling**: Errors stored in state, not thrown as exceptions
- **Type Safety**: Strong typing throughout (C++26, std::optional, no raw pointers)

### Architecture Layers

```
┌─────────────────────────────────────────┐
│       UI Components                     │ (PostCard, PostsFeed, Profile, etc.)
│   (ReactiveBoundComponent subclasses)   │
└────────────────┬────────────────────────┘
                 │ subscribe to
                 ▼
┌─────────────────────────────────────────┐
│       Stores (State Management)          │ (PostsStore, UserStore, ChatStore)
│       Store<T> + Reactive Bindings      │
└────────────────┬────────────────────────┘
                 │ read/write via
                 ▼
┌─────────────────────────────────────────┐
│       NetworkClient                     │ (HTTP + WebSocket)
│   (Rate limiting, error tracking)       │
└────────────────┬────────────────────────┘
                 │ sends/receives
                 ▼
┌─────────────────────────────────────────┐
│       Backend API                       │ (Go server + getstream.io)
│   (REST endpoints + WebSocket)          │
└─────────────────────────────────────────┘
```

---

## Store Pattern

### What is a Store?

A **Store** is a centralized, reactive state container that:
- Holds application state in a strongly-typed struct
- Provides `subscribe()` for reactive updates
- Exposes state via `getState()`
- Updates state through action methods
- Notifies all subscribers when state changes

### Why Stores?

| Without Stores | With Stores |
|---|---|
| State scattered across components | Centralized in one place |
| Manual server sync in each component | Automatic in store |
| Callback hell | Clean subscription model |
| Hard to test | Easy to mock NetworkClient |
| Data consistency risk | Single source of truth |

### The 9 Core Stores

| Store | Purpose | Key State |
|-------|---------|-----------|
| **PostsStore** | All posts (feeds, saved, archived) | `PostsState` |
| **UserStore** | All users (current, cached, discovery) | `UserState` |
| **ChatStore** | Messages and channels | `ChatStoreState` |
| **DraftStore** | Draft posts (singleton) | `DraftState[]` |
| **CommentStore** | Comments on posts | `CommentState` |
| **NotificationStore** | Notifications and unread counts | `NotificationState` |
| **StoriesStore** | Stories and highlights | `StoriesState` |
| **UploadStore** | Upload progress and state | `UploadState` |
| **FollowersStore** | Follower/following lists | `FollowListState` |

### Store Base Class

All stores inherit from `Store<TState>`:

```cpp
template<typename TState>
class Store {
public:
    // Get current state (read-only)
    const TState& getState() const;

    // Subscribe to state changes
    // Returns unsubscribe function for cleanup
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
```

### Design Principle: Single Source of Truth Per Entity

**❌ Wrong**: Separate stores for different views of the same data
```cpp
SavedPostsStore savedPosts;    // Wrong!
FeedStore feed;                 // Wrong!
ArchivedPostsStore archived;    // Wrong!
```

**✅ Right**: One store with multiple substates
```cpp
PostsStore postsStore;  // Handles all post collections
// - feeds (Timeline, Global, Trending, etc.)
// - savedPosts
// - archivedPosts
```

**Benefits**:
- Prevents state desynchronization
- Single place to update post-related state
- Easy to reason about
- Consistent behavior across views

### PostsStore Example

#### State Structure

```cpp
struct PostsState {
    // Feed collections (multiple feed types)
    std::map<FeedType, FeedState> feeds;
    std::map<FeedType, AggregatedFeedState> aggregatedFeeds;
    FeedType currentFeedType = FeedType::Timeline;

    // User post collections
    SavedPostsState savedPosts;
    ArchivedPostsState archivedPosts;

    // Global error tracking
    juce::String errorMessage;
    int64_t lastUpdated = 0;

    // Convenience accessors
    const FeedState& getCurrentFeed() const;
};
```

#### Key Methods

```cpp
// Feed operations
void loadFeed(FeedType feedType, bool forceRefresh = false);
void switchFeedType(FeedType feedType);
void loadMore();

// Post interactions (optimistic updates)
void toggleLike(const juce::String& postId);
void toggleSave(const juce::String& postId);
void toggleRepost(const juce::String& postId);
void addReaction(const juce::String& postId, const juce::String& emoji);

// Saved/archived posts
void loadSavedPosts();
void unsavePost(const juce::String& postId);
void loadArchivedPosts();
void restorePost(const juce::String& postId);

// Cache management
void clearCache();
void startCacheWarming();
```

#### Usage Pattern

```cpp
// 1. Subscribe to store
auto unsubscribe = postsStore->subscribe([this](const PostsState& state) {
    // 2. Render based on current state
    displayFeed(state.getCurrentFeed().posts);
    updateSavedCount(state.savedPosts.totalCount);

    // 3. Show loading state if needed
    if (state.getCurrentFeed().isLoading) {
        showLoadingSpinner();
    }
});

// 4. Trigger actions (store handles async + sync)
postsStore->toggleLike("post-123");

// 5. Cleanup in destructor
~MyComponent() {
    if (unsubscribe) unsubscribe();
}
```

### UserStore Example

#### State Structure

```cpp
struct UserState {
    // Current logged-in user
    juce::String userId;
    juce::String username;
    juce::String email;
    juce::String displayName;
    juce::String bio;
    juce::Image profileImage;

    // Social metrics
    int followerCount = 0;
    int followingCount = 0;
    int postCount = 0;

    // Cached users (for profiles, mentions)
    std::map<juce::String, CachedUser> userCache;

    // Discovery sections
    juce::Array<DiscoveredUser> trendingUsers;
    juce::Array<DiscoveredUser> suggestedUsers;
    juce::Array<DiscoveredUser> recommendedToFollow;

    // State flags
    bool isLoggedIn = false;
    juce::String error;
};
```

#### Key Methods

```cpp
// Authentication
void setAuthToken(const juce::String& token);
void clearAuthToken();
bool isLoggedIn() const;

// Profile operations
void fetchUserProfile(bool forceRefresh = false);
void updateProfile(const juce::String& username, const juce::String& bio);
void uploadProfilePicture(const juce::File& imageFile);

// User cache
const CachedUser* getCachedUser(const juce::String& userId) const;
void loadUserProfile(const juce::String& userId);

// User discovery
void loadDiscoveryData();
void loadTrendingUsers();
void loadSuggestedUsers();

// Social actions
void followUser(const juce::String& userId);
void unfollowUser(const juce::String& userId);
void blockUser(const juce::String& userId);
void muteUser(const juce::String& userId);
```

---

## Observable Collections

### ObservableProperty<T>

Reactive wrapper for single values:

```cpp
// Subscribe to property changes
auto property = std::make_shared<ObservableProperty<int>>(0);

auto unsubscribe = property->subscribe([](int newValue) {
    std::cout << "Value changed to: " << newValue << std::endl;
});

// Setting value notifies all subscribers
property->setValue(42);

// Map/filter operations
auto doubled = property
    ->map([](int x) { return x * 2; })
    ->subscribe([](int x) { std::cout << x << std::endl; });
```

### ObservableArray<T>

Reactive array with batch notifications:

```cpp
auto posts = std::make_shared<ObservableArray<FeedPost>>();

// Subscribe to array changes
posts->subscribe([](const ArrayChangeEvent<FeedPost>& event) {
    switch (event.type) {
        case ChangeType::ItemAdded:
            std::cout << "Post added at index " << event.index << std::endl;
            break;
        case ChangeType::ItemRemoved:
            std::cout << "Post removed at index " << event.index << std::endl;
            break;
        case ChangeType::BatchUpdate:
            std::cout << "Batch update with " << event.items.size() << " items" << std::endl;
            break;
    }
});

// Add items
posts->add(newPost);

// Batch operations
posts->setAll(newPosts);  // Single notification instead of N
posts->remove(0);
posts->update(0, updatedPost);
```

### ObservableMap<K, V>

Reactive key-value store:

```cpp
auto userCache = std::make_shared<ObservableMap<juce::String, User>>();

// Subscribe to map changes
userCache->subscribe([](const MapChangeEvent<juce::String, User>& event) {
    if (event.type == ChangeType::ItemAdded) {
        std::cout << "User " << event.key << " cached" << std::endl;
    }
});

// Map operations
userCache->set("user-123", user);
auto user = userCache->get("user-123");
userCache->remove("user-123");
userCache->clear();
```

---

## Data Flow Architecture

### Feed Loading Flow

```
┌─────────────┐
│   UI Render │ Shows loading spinner
└──────┬──────┘
       │
       ▼
┌─────────────────────┐
│ PostsFeed calls     │ "Load Timeline"
│ postsStore->        │
│ loadFeed()          │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│ PostsStore state:   │ isLoading = true
│ setState()          │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│ All subscribers     │ PostsFeed repaint()
│ notified            │
└──────┬──────────────┘
       │
       ▼ (async, background thread)
┌─────────────────────┐
│ NetworkClient       │ GET /api/feeds/timeline
│ getFeed()           │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│ Server responds     │ [post1, post2, post3]
│ with posts          │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│ PostsStore updates  │ posts = [...],
│ state in callback   │ isLoading = false
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│ All subscribers     │ PostsFeed renders posts
│ notified again      │
└─────────────────────┘
```

### Optimistic Update Flow

```
┌─────────────────────┐
│ User clicks         │
│ "Like" button       │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│ PostCard emits      │ onLikeClicked()
│ callback            │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│ PostsFeed calls     │ postsStore->toggleLike(id)
│ store method        │
└──────┬──────────────┘
       │
       ├─────────────────────────────────────┐
       │                                     │
       ▼                                     ▼
┌──────────────────┐              ┌─────────────────────┐
│ Optimistic       │ (immediate)  │ Server sync         │
│ Update:          │              │ NetworkClient->     │
│ - isLiked = true │              │ likePost(id)        │
│ - likeCount++    │              │ (async)             │
│ - setState()     │              │                     │
│ - notify subs    │              │                     │
│ - UI updates     │              │                     │
└────────┬─────────┘              └─────────────────────┘
         │                                  │
         ▼                                  ▼
┌─────────────────────┐           ┌──────────────────┐
│ User sees like      │           │ Server confirms  │
│ immediately (fast)  │           │ like successful  │
└─────────────────────┘           │ (may take 500ms) │
                                  └────────┬─────────┘
                                           │
                                           ▼
                                  ┌──────────────────┐
                                  │ If failed:       │
                                  │ rollback by      │
                                  │ calling toggle   │
                                  │ again to revert  │
                                  └──────────────────┘
```

### Real-Time Sync Flow

```
┌──────────────────────────┐
│ WebSocket connection     │ Open persistent connection
│ established              │ to /ws/sync
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│ User posts comment       │
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│ CommentStore applies     │ Operational Transform
│ transform locally        │
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│ Send Operation via WS    │ {type: Insert, pos: 42, text: "..."}
│ to server                │
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│ Server receives OT,      │ Transform against concurrent ops
│ transforms + broadcasts  │
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│ All other clients        │ Receive transformed operation
│ receive via WS           │
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│ CommentStore applies     │
│ transformed operation    │
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│ UI automatically         │ Comment appears for all users
│ updates via subscribe    │
└──────────────────────────┘
```

---

## Threading Model

### Thread Safety Constraints

#### Audio Thread (Real-Time, Lock-Free)

```
✅ Allowed:
- Read from atomic<> variables
- Pre-allocated buffers
- Queuing operations (lock-free queue)
- Ring buffers for IPC

❌ Forbidden:
- Any locks (mutex, semaphore)
- Memory allocation (new, malloc)
- File I/O
- Network calls
- Anything that could block
```

**Why**: Audio thread must never block or be interrupted, or we get clicks/pops.

#### Message Thread (UI Thread, JUCE)

```
✅ Allowed:
- Update components
- Repaint()
- Any synchronous operations
- Brief computations

❌ Forbidden:
- Network calls (use Async)
- File I/O (use Async)
- Long computations (use background thread)
```

#### Background Threads (Network, File I/O)

```
✅ Allowed:
- Network requests
- File I/O
- Long computations
- Blocking operations

❌ Forbidden:
- Direct UI updates (use callAsync)
- Audio buffer manipulation
```

### Async Pattern

All network operations use the `Async` helper:

```cpp
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
```

### Thread-Safe Subscriber Pattern

When subscribing from UI thread but handling async updates:

```cpp
class PostsFeed : public ReactiveBoundComponent {
private:
    std::function<void()> storeUnsubscribe;

public:
    void bindToStore(std::shared_ptr<PostsStore> store) {
        // Capture as SafePointer to handle async deletion
        auto safeThis = juce::Component::SafePointer<PostsFeed>(this);

        storeUnsubscribe = store->subscribe([safeThis](const PostsState& state) {
            // Check if component still exists before updating
            if (safeThis) {
                safeThis->updateUI(state);
            }
        });
    }

    ~PostsFeed() override {
        if (storeUnsubscribe) storeUnsubscribe();
    }
};
```

---

## Component Patterns

### Container/Presentation Pattern

**Container Component** (Smart, manages state):
```cpp
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
        // Store handles everything
        postsStore->toggleLike(post.id);
    }

    ~PostsFeed() override {
        if (unsubscribe) unsubscribe();
    }
};
```

**Presentation Component** (Dumb, receives props):
```cpp
class PostCard : public Component {
private:
    FeedPost post;  // Received as prop, not managed

public:
    void setPost(const FeedPost& newPost) {
        post = newPost;
        repaint();
    }

    // Callbacks bubble up to parent (PostsFeed)
    std::function<void(const FeedPost&)> onLikeClicked;
    std::function<void(const FeedPost&)> onCommentClicked;
    std::function<void(const FeedPost&)> onShareClicked;
};
```

### ReactiveBoundComponent

Base class for all components that subscribe to stores:

```cpp
class ReactiveBoundComponent : public Component {
protected:
    // Helper for safe async subscription cleanup
    void safeSubscribe(std::function<void()> unsubscribe) {
        // Stores unsubscribe in smart container
    }

    // Called when component is being destroyed
    // Cleanup happens automatically
};
```

---

## Testing Strategies

### Unit Testing Stores

```cpp
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
```

### Integration Testing

```cpp
class PostsFeedIntegrationTest : public juce::UnitTest {
public:
    void runTest() override {
        // Create real store + real feed component
        auto store = std::make_shared<PostsStore>(networkClient.get());
        auto feed = std::make_unique<PostsFeed>();
        feed->setPostsStore(store);

        // Load feed
        store->loadFeed(FeedType::Timeline);

        // Give async operation time to complete
        juce::Thread::sleep(100);

        // Verify feed rendered correctly
        expect(feed->getPostCount() > 0);
    }
};
```

---

## Common Patterns & Best Practices

### Immutable Updates

```cpp
// ✅ Good: Create new state, replace entirely
{
    auto state = getState();  // Copy
    state.posts.add(newPost);  // Modify copy
    setState(state);  // Replace entire state
}

// ❌ Bad: Mutate state directly
{
    auto& state = getState();  // Bad! Direct reference
    state.posts.add(newPost);  // Mutating original
}
```

### Error Handling

```cpp
// Store errors in state, don't throw
{
    auto state = getState();
    state.error = "Failed to load feed";
    state.isLoading = false;
    setState(state);
}

// UI subscribes and displays error
store->subscribe([this](const PostsState& state) {
    if (!state.error.isEmpty()) {
        showErrorNotification(state.error);
    }
});
```

### Cache Management

```cpp
// Multi-tier cache: Memory + Disk
// Automatically:
// - Returns from memory (< 1ms)
// - Falls back to disk (< 100ms)
// - Makes network call if needed

store->loadFeed(FeedType::Timeline);  // Uses cache automatically
store->loadFeed(FeedType::Timeline, true);  // Force refresh, bypass cache
store->clearCache();  // Clear memory cache
store->startCacheWarming();  // Pre-fill cache for offline support
```

### Subscription Cleanup

```cpp
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
        // ✅ Always cleanup
        if (unsubscribe) {
            unsubscribe();
        }
    }
};
```

---

## Troubleshooting

### Component Not Updating When Store Changes

**Problem**: Subscribe to store but UI doesn't update

**Causes**:
1. Didn't save unsubscribe function
2. Component was deleted before state changed
3. Not calling `repaint()` in subscription

**Fix**:
```cpp
auto unsubscribe = store->subscribe([this](const PostsState& state) {
    updateUI(state);
    repaint();  // Important!
});
// ... later ...
if (unsubscribe) unsubscribe();  // Cleanup!
```

### State Changes But Nobody Notified

**Problem**: Called `setState()` but subscribers didn't get called

**Causes**:
1. Mutated state instead of creating new one
2. Comparing same object (not deep copy)

**Fix**:
```cpp
// Create copy before modifying
auto state = getState();  // Makes copy
state.posts.add(newPost);  // Modify copy
setState(state);  // Notify subscribers

// NOT:
auto& state = getState();  // Direct reference (wrong!)
state.posts.add(newPost);  // Mutate original (wrong!)
// setState() won't be called
```

### Memory Leaks from Store Subscriptions

**Problem**: Component destroyed but never unsubscribed

**Fix**:
```cpp
class MyComponent {
private:
    std::function<void()> unsubscribe;

public:
    ~MyComponent() override {
        // Call in destructor!
        if (unsubscribe) unsubscribe();
    }
};
```

### Network Requests Taking Too Long

**Problem**: UI freezes during network call

**Causes**:
1. Network call on UI thread (should be background)
2. Store not using Async properly

**Fix**: All NetworkClient calls automatically happen on background thread via Async

---

## See Also

- [Store Pattern Documentation](../plugin-architecture/stores.rst)
- [Reactive Components Guide](../plugin-architecture/reactive-components.rst)
- [MODERNIZATION_EVALUATION_REPORT](../../notes/MODERNIZATION_EVALUATION_REPORT.md) - Complete project assessment
- [CLAUDE.md](../../CLAUDE.md) - Development commands and guidelines
