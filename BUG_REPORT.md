# Plugin Architecture Bug Report

## Executive Summary
Found **7 critical bugs** in the plugin architecture related to state management, optimistic updates, and error handling. These bugs can cause UI state inconsistencies, network failures to go unnoticed, and potential memory leaks.

---

## Bug #1: Missing Rollback Logic in `toggleLike`, `toggleSave`, `toggleRepost`
**Severity**: 🔴 **CRITICAL**
**File**: `plugin/src/stores/app/Feed.cpp`
**Lines**: 302-365, 367-429, 431-493

### Issue
The `toggleLike`, `toggleSave`, and `toggleRepost` methods implement optimistic updates but fail to rollback when the network request fails.

**Current Flow**:
1. Read `isCurrentlyLiked` from state (line 308-318)
2. Apply optimistic update to UI (line 321-345)
3. Make network call with error callback (line 348-365)
4. If error: Just log it, **don't rollback** ❌

### Impact
- User sees the toggled state (e.g., post marked as liked) even though the server rejected it
- On next app restart, state reverts, confusing the user
- No automatic retry mechanism

### Example - toggleLike (Line 349):
```cpp
networkClient->unlikePost(postId, [](Outcome<juce::var> result) {
  if (!result.isOk()) {
    Util::logError("AppStore", "Failed to unlike post: " + result.getError());
    // BUG: No rollback! UI still shows unliked state even though it failed
  }
});
```

### Fix Required
Implement rollback logic in error callbacks:
```cpp
networkClient->unlikePost(postId, [this, postId, previousState](Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Rollback optimistic update
    PostsState rollbackState = sliceManager.posts->getState();
    for (auto &[feedType, feedState] : rollbackState.feeds) {
      for (auto &post : feedState.posts) {
        if (post->id == postId) {
          post->isLiked = previousState;  // Restore original state
          post->likeCount += previousState ? 1 : -1;
        }
      }
    }
    sliceManager.posts->setState(rollbackState);
  }
});
```

---

## Bug #2: Stale State Read in Optimistic Updates
**Severity**: 🟠 **HIGH**
**File**: `plugin/src/stores/app/Feed.cpp`
**Lines**: 302-365, 367-429, 431-493

### Issue
The `toggleLike` method reads the current state early but uses it much later, creating a race condition:

```cpp
// Line 308-318: Read state EARLY
bool isCurrentlyLiked = false;
for (const auto &[feedType, feedState] : currentPostsState.feeds) {
  for (const auto &post : feedState.posts) {
    if (post->id == postId) {
      isCurrentlyLiked = post->isLiked;  // Captured here
      break;
    }
  }
}

// Line 321-345: Apply optimistic update

// Line 348: Use stale `isCurrentlyLiked` value here
if (isCurrentlyLiked) {
  networkClient->unlikePost(postId, ...);
} else {
  networkClient->likePost(postId, ...);
}
```

**Problem**: If the user clicks "like" again before the first network call completes:
1. First click: `isCurrentlyLiked = false` → Send `likePost()`
2. Second click: `isCurrentlyLiked = false` (stale!) → Send `likePost()` again instead of `unlikePost()`

### Fix Required
Read state immediately before making the network call and refactor to capture state at decision time.

---

## Bug #3: Missing Callback in `togglePin`
**Severity**: 🟡 **MEDIUM**
**File**: `plugin/src/stores/app/Feed.cpp`
**Lines**: 609-633

### Issue
The `togglePin` method calls the network client with `nullptr` as the callback:

```cpp
void AppStore::togglePin(const juce::String &postId, bool pinned) {
  // ... optimistic update ...

  if (pinned) {
    networkClient->pinPost(postId, nullptr);  // BUG: null callback!
  } else {
    networkClient->unpinPost(postId, nullptr);  // BUG: null callback!
  }
}
```

### Impact
- Network errors are silently ignored
- No error handling or logging
- UI could show pinned state even if server rejected it

### Fix Required
Add proper error handling callback with rollback logic.

---

## Bug #4: Race Condition in WebSocket Presence Update
**Severity**: 🟡 **MEDIUM**
**File**: `plugin/src/stores/app/WebSocket.cpp`
**Lines**: 84-101

### Issue
The `onWebSocketPresenceUpdate` method compares user IDs without null checks:

```cpp
void AppStore::onWebSocketPresenceUpdate(const juce::String &userId, bool isOnline) {
  // ...
  PresenceState newState = sliceManager.presence->getState();

  // No check if userId is empty or if sliceManager is valid
  newState.userPresence[userId] = userPresence;
  sliceManager.presence->setState(newState);
}
```

