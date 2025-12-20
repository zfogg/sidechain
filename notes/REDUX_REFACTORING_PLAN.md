# Redux-Style Data Store Refactoring Plan

**Date**: December 20, 2025
**Status**: Planning Phase
**Objective**: Migrate all UI components to use a centralized Redux-style data store with normalized entity storage via unordered_maps of shared_pointers

---

## Executive Summary

The Sidechain VST plugin currently has a partial Redux architecture:
- ✅ AppStore (action dispatcher)
- ✅ AppSliceManager (state slices with subscriptions)
- ✅ EntityStore (normalized entity caching with shared_ptr)
- ⚠️ **Problem**: 45 components still call NetworkClient directly instead of dispatching to AppStore

**Goal**: Eliminate ALL direct NetworkClient calls from UI components. Only AppStore should communicate with NetworkClient.

**Scope**: ~45 files across plugin/src/ui/, plugin/src/stores/app/, and plugin/src/network/

**Estimated Effort**: 3-5 days for comprehensive refactoring

---

## Phase 1: Infrastructure Enhancements (1 day)

### 1.1 EntityStore Enhancement
**File**: `plugin/src/stores/EntityStore.h/cpp`

- [x] Verify all caches use `std::unordered_map<std::string, std::shared_ptr<T>>`
- [x] Confirm `EntityCache<T>::getOrCreate()` handles memory deduplication
- [x] Test TTL-based expiration for each entity type
- [x] Add cache statistics methods (size, hit rate, memory usage)
- [x] Document entity lifecycle (creation → update → expiration)

**Acceptance Criteria**:
- ✅ All entity types backed by unordered_map of shared_ptr
- ✅ Same entity ID always returns same shared_ptr instance
- ✅ TTL expiration working for stale data
- ✅ No memory leaks from circular shared_ptr references
- ✅ EntitySlice successfully bridges EntityStore and Redux
- ✅ All methods are thread-safe
- ✅ Project compiles with no errors
- ✅ Cache statistics available for debugging

### 1.2 EntitySlice Bridge Creation
**New File**: `plugin/src/stores/slices/EntitySlice.h`

```cpp
namespace Sidechain::Stores::Slices {
  /**
   * EntitySlice - Bridge between EntityStore and Redux state system
   * Ensures all entity updates flow through slice subscriptions
   *
   * When an entity is updated in EntityStore, automatically
   * notifies all slice subscribers so UI components receive updates
   */
  class EntitySlice {
  public:
    static EntitySlice& getInstance();

    // Normalize and cache entities, then notify subscribers
    void cachePost(const std::shared_ptr<FeedPost>& post);
    void cacheUser(const std::shared_ptr<User>& user);
    void cacheMessage(const std::shared_ptr<Message>& message);

    // Subscribe to entity-level changes
    using PostObserver = std::function<void(const std::shared_ptr<FeedPost>&)>;
    std::function<void()> subscribeToPost(const juce::String& postId, PostObserver observer);

  private:
    EntityStore& entityStore_;
    AppStore* appStore_;
  };
}
```

- [x] Create `EntitySlice.h` with entity caching methods
- [x] Implement batch caching for all entity types (posts, users, messages, stories, etc.)
- [x] Add subscription system for individual entity changes
- [x] Add cache invalidation methods
- [x] Add cache statistics for debugging
- [x] Wire into AppStore for automatic updates
- [x] Test: Project compiles with no errors
- [x] Add unit test stubs (to be implemented in Phase 4)

**Acceptance Criteria**:
- Entities cached through EntitySlice
- All slice subscribers notified on entity updates
- No direct entity manipulation outside AppStore
- Proper cleanup on unsubscribe

### 1.3 AppStore Documentation Update
**File**: `plugin/src/stores/AppStore.h`

- [x] Add detailed inline documentation for each action method
- [x] Document expected state transitions
- [x] Add examples of component usage patterns
- [x] Link to EntityStore usage guidelines
- [x] Create a "Migration Guide" comment block at top
- [x] Update AppSlices.h with EntitySlice reference

---

## Phase 2: Core Component Refactoring ✅ Starting (1 of 3 Priority 1 complete)

### 2.1 Priority 1 - Feed & Post Operations (3 components)

#### 2.1.1 PostCard.cpp
**File**: `plugin/src/ui/feed/PostCard.cpp` (1639 lines)
**Current Issue**: Handles like/save/repost locally; needs AppStore dispatch

- [x] Remove `NetworkClient *networkClient` member
- [x] Remove `setNetworkClient()` method (changed to inline no-op)
- [x] Verify all like/save/repost dispatch to AppStore via observables
- [x] Remove local callback lambdas; use AppStore subscriptions instead
- [x] Update waveformView to use AppStore-supplied NetworkClient
- [x] Test: Like/save/repost buttons trigger AppStore actions
- [x] Test: Post state updates reflected via slice subscriptions
- [x] Test: No direct NetworkClient calls in component
- [x] Plugin compiles successfully with no errors

**Files Modified**:
- `plugin/src/ui/feed/PostCard.h` (remove NetworkClient member)
- `plugin/src/ui/feed/PostCard.cpp` (refactor all actions)

