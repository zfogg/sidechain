# RxCpp + Slices Architecture Guide

## Overview

The Sidechain plugin uses a **hybrid reactive architecture** combining:
- **ImmutableSlice** (Redux-style state management) for persistent UI state
- **RxCpp Observables** for async operations and data transformations

Both are necessary and complementary.

## When to Use What

### Use Slices For:
- **Persistent UI state** (current user, feed posts, auth status)
- **State shared across components** (multiple views reading same data)
- **State that survives async operations** (feed data stays after network call)
- **Optimistic updates with rollback** (toggle like → update slice → rollback on error)

### Use Observables For:
- **Async operations** (network requests, file I/O, timers)
- **Data transformations** (map, filter, merge streams)
- **Debouncing** (search input with 300ms delay)
- **One-shot results** (login returns result once, then completes)
- **Combining multiple async sources** (merge multiple API calls)

## Architecture Patterns

### Pattern 1: Observable Updates Slice (Most Common)

```cpp
// UI subscribes to slice for state updates
auto unsub = postsSlice->subscribe([this](const PostsState& state) {
    renderPosts(state.feeds[FeedType::Timeline].posts);
});

// User action triggers observable
appStore->loadFeedObservable(FeedType::Timeline)
    .subscribe(
        [](const std::vector<FeedPost>& posts) {
            // Observable completed - slice was already updated internally
        },
        [](std::exception_ptr e) {
            // Handle error
        });
```

Inside AppStore:
```cpp
rxcpp::observable<std::vector<FeedPost>> loadFeedObservable(FeedType type) {
    return rxcpp::sources::create<std::vector<FeedPost>>([this, type](auto observer) {
        networkClient->getFeed(type, [this, type, observer](auto result) {
            if (result.isOk()) {
                auto posts = parsePosts(result.getValue());

                // Update slice (triggers all UI subscribers)
                PostsState newState = sliceManager.posts->getState();
                newState.feeds[type].posts = posts;
                sliceManager.posts->setState(newState);

                // Complete observable
                observer.on_next(posts);
                observer.on_completed();
            } else {
                observer.on_error(std::make_exception_ptr(...));
            }
        });
    }).observe_on(Rx::observe_on_juce_thread());
}
```

### Pattern 2: Observable-Only (No State Persistence)

For operations that don't need persistent state:

```cpp
// Search results shown temporarily, not stored in slice
appStore->searchUsersObservable(query)
    .subscribe([this](const std::vector<User>& users) {
        showSearchResults(users);  // Just display, no slice update
    });
```

### Pattern 3: Optimistic Update with Rollback

```cpp
rxcpp::observable<int> toggleLikeObservable(const juce::String& postId) {
    return rxcpp::sources::create<int>([this, postId](auto observer) {
        // 1. Capture current state for potential rollback
        auto currentState = sliceManager.posts->getState();
        bool wasLiked = findPost(currentState, postId)->isLiked;

        // 2. Optimistic update (instant UI feedback)
        PostsState newState = currentState;
        updatePost(newState, postId, [](auto& post) {
            post->isLiked = !post->isLiked;
            post->likeCount += post->isLiked ? 1 : -1;
        });
        sliceManager.posts->setState(newState);

        // 3. Server request
        networkClient->toggleLike(postId, [this, postId, wasLiked, observer](auto result) {
            if (result.isOk()) {
                observer.on_next(0);
                observer.on_completed();
            } else {
                // 4. Rollback on failure
                PostsState rollbackState = sliceManager.posts->getState();
                updatePost(rollbackState, postId, [wasLiked](auto& post) {
                    post->isLiked = wasLiked;
                    post->likeCount += wasLiked ? 1 : -1;
                });
                sliceManager.posts->setState(rollbackState);
                observer.on_error(...);
            }
        });
    }).observe_on(Rx::observe_on_juce_thread());
}
```

## Slice Responsibilities

| Slice | State Managed |
|-------|---------------|
| `AuthSlice` | Login status, tokens, 2FA state |
| `PostsSlice` | All feeds, saved posts, archived posts |
| `UserSlice` | Current user profile, settings |
| `ChatSlice` | Conversations, messages, typing indicators |
| `CommentsSlice` | Comments per post |
| `SearchSlice` | Search queries, results |
| `NotificationSlice` | Push notifications, unread counts |
| `UploadSlice` | Upload progress, current file |
| `PlaylistSlice` | User playlists |
| `SoundSlice` | Featured/trending sounds |
| `ChallengeSlice` | MIDI challenges |
| `StoriesSlice` | User stories |
| `DraftSlice` | Saved drafts |
| `DiscoverySlice` | Discover/trending content |
| `FollowersSlice` | Followers/following lists |
| `PresenceSlice` | Online/offline status |

## Observable Categories

### Data Loading
- `loadFeedObservable()` - Load feed posts
- `loadPlaylistsObservable()` - Load user playlists
- `loadChallengesObservable()` - Load MIDI challenges
- `loadFeaturedSoundsObservable()` - Load trending sounds

### User Actions
- `toggleLikeObservable()` - Like/unlike post
- `toggleSaveObservable()` - Save/unsave post
- `toggleFollowObservable()` - Follow/unfollow user
- `toggleRepostObservable()` - Repost/unrepost

### Authentication
- `loginObservable()` - Login with credentials
- `registerAccountObservable()` - Create account
- `verify2FAObservable()` - Verify 2FA code

### Content Creation
- `uploadPostObservable()` - Upload audio with progress
- `sendMessageObservable()` - Send chat message
- `submitChallengeObservable()` - Submit challenge entry

### Search
- `searchUsersObservable()` - Search users
- `searchPostsObservable()` - Search posts (with debounce)

## Best Practices

### 1. Always Use `observe_on(Rx::observe_on_juce_thread())`
Ensures callbacks run on JUCE message thread for safe UI updates.

### 2. Use SafePointer in Subscriptions
```cpp
juce::Component::SafePointer<MyComponent> safeThis(this);
observable.subscribe([safeThis](auto result) {
    if (!safeThis) return;  // Component was destroyed
    safeThis->updateUI(result);
});
```

### 3. Store Unsubscriber for Cleanup
```cpp
class MyComponent {
    std::function<void()> unsubscriber_;

    void init() {
        unsubscriber_ = slice->subscribe([this](auto& state) { ... });
    }

    ~MyComponent() {
        if (unsubscriber_) unsubscriber_();
    }
};
```

### 4. Prefer Observables Over Callbacks for New Code
```cpp
// ❌ Old pattern (callback hell)
networkClient->getFeed(type, [this](auto result) {
    networkClient->getUser(userId, [this](auto user) {
        // Nested callbacks
    });
});

// ✅ New pattern (observable composition)
loadFeedObservable(type)
    .flat_map([this](auto posts) {
        return loadUserObservable(posts[0].userId);
    })
    .subscribe(...);
```

## Future Improvements

1. **BehaviorSubject for Hot Observables** - Share subscriptions for real-time data
2. **Retry Operators** - `retry(3)` for transient network failures
3. **Caching Layer** - `replay(1)` for cached responses
4. **Unified Error Handling** - Central error observable for toast notifications