### Impact
- Could insert empty keys into presence map
- Undefined behavior if called during shutdown

### Fix Required
Add input validation for empty userIds.

---

## Bug #5: Missing Null Pointer Check in `toggleFollow`
**Severity**: 🟡 **MEDIUM**
**File**: `plugin/src/stores/app/Feed.cpp`
**Lines**: 508-583

### Issue
The `toggleFollow` method doesn't validate that the post was found before proceeding with the network call. If the post isn't found in any feed, the method silently returns without error logging.

### Impact
- Silent failures when posts are missing
- No user feedback for why follow action didn't work

---

## Bug #6: Incomplete Error Handling in Feed Loading
**Severity**: 🟡 **MEDIUM**
**File**: `plugin/src/stores/app/Feed.cpp`
**Lines**: 47-73, 80-102

### Issue
When feed loading fails due to missing network client, the state `isLoading` flag is not properly reset to false:

```cpp
void AppStore::loadFeed(FeedType feedType, bool forceRefresh) {
  if (!networkClient) {
    PostsState newState = sliceManager.posts->getState();
    newState.feedError = "Network client not initialized";
    sliceManager.posts->setState(newState);
    // BUG: isLoading is NOT set to false!
    return;
  }
  // ...
}
```

### Impact
- UI spinners never disappear
- User cannot retry loading
- UX appears stuck/frozen

---

## Bug #7: Observer Pattern Memory Leak Risk in AppStoreComponent
**Severity**: 🟠 **HIGH** (Potential)
**File**: `plugin/src/core/PluginEditor.cpp` and other UI components

### Issue
UI components subscribe to AppStore slices but may not unsubscribe in their destructors. Subscriptions are stored as `std::function<void()>` which need to be called to unsubscribe.

### Impact
- Callback lambdas capture `this` and are never released
- AppStore keeps dangling references to destroyed components
- Memory leak of subscription closures
- Potential use-after-free when state changes after component destruction

### Pattern for Correct Usage:
```cpp
class SomeComponent : public juce::Component {
  std::function<void()> unsubscribe;

  void setup() {
    unsubscribe = appStore.subscribeToFeed([this](const PostsState& state) {
      handleStateChange(state);
    });
  }

  ~SomeComponent() override {
    if (unsubscribe) unsubscribe();  // MUST call to cleanup
  }
};
```

---

## Summary Table

| Bug # | Component | Type | Severity | Status |
|-------|-----------|------|----------|--------|
| 1 | Feed.cpp | Optimistic Update | CRITICAL | Needs Fix |
| 2 | Feed.cpp | Race Condition | HIGH | Needs Fix |
| 3 | Feed.cpp | Null Callback | MEDIUM | Needs Fix |
| 4 | WebSocket.cpp | Input Validation | MEDIUM | Needs Fix |
| 5 | Feed.cpp | Null Check | MEDIUM | Needs Fix |
| 6 | Feed.cpp | Error Handling | MEDIUM | Needs Fix |
| 7 | Architecture | Memory Leak | HIGH | Needs Audit |

---

## Recommendations

### Priority 1 (Fix Immediately)
1. **Bug #1**: Add rollback logic to `toggleLike`, `toggleSave`, `toggleRepost` for network failure cases
2. **Bug #2**: Fix state read timing in toggle methods to avoid stale captures
3. **Bug #7**: Audit and fix all subscription cleanup in UI components

### Priority 2 (Fix Soon)
4. **Bug #3**: Add error callbacks to `togglePin` with proper error handling
5. **Bug #6**: Fix error state handling in `loadFeed` and `loadMore` to reset `isLoading`

### Priority 3 (Code Quality)
6. **Bug #4**: Add input validation to WebSocket handlers for empty userIds
7. **Bug #5**: Add null checks and error logging for missing posts

---

## Testing Strategy

After fixes:
1. **Network Failure Tests**: Disable network and verify rollbacks occur correctly
2. **Race Condition Tests**: Rapid double-clicks on like/save buttons to test state consistency
3. **Memory Leak Tests**: Subscribe/unsubscribe cycles with memory profiler
4. **WebSocket Tests**: Send presence updates with invalid/empty user IDs
5. **Feed Loading Tests**: Verify spinners hide on error and state is properly reset
6. **Integration Tests**: Full user workflows with network failures at various points