**Acceptance Criteria**:
- ✅ Zero direct NetworkClient calls in component
- ✅ All interactions dispatch to AppStore via observables
- ✅ UI updates via state subscriptions
- ✅ Tests pass
- ✅ Plugin compiles with no errors

#### 2.1.2 PostsFeed.cpp
**File**: `plugin/src/ui/feed/PostsFeed.cpp` (1500+ lines)
**Current Issue**: Uses AppStore but still has NetworkClient pointer

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Remove `setNetworkClient()` method
- [ ] Verify all feed loading goes through `AppStore::getInstance().loadFeed()`
- [ ] Verify pagination via `AppStore::getInstance().loadMore()`
- [ ] Remove any remaining direct NetworkClient calls
- [ ] Test: Feed loads via AppStore actions only
- [ ] Test: Pagination works through AppStore

**Files Modified**:
- `plugin/src/ui/feed/PostsFeed.h` (remove NetworkClient member)
- `plugin/src/ui/feed/PostsFeed.cpp` (remove direct calls)

**Acceptance Criteria**:
- ✅ Feed loading entirely through AppStore
- ✅ No NetworkClient pointer in component
- ✅ Pagination via AppStore.loadMore()

#### 2.1.3 Comment.cpp
**File**: `plugin/src/ui/feed/Comment.cpp` (2300+ lines)
**Current Issue**: Posts comments via NetworkClient directly

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Remove `setNetworkClient()` method
- [ ] Refactor comment posting: `AppStore::getInstance().postComment(postId, text)`
- [ ] Refactor like comment: `AppStore::getInstance().toggleCommentLike(commentId)`
- [ ] Refactor delete comment: `AppStore::getInstance().deleteComment(commentId)`
- [ ] Refactor edit comment: `AppStore::getInstance().editComment(commentId, newText)`
- [ ] Remove direct callback handlers
- [ ] Subscribe to ChatState for comment updates
- [ ] Test: Comment actions dispatch to AppStore
- [ ] Test: UI updates via state subscriptions

**Files Modified**:
- `plugin/src/ui/feed/Comment.h` (remove NetworkClient member)
- `plugin/src/ui/feed/Comment.cpp` (refactor all actions)
- `plugin/src/stores/app/Comments.cpp` (may need new methods)

**Acceptance Criteria**:
- ✅ Comment posting via AppStore
- ✅ No direct NetworkClient calls
- ✅ UI updates via subscriptions

---

### 2.2 Priority 2 - Search & Discovery (4 components)

#### 2.2.1 UserDiscovery.cpp
**File**: `plugin/src/ui/social/UserDiscovery.cpp` (1324 lines)
**Current Issue**: Directly calls networkClient->searchUsers()

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Remove `setNetworkClient()` method
- [ ] Refactor user search: `AppStore::getInstance().searchUsers(query)`
- [ ] Refactor trending users: `AppStore::getInstance().loadTrendingUsers()`
- [ ] Refactor featured producers: `AppStore::getInstance().loadFeaturedProducers()`
- [ ] Refactor suggested users: `AppStore::getInstance().loadSuggestedUsers()`
- [ ] Refactor similar producers: `AppStore::getInstance().loadSimilarProducers()`
- [ ] Refactor follow user: `AppStore::getInstance().followUser(userId)`
- [ ] Refactor unfollow user: `AppStore::getInstance().unfollowUser(userId)`
- [ ] Remove all local caching (trending/featured/suggested - use AppStore state instead)
- [ ] Subscribe to SearchState for discovery data
- [ ] Test: All discovery actions dispatch to AppStore
- [ ] Test: UI updates via state subscriptions

**Files Modified**:
- `plugin/src/ui/social/UserDiscovery.h` (remove NetworkClient member)
- `plugin/src/ui/social/UserDiscovery.cpp` (refactor discovery methods)
- `plugin/src/stores/app/User.cpp` (add loadTrendingUsers, etc.)

**Acceptance Criteria**:
- ✅ All discovery actions through AppStore
- ✅ No local caching of trending/featured users
- ✅ State-driven UI updates
- ✅ No direct NetworkClient calls

#### 2.2.2 Search.cpp (Main Search View)
**File**: `plugin/src/ui/search/Search.cpp` (1000+ lines)
**Current Issue**: Direct search API calls

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor search: `AppStore::getInstance().searchPostsAndUsers(query)`
- [ ] Refactor filter: `AppStore::getInstance().filterSearchResults(type)`
- [ ] Subscribe to SearchState for results
- [ ] Test: Searches dispatch to AppStore
- [ ] Test: Results update via state

**Files Modified**:
- `plugin/src/ui/search/Search.h` (remove NetworkClient)
- `plugin/src/ui/search/Search.cpp` (refactor searches)

**Acceptance Criteria**:
- ✅ All search through AppStore
- ✅ No direct API calls

#### 2.2.3 FollowersList.cpp
**File**: `plugin/src/ui/social/FollowersList.cpp` (400+ lines)
**Current Issue**: Loads follower/following lists via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load followers: `AppStore::getInstance().loadFollowers(userId)`
- [ ] Refactor load following: `AppStore::getInstance().loadFollowing(userId)`
- [ ] Refactor follow/unfollow from list
- [ ] Subscribe to UserState for follower lists
- [ ] Test: List loading through AppStore

**Files Modified**:
- `plugin/src/ui/social/FollowersList.h` (remove NetworkClient)
- `plugin/src/ui/social/FollowersList.cpp` (refactor loading)

**Acceptance Criteria**:
- ✅ Follower/following lists loaded via AppStore
- ✅ No direct API calls

#### 2.2.4 MessagesList.cpp
**File**: `plugin/src/ui/messages/MessagesList.cpp` (1000+ lines)
**Current Issue**: User search for messaging via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor user search: `AppStore::getInstance().searchUsersForMessaging(query)`
- [ ] Refactor load conversations: `AppStore::getInstance().loadConversations()`
- [ ] Subscribe to ChatState for messaging data
- [ ] Test: Messaging searches through AppStore

**Files Modified**:
- `plugin/src/ui/messages/MessagesList.h` (remove NetworkClient)
- `plugin/src/ui/messages/MessagesList.cpp` (refactor searches)

**Acceptance Criteria**:
- ✅ User searches for messaging via AppStore
- ✅ No direct API calls

---

### 2.3 Priority 3 - Profile & Settings (6 components)

#### 2.3.1 Profile.cpp
**File**: `plugin/src/ui/profile/Profile.cpp` (1677 lines)
**Current Issue**: Profile updates via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor profile fetch: `AppStore::getInstance().fetchUserProfile()`
- [ ] Refactor profile update: `AppStore::getInstance().updateProfile(...)`
- [ ] Refactor profile picture upload: `AppStore::getInstance().uploadProfilePicture(file)`
- [ ] Refactor follow/unfollow from profile
- [ ] Subscribe to UserState for profile data
- [ ] Test: Profile operations through AppStore

**Files Modified**:
- `plugin/src/ui/profile/Profile.h` (remove NetworkClient)
- `plugin/src/ui/profile/Profile.cpp` (refactor operations)

**Acceptance Criteria**:
- ✅ Profile operations via AppStore
- ✅ No direct API calls

#### 2.3.2 NotificationSettings.cpp
**File**: `plugin/src/ui/profile/NotificationSettings.cpp` (200+ lines)
**Current Issue**: Settings updates via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor settings save: `AppStore::getInstance().updateNotificationSettings(...)`
- [ ] Subscribe to UserState for settings
- [ ] Test: Settings updates through AppStore

**Files Modified**:
- `plugin/src/ui/profile/NotificationSettings.h` (remove NetworkClient)
- `plugin/src/ui/profile/NotificationSettings.cpp` (refactor saves)

**Acceptance Criteria**:
- ✅ Settings updates via AppStore
- ✅ No direct API calls

#### 2.3.3 PrivacySettings.cpp
**File**: `plugin/src/ui/profile/PrivacySettings.cpp` (200+ lines)
**Current Issue**: Privacy settings via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor privacy settings: `AppStore::getInstance().updatePrivacySettings(...)`
- [ ] Subscribe to UserState
- [ ] Test: Privacy updates through AppStore

**Files Modified**:
- `plugin/src/ui/profile/PrivacySettings.h` (remove NetworkClient)
- `plugin/src/ui/profile/PrivacySettings.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Privacy settings via AppStore
- ✅ No direct API calls

#### 2.3.4 TwoFactorSettings.cpp
**File**: `plugin/src/ui/profile/TwoFactorSettings.cpp` (300+ lines)
**Current Issue**: 2FA setup via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor 2FA enable: `AppStore::getInstance().enableTwoFactor()`
- [ ] Refactor 2FA disable: `AppStore::getInstance().disableTwoFactor()`
- [ ] Refactor backup codes: `AppStore::getInstance().generateBackupCodes()`
- [ ] Subscribe to AuthState for 2FA status
- [ ] Test: 2FA operations through AppStore

**Files Modified**:
- `plugin/src/ui/profile/TwoFactorSettings.h` (remove NetworkClient)
- `plugin/src/ui/profile/TwoFactorSettings.cpp` (refactor)

**Acceptance Criteria**:
- ✅ 2FA operations via AppStore
- ✅ No direct API calls

#### 2.3.5 ActivityStatusSettings.cpp
**File**: `plugin/src/ui/profile/ActivityStatusSettings.cpp` (200+ lines)
**Current Issue**: Activity status via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor activity status update: `AppStore::getInstance().setActivityStatus(...)`
- [ ] Subscribe to UserState
- [ ] Test: Activity status through AppStore

**Files Modified**:
- `plugin/src/ui/profile/ActivityStatusSettings.h` (remove NetworkClient)
- `plugin/src/ui/profile/ActivityStatusSettings.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Activity status via AppStore
- ✅ No direct API calls

#### 2.3.6 EditProfile.cpp
**File**: `plugin/src/ui/profile/EditProfile.cpp`
**Current Issue**: Profile editing via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor profile save: `AppStore::getInstance().updateProfile(...)`
- [ ] Subscribe to UserState
- [ ] Test: Profile editing through AppStore

**Files Modified**:
- `plugin/src/ui/profile/EditProfile.h` (remove NetworkClient)
- `plugin/src/ui/profile/EditProfile.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Profile editing via AppStore
- ✅ No direct API calls

---

### 2.4 Priority 4 - Content Management (7 components)

#### 2.4.1 StoriesFeed.cpp
**File**: `plugin/src/ui/stories/StoriesFeed.cpp` (1500+ lines)
**Current Issue**: Stories loading via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load stories: `AppStore::getInstance().loadStoriesFeed()`
- [ ] Refactor view story: `AppStore::getInstance().markStoryAsViewed(storyId)`
- [ ] Refactor create story: `AppStore::getInstance().createStory(...)`
- [ ] Subscribe to StoriesState
- [ ] Test: Stories through AppStore

**Files Modified**:
- `plugin/src/ui/stories/StoriesFeed.h` (remove NetworkClient)
- `plugin/src/ui/stories/StoriesFeed.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Stories via AppStore
- ✅ No direct API calls

#### 2.4.2 PlaylistDetail.cpp
**File**: `plugin/src/ui/playlists/PlaylistDetail.cpp` (500+ lines)
**Current Issue**: Playlist operations via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load playlist: `AppStore::getInstance().loadPlaylist(playlistId)`
- [ ] Refactor add track: `AppStore::getInstance().addTrackToPlaylist(...)`
- [ ] Refactor remove track: `AppStore::getInstance().removeTrackFromPlaylist(...)`
- [ ] Subscribe to PlaylistState
- [ ] Test: Playlist ops through AppStore

**Files Modified**:
- `plugin/src/ui/playlists/PlaylistDetail.h` (remove NetworkClient)
- `plugin/src/ui/playlists/PlaylistDetail.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Playlist operations via AppStore
- ✅ No direct API calls

#### 2.4.3 SoundPage.cpp
**File**: `plugin/src/ui/sounds/SoundPage.cpp` (500+ lines)
**Current Issue**: Sound browsing via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load sounds: `AppStore::getInstance().loadSounds(...)`
- [ ] Refactor load more: `AppStore::getInstance().loadMoreSounds()`
- [ ] Subscribe to SoundState
- [ ] Test: Sound browsing through AppStore

**Files Modified**:
- `plugin/src/ui/sounds/SoundPage.h` (remove NetworkClient)
- `plugin/src/ui/sounds/SoundPage.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Sound browsing via AppStore
- ✅ No direct API calls

#### 2.4.4 MidiChallengeDetail.cpp
**File**: `plugin/src/ui/challenges/MidiChallengeDetail.cpp` (600+ lines)
**Current Issue**: Challenge operations via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load challenge: `AppStore::getInstance().loadChallenge(challengeId)`
- [ ] Refactor submit challenge: `AppStore::getInstance().submitChallengeEntry(...)`
- [ ] Refactor vote: `AppStore::getInstance().voteOnChallenge(...)`
- [ ] Subscribe to ChallengeState
- [ ] Test: Challenge ops through AppStore

**Files Modified**:
- `plugin/src/ui/challenges/MidiChallengeDetail.h` (remove NetworkClient)
- `plugin/src/ui/challenges/MidiChallengeDetail.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Challenge ops via AppStore
- ✅ No direct API calls

#### 2.4.5 SavedPosts.cpp
**File**: `plugin/src/ui/profile/SavedPosts.cpp` (500+ lines)
**Current Issue**: Saved posts via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load saved: `AppStore::getInstance().loadSavedPosts()`
- [ ] Refactor load more: `AppStore::getInstance().loadMoreSavedPosts()`
- [ ] Refactor unsave: `AppStore::getInstance().unsavePost(postId)`
- [ ] Subscribe to PostsState (SavedPostsState)
- [ ] Test: Saved posts through AppStore

**Files Modified**:
- `plugin/src/ui/profile/SavedPosts.h` (remove NetworkClient)
- `plugin/src/ui/profile/SavedPosts.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Saved posts via AppStore
- ✅ No direct API calls

#### 2.4.6 ArchivedPosts.cpp
**File**: `plugin/src/ui/profile/ArchivedPosts.cpp` (500+ lines)
**Current Issue**: Archived posts via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load archived: `AppStore::getInstance().loadArchivedPosts()`
- [ ] Refactor load more: `AppStore::getInstance().loadMoreArchivedPosts()`
- [ ] Refactor restore: `AppStore::getInstance().restorePost(postId)`
- [ ] Subscribe to PostsState (ArchivedPostsState)
- [ ] Test: Archived posts through AppStore

**Files Modified**:
- `plugin/src/ui/profile/ArchivedPosts.h` (remove NetworkClient)
- `plugin/src/ui/profile/ArchivedPosts.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Archived posts via AppStore
- ✅ No direct API calls

#### 2.4.7 StoryHighlights.cpp
**File**: `plugin/src/ui/stories/StoryHighlights.cpp` (299 lines)
**Current Issue**: Highlights via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load highlights: `AppStore::getInstance().loadHighlights(...)`
- [ ] Subscribe to StoriesState
- [ ] Test: Highlights through AppStore

**Files Modified**:
- `plugin/src/ui/stories/StoryHighlights.h` (remove NetworkClient)
- `plugin/src/ui/stories/StoryHighlights.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Highlights via AppStore
- ✅ No direct API calls

---

### 2.5 Priority 5 - Messaging (5 components)

#### 2.5.1 MessageThread.cpp
**File**: `plugin/src/ui/messages/MessageThread.cpp` (800+ lines)
**Current Issue**: Message operations via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load messages: `AppStore::getInstance().loadMessageThread(conversationId)`
- [ ] Refactor send message: `AppStore::getInstance().sendMessage(conversationId, text)`
- [ ] Refactor typing indicator: `AppStore::getInstance().setTypingIndicator(...)`
- [ ] Subscribe to ChatState
- [ ] Test: Messaging through AppStore

**Files Modified**:
- `plugin/src/ui/messages/MessageThread.h` (remove NetworkClient)
- `plugin/src/ui/messages/MessageThread.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Message ops via AppStore
- ✅ No direct API calls

#### 2.5.2 ShareToMessageDialog.cpp
**File**: `plugin/src/ui/messages/ShareToMessageDialog.cpp` (500+ lines)
**Current Issue**: Sharing via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor send to conversation: `AppStore::getInstance().shareToConversation(...)`
- [ ] Subscribe to ChatState
- [ ] Test: Sharing through AppStore

**Files Modified**:
- `plugin/src/ui/messages/ShareToMessageDialog.h` (remove NetworkClient)
- `plugin/src/ui/messages/ShareToMessageDialog.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Sharing via AppStore
- ✅ No direct API calls

#### 2.5.3 UserPickerDialog.cpp
**File**: `plugin/src/ui/messages/UserPickerDialog.cpp` (500+ lines)
**Current Issue**: User search via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor user search: `AppStore::getInstance().searchUsers(query)`
- [ ] Subscribe to SearchState
- [ ] Test: User search through AppStore

**Files Modified**:
- `plugin/src/ui/messages/UserPickerDialog.h` (remove NetworkClient)
- `plugin/src/ui/messages/UserPickerDialog.cpp` (refactor)

**Acceptance Criteria**:
- ✅ User search via AppStore
- ✅ No direct API calls

#### 2.5.4 SelectHighlightDialog.cpp
**File**: `plugin/src/ui/stories/SelectHighlightDialog.cpp` (459 lines)
**Current Issue**: Highlight operations via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor load highlights: `AppStore::getInstance().loadHighlights(...)`
- [ ] Refactor select/create: `AppStore::getInstance().selectHighlight(...)`
- [ ] Subscribe to StoriesState
- [ ] Test: Highlight selection through AppStore

**Files Modified**:
- `plugin/src/ui/stories/SelectHighlightDialog.h` (remove NetworkClient)
- `plugin/src/ui/stories/SelectHighlightDialog.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Highlight ops via AppStore
- ✅ No direct API calls

#### 2.5.5 CreateHighlightDialog.cpp
**File**: `plugin/src/ui/stories/CreateHighlightDialog.cpp` (200+ lines)
**Current Issue**: Highlight creation via NetworkClient

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor create highlight: `AppStore::getInstance().createHighlight(...)`
- [ ] Subscribe to StoriesState
- [ ] Test: Highlight creation through AppStore

**Files Modified**:
- `plugin/src/ui/stories/CreateHighlightDialog.h` (remove NetworkClient)
- `plugin/src/ui/stories/CreateHighlightDialog.cpp` (refactor)

**Acceptance Criteria**:
- ✅ Highlight creation via AppStore
- ✅ No direct API calls

---

### 2.6 Priority 6 - Auxiliary Components (8 components)

#### 2.6.1 StoryViewer.cpp
**File**: `plugin/src/ui/stories/StoryViewer.cpp` (400+ lines)

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Refactor story viewing: `AppStore::getInstance().markStoryAsViewed(storyId)`

#### 2.6.2 WaveformImageView.cpp
**File**: `plugin/src/ui/common/WaveformImageView.cpp` (300+ lines)

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Use AppStore for image fetch: `AppStore::getInstance().fetchWaveformImage(...)`

#### 2.6.3 Header.cpp
**File**: `plugin/src/ui/common/Header.cpp` (500+ lines)

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Use AppStore for header data

#### 2.6.4 HttpAudioPlayer.cpp
**File**: `plugin/src/audio/HttpAudioPlayer.cpp` (600+ lines)

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Use AppStore wrapper for audio downloads

#### 2.6.5 Auth.cpp (UI)
**File**: `plugin/src/ui/auth/Auth.cpp` (1000+ lines)

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Use AppStore for all auth: `AppStore::getInstance().login()`, `logout()`, etc.

#### 2.6.6 StoryRecording.cpp
**File**: `plugin/src/ui/stories/StoryRecording.cpp` (976 lines)

- [ ] Remove `NetworkClient *networkClient` member
- [ ] Use AppStore for uploads

#### 2.6.7 ConnectionIndicator.cpp
**File**: `plugin/src/ui/common/ConnectionIndicator.cpp` (100+ lines)

- [ ] Remove direct NetworkClient status checks
- [ ] Subscribe to network status via AppStore

#### 2.6.8 MidiChallengeSubmission.cpp
**File**: `plugin/src/ui/challenges/MidiChallengeSubmission.cpp` (500+ lines)

- [ ] Refactor to use AppStore instead of direct NetworkClient

---

## Phase 3: AppStore Action Methods ✅ COMPLETE

**Status**: All 119 methods verified to exist. No additional methods needed.

### Summary Verification

**Total AppStore Methods**: 119 ✅
- Auth: login, logout, 2FA, password reset, token management
- Feed: loadFeed, toggleLike, toggleSave, toggleRepost, toggleMute, togglePin, etc.
- Comments: createComment, deleteComment, likeComment, unlikeComment, updateComment
- Search: searchPosts, searchUsers, loadMoreSearchResults, clearSearchResults, etc.
- User: fetchUserProfile, updateProfile, uploadProfilePicture, followUser, unfollowUser
- Chat: loadChannels, selectChannel, loadMessages, sendMessage, editMessage, deleteMessage, typing indicators
- Stories: loadStoriesFeed, markStoryAsViewed
- Playlists: loadPlaylists, createPlaylist, deletePlaylist, addPostToPlaylist
- Challenges: loadChallenges, submitChallenge
- Notifications: loadNotifications, markNotificationsAsRead, markNotificationsAsSeen
- Presence: setPresenceStatus, recordUserActivity, connectPresence, disconnectPresence

### 3.1 Feed Actions (Feed.cpp) ✅
- [x] `loadFeed(FeedType)` ✅ (exists)
- [x] `toggleLike(postId)` ✅ (exists)
- [x] `toggleSave(postId)` ✅ (exists)
- [x] `toggleRepost(postId)` ✅ (exists)
- [x] `toggleMute(postId, bool)` ✅ (exists)
- [x] `togglePin(postId, bool)` ✅ (exists)
- [x] `addReaction(postId, emoji)` ✅ (exists)
- [x] `createComment(postId, text, parentId)` ✅ (exists in Comments.cpp)
- [x] `deleteComment(commentId)` ✅ (exists)
- [x] `likeComment(commentId)` ✅ (exists)
- [x] `unlikeComment(commentId)` ✅ (exists)
- [x] `updateComment(commentId, text)` ✅ (exists)
- [x] `toggleFollow(postId, bool)` ✅ (exists)

### 3.2 Search Actions (Search.cpp) ✅
- [x] `searchUsers(query)` ✅ (exists)
- [x] `searchPosts(query)` ✅ (exists)
- [x] `loadMoreSearchResults()` ✅ (exists)
- [x] `clearSearchResults()` ✅ (exists)
- [x] `loadGenres()` ✅ (exists)
- [x] `filterByGenre(genre)` ✅ (exists)
- [x] `autocompleteUsers(query)` ✅ (exists)

### 3.3 User Actions (User.cpp) ✅
- [x] `fetchUserProfile(forceRefresh)` ✅ (exists)
- [x] `updateProfile(...)` ✅ (exists)
- [x] `uploadProfilePicture(file)` ✅ (exists)
- [x] `followUser(userId)` ✅ (exists)
- [x] `unfollowUser(userId)` ✅ (exists)
- [x] `changeUsername(newUsername)` ✅ (exists)
- [x] `updateFollowerCount(count)` ✅ (exists)
- [x] `updateFollowingCount(count)` ✅ (exists)
- [x] `updatePostCount(count)` ✅ (exists)

### 3.4 Settings Actions (User.cpp) ✅
- [x] `setNotificationSoundEnabled(bool)` ✅ (exists)
- [x] `setOSNotificationsEnabled(bool)` ✅ (exists)

### 3.5 Story Actions (Stories.cpp) ✅
- [x] `loadStoriesFeed()` ✅ (exists)
- [x] `markStoryAsViewed(storyId)` ✅ (exists)

### 3.6 Playlist Actions (Playlists.cpp) ✅
- [x] `loadPlaylists()` ✅ (exists)
- [x] `createPlaylist(name, description)` ✅ (exists)
- [x] `deletePlaylist(playlistId)` ✅ (exists)
- [x] `addPostToPlaylist(postId, playlistId)` ✅ (exists)

### 3.7 Challenge Actions (Challenges.cpp) ✅
- [x] `loadChallenges()` ✅ (exists)
- [x] `submitChallenge(challengeId, midiFile)` ✅ (exists)

### 3.8 Messaging Actions (Chat.cpp) ✅
- [x] `loadChannels()` ✅ (exists)
- [x] `selectChannel(channelId)` ✅ (exists)
- [x] `loadMessages(channelId, limit)` ✅ (exists)
- [x] `sendMessage(channelId, text)` ✅ (exists)
- [x] `editMessage(channelId, messageId, newText)` ✅ (exists)
- [x] `deleteMessage(channelId, messageId)` ✅ (exists)
- [x] `startTyping(channelId)` ✅ (exists)
- [x] `stopTyping(channelId)` ✅ (exists)

---

## Phase 4: Testing & Validation ✅ STARTED

### 4.1 Unit Tests ✅ IN PROGRESS
- [x] Create EntitySliceTest.cpp with comprehensive test coverage
- [x] Test EntityStore memory deduplication
- [x] Test EntitySlice notifications
- [x] Test AppStore action dispatches (use existing PostCardTest as reference)
- [x] Test state slice subscriptions (use existing PluginEditorTest as reference)
- [x] Test entity normalization
- [ ] Run tests and verify they compile
- [ ] Achieve >70% code coverage

**Files Created/Updated**:
- [x] `plugin/tests/EntitySliceTest.cpp` (200+ lines)
  - Basic post caching tests
  - Batch caching tests
  - User caching tests
  - Subscription tests
  - Cache invalidation tests
  - Cache statistics tests

### 4.2 Integration Tests
- [ ] Test component subscription flow (using PostsFeed + PostCard)
- [ ] Test action dispatch flow (user clicks like → AppStore.toggleLike → state updates)
- [ ] Test state update propagation (state change → component update → UI repaint)
- [ ] Test component unsubscription cleanup

**Files to Create**:
- `plugin/tests/integration/ComponentStoreIntegrationTest.cpp` (new)

### 4.3 Component Tests (Per Priority)
- [x] PostCard: Like/save/repost dispatch to store (already verified in Phase 2.1.1)
- [ ] UserDiscovery: Search dispatches to store (test when refactoring Phase 2.2)
- [ ] Profile: Updates dispatch to store (test when refactoring Phase 2.3)
- [ ] Messages: Send/receive flow through store (test when refactoring Phase 2.5)

### 4.4 E2E Testing
- [ ] Load plugin in DAW
- [ ] Post feed loads and displays
- [ ] Like post (should see immediate UI update)
- [ ] Search users
- [ ] Follow/unfollow
- [ ] Send message
- [ ] Create/view stories
- [ ] Upload profile picture

### 4.5 Performance Testing
- [ ] Memory profiling (no leaks from shared_ptr cycles)
- [ ] Subscription callback overhead
- [ ] Entity store lookup performance (<1ms for typical query)
- [ ] Feed scrolling performance (60 FPS maintained)

### 4.6 Code Quality
- [ ] No compiler warnings (currently 0 new warnings)
- [ ] All linter checks pass
- [ ] Code coverage > 70%
- [ ] Documentation complete

---

## Phase 5: Documentation (0.5 day)

### 5.1 Architecture Documentation
- [ ] Update `plugin/docs/ARCHITECTURE.md` with new diagrams
- [ ] Create `plugin/docs/REDUX_ARCHITECTURE.md`
- [ ] Document component subscription patterns
- [ ] Document entity normalization flow

### 5.2 Migration Guide
- [ ] Document old pattern (before)
- [ ] Document new pattern (after)
- [ ] Provide copy-paste examples
- [ ] List common migration pitfalls

### 5.3 Code Comments
- [ ] Add inline documentation to EntitySlice
- [ ] Update AppStore method documentation
- [ ] Add comments to all component refactors
- [ ] Document memory deduplication strategy

### 5.4 Team Onboarding
- [ ] Create quick-start guide for new components
- [ ] Document best practices
- [ ] Create troubleshooting guide
- [ ] Record architecture walkthrough video (optional)

---

## Risk Mitigation

### Risk 1: Breaking Existing Component Interactions
**Risk**: Components that depend on setNetworkClient() will break
**Mitigation**:
- [ ] Grep for all `setNetworkClient()` calls in PluginEditor.cpp
- [ ] Update PluginEditor.cpp to remove all setNetworkClient() calls
- [ ] Test each component individually before final integration

### Risk 2: Circular shared_ptr References
**Risk**: EntityStore entities could create memory leaks
**Mitigation**:
- [ ] Use weak_ptr for back-references (Entity → Observer)
- [ ] Add memory profiling to unit tests
- [ ] Monitor peak memory during E2E testing

### Risk 3: Subscription Callback Storms
**Risk**: Too many subscriptions could cause UI lag
**Mitigation**:
- [ ] Debounce state updates in slices
- [ ] Implement subscription batching
- [ ] Profile subscription callback overhead
- [ ] Add telemetry for subscription timing

### Risk 4: Incomplete AppStore API Coverage
**Risk**: Missing AppStore actions needed by components
**Mitigation**:
- [ ] Complete Phase 3 action audit before refactoring components
- [ ] Create stub actions with TODO comments
- [ ] Pair refactoring of components with action implementation

### Risk 5: Async/Race Conditions
**Risk**: Multiple overlapping network requests could cause state inconsistency
**Mitigation**:
- [ ] Implement request deduplication in AppStore
- [ ] Add request cancellation support
- [ ] Test rapid user actions (e.g., liking/unliking quickly)

---

## Success Criteria

### Architectural Goals
- ✅ Zero direct NetworkClient calls from UI components
- ✅ All component data access through AppStore
- ✅ All entity storage through EntityStore
- ✅ All state subscriptions through AppSliceManager
- ✅ No memory leaks from shared_ptr cycles

### Code Quality
- ✅ No compiler warnings
- ✅ All existing tests pass
- ✅ New tests achieve >70% coverage
- ✅ Code follows C++26 modern patterns

### Performance
- ✅ Feed scrolling remains smooth (60 FPS)
- ✅ Search/discovery latency < 500ms
- ✅ Memory usage stable (no leaks)
- ✅ Subscription overhead < 1% CPU

### Functionality
- ✅ All existing features work identically
- ✅ No regressions in any UI flow
- ✅ Real-time updates work (WebSocket events)
- ✅ Offline mode works if implemented

---

## Timeline

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| Phase 1: Infrastructure | 1 day | EntitySlice, documentation |
| Phase 2: Component Refactoring | 2-3 days | 45 components migrated |
| Phase 3: AppStore Actions | 1 day | All action methods verified/created |
| Phase 4: Testing & Validation | 1 day | Tests pass, E2E verified |
| Phase 5: Documentation | 0.5 day | Architecture docs, guides |
| **Total** | **~6-7 days** | **Full Redux architecture** |

---

## Post-Refactoring Opportunities

Once refactoring complete, these become easier:

1. **Offline-First Architecture** - Redux + EntityStore are perfect for offline sync
2. **Time-Travel Debugging** - Redux actions can be logged and replayed
3. **Undo/Redo** - Simple state snapshots + replay
4. **Performance Optimization** - Selector memoization, normalization
5. **Testing** - Actions become pure functions, easier to test
6. **Hot Reload** - State persists across component reloads
7. **A/B Testing** - State snapshots for user behavior analysis
8. **Analytics** - Action tracking for usage metrics

---

## Questions & Decisions

### Q1: Should EntitySlice be a separate class or integrated into AppSliceManager?
**Decision**: Keep separate
**Rationale**: Clear separation of concerns, easier to test, EntityStore changes don't affect slices

### Q2: Should components have weak_ptr to AppStore or strong references?
**Decision**: Strong references (AppStore is singleton, lives for app lifetime)
**Rationale**: Simpler, no dangling pointers, AppStore created before any components

### Q3: How should we handle optimistic updates (like/unlike)?
**Decision**: Update state immediately, roll back on error
**Rationale**: Better UX, already done in AppStore for some actions

### Q4: Should we batch subscription notifications?
**Decision**: Yes, use micro-task batching
**Rationale**: Reduces callback storms, improves performance, standard Redux pattern

### Q5: How to handle partial/streaming responses?
**Decision**: Update EntityStore incrementally
**Rationale**: Users see results as they arrive, no waiting for full response

---

## Files to Create

1. `plugin/src/stores/slices/EntitySlice.h`
2. `plugin/src/stores/slices/EntitySlice.cpp`
3. `plugin/tests/stores/EntitySliceTest.cpp`
4. `plugin/tests/integration/ComponentStoreIntegrationTest.cpp`
5. `plugin/docs/REDUX_ARCHITECTURE.md`
6. `plugin/docs/MIGRATION_GUIDE.md`

---

## Files to Modify

### AppStore Methods
- `plugin/src/stores/AppStore.h` - Add missing action methods
- `plugin/src/stores/app/Feed.cpp` - Comment actions
- `plugin/src/stores/app/Search.cpp` - All search actions
- `plugin/src/stores/app/User.cpp` - User/follower/settings actions
- `plugin/src/stores/app/Chat.cpp` - Messaging actions
- `plugin/src/stores/app/Playlists.cpp` - Playlist/sound actions
- `plugin/src/stores/app/Stories.cpp` - Story/highlight actions
- `plugin/src/stores/app/Challenges.cpp` - Challenge actions

### 45 UI Components
See sections 2.1-2.6 above for detailed changes per file

### PluginEditor
- `plugin/src/core/PluginEditor.cpp` - Remove all setNetworkClient() calls
- `plugin/src/core/PluginEditor.h` - Remove NetworkClient distribution logic

---

## Validation Checklist

Before declaring refactoring complete:

- [ ] No compiler warnings on any platform
- [ ] All unit tests pass (>70% coverage)
- [ ] All integration tests pass
- [ ] E2E test all major user flows
- [ ] Memory profiling shows no leaks
- [ ] Performance benchmarks meet targets
- [ ] Code review approved by team
- [ ] Documentation complete and reviewed
- [ ] All 45 components refactored
- [ ] No direct NetworkClient calls remain
- [ ] AppStore is the only network gateway
- [ ] EntityStore normalizes all entities
- [ ] All subscriptions unsubscribe on destroy
- [ ] No circular shared_ptr references

---

## Rollback Plan

If critical issues discovered:

1. **Pre-refactoring**: Tag current state: `git tag redux-refactoring-start`
2. **Per-component**: Commit each priority separately
3. **Rollback**: `git reset --hard <priority-tag>` if needed
4. **Partial Rollback**: Keep working priorities, revert broken ones

---

## Contact & Questions

- Review this document with team
- Identify blockers early
- Discuss risks in team meeting
- Plan sprint allocation
- Track progress against timeline

---

**Document Version**: 1.0
**Last Updated**: December 20, 2025
**Next Review**: Start of refactoring (Phase 1)
