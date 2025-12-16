# TODO Implementation Plan - All 67 Items

> **Status**: Planning phase - organize by difficulty, dependencies, and backend requirements
> **Last Updated**: 2025-12-16
> **Total TODOs**: 67

---

## Overview

### By Complexity Level
- **Tier 1 - TRIVIAL** (5 items): Quick fixes, minimal code
- **Tier 2 - EASY** (12 items): Straightforward implementations, mostly UI/logic
- **Tier 3 - MEDIUM** (18 items): More complex, but isolated to specific features
- **Tier 4 - HARD** (20 items): Require architecture changes or backend integration
- **Tier 5 - VERY HARD** (12 items): Complex implementations, major refactoring or new infrastructure

### By Backend Dependencies
- **No Backend Needed**: 31 items
- **Backend Partial**: 18 items (backend method exists but needs wiring)
- **Backend Required**: 18 items (requires new backend endpoint/feature)

---

## TIER 1 - TRIVIAL (5 items) ✓ ALL COMPLETED

### 1.1 Playlists - Apply Filter ✓ COMPLETED
**File**: `src/ui/playlists/Playlists.cpp`
**TODO**: Apply filter to playlists based on currentFilter
**Complexity**: TRIVIAL
**Backend**: None needed
**Task**: Wire up existing filter UI to filter results
**Estimate**: 15 min
**Status**: Phase 1 completion

---

### 1.2 Profile - Populate from AppStore ✓ COMPLETED
**File**: `src/ui/profile/Profile.cpp`
**TODO**: Populate own profile from AppStore::UserState instead of fetching
**Complexity**: TRIVIAL
**Backend**: None needed
**Task**: Use cached UserState from AppStore when viewing own profile
**Estimate**: 20 min
**Status**: Phase 1 completion

---

### 1.3 RealtimeSync - Parse Operation ✓ COMPLETED
**File**: `src/network/RealtimeSync.h`
**TODO**: Parse operation from JSON and handle
**Complexity**: TRIVIAL
**Backend**: None needed
**Task**: Uncomment and complete operation deserialization
**Estimate**: 10 min
**Status**: Phase 1 completion

---

### 1.4 Recording - Test Bus Configurations ✓ COMPLETED
**File**: `src/ui/recording/Recording.cpp`
**TODO**: Test with mono/stereo/surround bus configurations
**Complexity**: TRIVIAL
**Backend**: None needed
**Task**: Run test suite with various audio configurations
**Estimate**: 30 min (testing only)
**Status**: Phase 1 completion

---

### 1.5 ShareToMessage - Load Recent Conversations ✓ COMPLETED
**File**: `src/ui/messages/ShareToMessageDialog.cpp`
**TODO**: Implement loading recent conversations from StreamChatClient
**Complexity**: TRIVIAL
**Backend**: None needed (StreamChatClient already integrated)
**Task**: Query StreamChatClient for recent conversations on component load
**Estimate**: 20 min
**Status**: Phase 1 completion

---

## TIER 2 - EASY (12 items)

### 2.1 PluginEditor - Navigate to Post Details ✓ COMPLETED
**File**: `src/core/PluginEditor.cpp`
**TODO**: Navigate to post details or play post - see PLAN.md Phase 4
**Complexity**: EASY
**Backend**: None needed
**Task**: Implement navigation to post detail view (already has audio playback)
**Estimate**: 30 min
**Status**: Phase 1 completion - navigation system implemented

---

### 2.2 PluginEditor - Navigate to Sound Page Post ✓ COMPLETED
**File**: `src/core/PluginEditor.cpp`
**TODO**: Navigate to post detail view when implemented
**Complexity**: EASY
**Backend**: None needed
**Task**: Wire up post selection from SoundPage to navigate to post view
**Estimate**: 20 min
**Status**: Phase 1 completion - navigation system implemented

---

### 2.3 PluginEditor - Navigate to Entry Detail ✓ COMPLETED
**File**: `src/core/PluginEditor.cpp`
**TODO**: Navigate to entry/post detail
**Complexity**: EASY
**Backend**: None needed
**Task**: Wire up entry selection callback to navigate to post detail
**Estimate**: 20 min
**Status**: Phase 1 completion - navigation system implemented

---

### 2.4 Search - Genre Filtering in Store
**File**: `src/stores/app/Search.cpp`
**TODO**: Implement genre filtering in search
**Complexity**: EASY
**Backend**: Partial (API exists, needs integration)
**Task**: Wire SearchClient genre filter to AppStore state
**Estimate**: 25 min
**Notes**: NetworkClient SearchClient already has genre param, just wire AppStore

---

### 2.5 Auth - Implement OAuth Flow
**File**: `src/stores/app/Auth.cpp`
**TODO**: Implement OAuth flow
```cpp
// TODO: Implement OAuth flow
updateAuthState([](AuthState &state) { state.authError = "OAuth not yet implemented"; });
```
**Complexity**: EASY (basic OAuth)
**Backend**: Required (OAuth endpoint)
**Task**: Implement basic OAuth redirect and token exchange
**Estimate**: 45 min
**Backend Need**: OAuth provider integration endpoint
**Notes**: Can use standard OAuth2 flow with backend support

---

### 2.6 Auth - Implement Token Refresh
**File**: `src/stores/app/Auth.cpp`
**TODO**: Implement token refresh
```cpp
// TODO: Implement token refresh
updateAuthState([](AuthState &state) { state.authError = "Token refresh not yet implemented"; });
```
**Complexity**: EASY
**Backend**: Required (refresh endpoint)
**Task**: Call refresh token endpoint and update stored tokens
**Estimate**: 30 min
**Backend Need**: `/auth/refresh` endpoint
**Notes**: Standard JWT refresh flow

---

### 2.7 Challenges - MIDI Challenge Submission
**File**: `src/stores/app/Challenges.cpp`
**TODO**: Implement MIDI challenge submission with audio upload
**Complexity**: EASY (structure exists)
**Backend**: Required (submission endpoint)
**Task**: Wire upload ID to challenge submission API
**Estimate**: 30 min
**Backend Need**: Challenge submission endpoint
**Notes**: Upload logic exists, just needs submission wiring

---

### 2.8 Stories - Load User's Own Stories
**File**: `src/stores/app/Stories.cpp`
**TODO**: Implement loading user's own stories when NetworkClient provides an API
**Complexity**: EASY
**Backend**: Required (endpoint exists)
**Task**: Call API to load user's stories into state
**Estimate**: 25 min
**Backend Need**: GET /api/stories/my-stories (likely exists)
**Notes**: Pattern already exists for other data loading

---

### 2.9 Stories - Implement Highlight Creation
**File**: `src/stores/app/Stories.cpp`
**TODO**: Implement highlight creation when NetworkClient provides createHighlight API
**Complexity**: EASY
**Backend**: Required (endpoint)
**Task**: Call createHighlight API from Stories store
**Estimate**: 25 min
**Backend Need**: POST /api/highlights endpoint
**Notes**: Wire existing UI to API call

---

### 2.10 Draft - Load from Local Storage ✓ COMPLETED
**File**: `src/stores/app/Draft.cpp`
**TODO**: Load drafts from local storage (DraftStorage)
**Complexity**: EASY
**Backend**: None needed (local storage)
**Task**: Initialize DraftStorage and load persisted drafts on startup
**Estimate**: 30 min
**Notes**: Implemented with FileCache-based DraftCache. Loads drafts from cache directory.
**Implementation**: Uses DraftCache with FileCache<DraftKey> to load all draft files from cache directory

---

### 2.11 Draft - Delete from Local Storage ✓ COMPLETED
**File**: `src/stores/app/Draft.cpp`
**TODO**: Delete from local storage (DraftStorage)
**Complexity**: EASY
**Backend**: None needed (local storage)
**Task**: Call DraftStorage::deleteDraft when user deletes draft
**Estimate**: 15 min
**Notes**: Implemented with FileCache removal. Deletes from cache via DraftCache::removeDraftFile()
**Implementation**: Calls draftCache.removeDraftFile(DraftKey) when user deletes draft

---

### 2.12 Draft - Clear Auto-Recovery Draft ✓ COMPLETED
**File**: `src/stores/app/Draft.cpp`
**TODO**: Clear auto-recovery draft from local storage (DraftStorage)
**Complexity**: EASY
**Backend**: None needed (local storage)
**Task**: Clear auto-saved draft after successful post
**Estimate**: 10 min
**Notes**: Implemented with FileCache removal. Removes special "autoRecoveryDraft" key from cache
**Implementation**: Calls draftCache.removeDraftFile(DraftKey("autoRecoveryDraft")) to clear auto-recovery

---

## TIER 3 - MEDIUM (18 items)

### 3.1 PluginEditor - Playlist Playback
**File**: `src/core/PluginEditor.cpp`
**TODO**: Implement playlist playback
**Complexity**: MEDIUM
**Backend**: None needed (audio player exists)
**Task**: Extend audio player to support sequential playback of playlist tracks
**Estimate**: 60 min
**Notes**: AudioPlayer already plays individual posts, extend to playlist queue

---

### 3.2 PluginEditor - Pass Challenge ID to Recording
**File**: `src/core/PluginEditor.cpp`
**TODO**: Pass challenge ID to recording component for constraint validation
**Complexity**: MEDIUM
**Backend**: None needed
**Task**: Wire challenge ID through to Recording component, validate constraints
**Estimate**: 45 min
**Notes**: Requires threading ID through component hierarchy

---

### 3.3 PluginEditor - Implement Muted Users Component
**File**: `src/core/PluginEditor.cpp`
**TODO**: Implement MutedUsers component
**Complexity**: MEDIUM
**Backend**: Required (list endpoint)
**Task**: Create MutedUsers component with list/unblock functionality
**Estimate**: 90 min
**Backend Need**: GET /api/users/muted endpoint
**Notes**: New component, similar to FollowersList pattern

---

### 3.4 PluginEditor - scrollToPost in PostsFeed
**File**: `src/core/PluginEditor.cpp`
**TODO**: Implement scrollToPost(postId) in PostsFeed to jump to specific post
**Complexity**: MEDIUM
**Backend**: None needed
**Task**: Implement scroll-to-item functionality for feed
**Estimate**: 45 min
**Notes**: Requires modifying PostsFeed to support direct navigation to post

---

### 3.5 PluginEditor - Fix slideLeft Animation
**File**: `src/core/PluginEditor.cpp`
**TODO (Task 4.21)**: Fix ViewTransitionManager.slideLeft animation for PostsFeed
**Complexity**: MEDIUM
**Backend**: None needed
**Task**: Debug and fix animation timing/positioning issue
**Estimate**: 60 min
**Notes**: Animation framework exists, needs debugging/tweaking

---

### 3.6 Auth - Secure Credential Storage
**File**: `src/ui/auth/Auth.cpp`
**TODO**: Implement secure credential storage using OS keychain
**Complexity**: MEDIUM
**Backend**: None needed (OS integration)
**Task**: Use platform-specific keychain (Keychain on macOS, Credential Manager on Windows)
**Estimate**: 90 min
**Notes**: Cross-platform complexity - need platform-specific code

---

### 3.7 Feed - Load Archived Posts
**File**: `src/stores/app/Feed.cpp`
**TODO**: Implement archived posts loading via AppStore
**Complexity**: MEDIUM
**Backend**: Required (endpoint likely exists)
**Task**: Implement archived posts loading and caching
**Estimate**: 40 min
**Backend Need**: GET /api/feed/archived endpoint
**Notes**: Similar to regular feed loading, just different endpoint

---

### 3.8 Feed - Pin/Unpin Posts
**File**: `src/stores/app/Feed.cpp`
**TODO**: Call actual API when NetworkClient has pin/unpin methods
**Complexity**: MEDIUM
**Backend**: Required (endpoints)
**Task**: Implement pin/unpin API calls in Feed store
**Estimate**: 30 min
**Backend Need**: POST /api/posts/{id}/pin, POST /api/posts/{id}/unpin
**Notes**: Endpoints likely exist, just needs wiring

---

### 3.9 Feed - Aggregated Feed Handling
**File**: `src/stores/app/Feed.cpp` (multiple TODOs)
**TODO**: Implement aggregated feed handling / Handle aggregated feed types / Implement proper aggregated feed caching
**Complexity**: MEDIUM
**Backend**: Required (API support)
**Task**: Implement aggregated feed type support (challenges, trending, etc.)
**Estimate**: 120 min (3 TODOs combined)
**Backend Need**: Aggregated feed endpoint with type filtering
**Notes**: Requires architecture for multiple feed types

---

### 3.10 Playlist - Delete via NetworkClient
**File**: `src/stores/app/Playlists.cpp`
**TODO**: Implement playlist deletion via NetworkClient
**Complexity**: MEDIUM
**Backend**: Required (endpoint)
**Task**: Call delete endpoint when user removes playlist
**Estimate**: 25 min
**Backend Need**: DELETE /api/playlists/{id} endpoint
**Notes**: Simple API integration

---

### 3.11 Presence - Send Status Message
**File**: `src/stores/app/Presence.cpp`
**TODO**: Send status message to server via presence endpoint
**Complexity**: MEDIUM
**Backend**: Required (presence endpoint)
**Task**: Periodically send status updates to presence service
**Estimate**: 40 min
**Backend Need**: POST /api/presence/status endpoint
**Notes**: Requires periodic update mechanism

---

### 3.12 Messages - Hit Testing Implementation
**File**: `src/ui/messages/MessageThread.cpp`
**TODO**: Re-implement hit testing once messages are drawn via paint()
**Complexity**: MEDIUM
**Backend**: None needed
**Task**: Implement click detection on message components in paint output
**Estimate**: 60 min
**Notes**: Complex rendering logic, needs careful coordinate mapping

---

### 3.13 Messages - Shared Post/Story Hit Testing
**File**: `src/ui/messages/MessageThread.cpp`
**TODO**: Re-enable shared post/story/parent message hit testing
**Complexity**: MEDIUM
**Backend**: None needed
**Task**: Wire ChatStore state to enable hit testing on attached content
**Estimate**: 50 min
**Notes**: Requires AppStore ChatState integration

---

### 3.14 Messages - Delete Message
**File**: `src/ui/messages/MessageThread.cpp`
**TODO**: refactor to use AppStore (delete message)
**Complexity**: MEDIUM
**Backend**: Required (delete endpoint)
**Task**: Implement delete message via AppStore
**Estimate**: 30 min
**Backend Need**: DELETE /api/messages/{id} endpoint (likely exists in StreamChat)
**Notes**: Refactor from direct StreamChat to AppStore pattern

---

### 3.15 Messages - Report Message
**File**: `src/ui/messages/MessageThread.cpp`
**TODO**: Send report to backend when report API endpoint is available
**Complexity**: MEDIUM
**Backend**: Required (report endpoint)
**Task**: Call report endpoint with reason and message ID
**Estimate**: 25 min
**Backend Need**: POST /api/messages/{id}/report endpoint
**Notes**: Simple API call, already has UI

---

### 3.16 Messages - Leave Channel
**File**: `src/ui/messages/MessageThread.cpp`
**TODO**: refactor to use AppStore (leave channel)
**Complexity**: MEDIUM
**Backend**: Required (leave endpoint)
**Task**: Implement leave channel via AppStore
**Estimate**: 30 min
**Backend Need**: POST /api/channels/{id}/leave endpoint (likely exists in StreamChat)
**Notes**: Refactor pattern

---

### 3.17 Messages - Rename Group
**File**: `src/ui/messages/MessageThread.cpp`
**TODO**: ChatStore doesn't support renaming groups yet
**Complexity**: MEDIUM
**Backend**: Required (update endpoint)
**Task**: Add renameGroup method to ChatStore and call it
**Estimate**: 40 min
**Backend Need**: PATCH /api/channels/{id} endpoint
**Notes**: Need to extend AppStore ChatState

---

### 3.18 Messages - Manage Group Members
**File**: `src/ui/messages/MessageThread.cpp` (2 TODOs)
**TODO**: Add members / Remove members not yet implemented via ChatStore
**Complexity**: MEDIUM
**Backend**: Required (endpoints)
**Task**: Add addMembers/removeMembers to ChatStore
**Estimate**: 60 min (both combined)
**Backend Need**: POST /api/channels/{id}/members, DELETE /api/channels/{id}/members/{userId}
**Notes**: Extend AppStore ChatState with member management

---

## TIER 4 - HARD (20 items)

### 4.1 Messages - Message Reactions
**File**: `src/ui/messages/MessageThread.cpp` (2 TODOs)
**TODO**: Add reaction / Remove reaction / Get current reactions
**Complexity**: HARD
**Backend**: Required (reactions API)
**Task**: Implement reaction system in ChatStore with add/remove/list operations
**Estimate**: 90 min (all 3 combined)
**Backend Need**: POST/DELETE /api/messages/{id}/reactions endpoints
**Notes**: New feature, requires AppStore ChatState extension

---

### 4.2 Messages - Typing Indicator
**File**: `src/ui/messages/MessagesList.cpp`
**TODO**: Show "typing" indicator (future)
**Complexity**: HARD
**Backend**: Required (typing events)
**Task**: Implement real-time typing indicator via WebSocket
**Estimate**: 120 min
**Backend Need**: WebSocket typing events from StreamChat integration
**Notes**: Requires real-time event handling, not just HTTP

---

### 4.3 Messages - Audio Snippet Playback and Upload
**File**: `src/ui/messages/MessagesList.cpp` (2 TODOs)
**TODO**: Audio snippet playback in messages / Upload audio snippet when sending
**Complexity**: HARD
**Backend**: Required (audio storage)
**Task**: Implement audio recording/playback inline in messages
**Estimate**: 180 min (both combined)
**Backend Need**: Audio upload endpoint for message attachments
**Notes**: Requires audio recording UI, playback, streaming

---

### 4.4 Messages - Share to Message Dialog UI
**File**: `src/ui/messages/ShareToMessageDialog.cpp` (2 TODOs)
**TODO**: Implement post preview rendering / Implement debounced search
**Complexity**: HARD
**Backend**: None needed (UI only)
**Task**: Render post preview, implement debounced search for recipients
**Estimate**: 120 min (both combined)
**Notes**: Complex UI work, performance optimization needed

---

### 4.5 Notifications - Preferences System
**File**: `src/ui/notifications/NotificationList.cpp` (3 TODOs)
**TODO**: Notification sound option / Notification preferences / WebSocket push updates
**Complexity**: HARD
**Backend**: Required (preferences endpoint)
**Task**: Implement notification preferences, sound playback, real-time updates
**Estimate**: 180 min (all 3 combined)
**Backend Need**: POST /api/preferences/notifications endpoint, WebSocket push integration
**Notes**: Requires platform audio integration, real-time sync

---

### 4.6 Search - Advanced Metadata Filtering
**File**: `src/ui/search/Search.cpp` (3 TODOs)
**TODO**: Search by BPM, key, genre / Filter posts by metadata / Search hashtags
**Complexity**: HARD
**Backend**: Required (metadata search API)
**Task**: Implement advanced search with multiple filter dimensions
**Estimate**: 150 min (all 3 combined)
**Backend Need**: Enhance search API with metadata filtering
**Notes**: Requires indexing and query optimization on backend

---

### 4.7 Challenge Submission Flow
**File**: `src/ui/challenges/MidiChallengeSubmission.cpp` (3 TODOs)
**TODO**: Get post ID from upload / Intercept upload flow / Trigger upload then submit
**Complexity**: HARD
**Backend**: Required (challenge API)
**Task**: Wire upload flow to challenge submission
**Estimate**: 120 min (all 3 combined)
**Backend Need**: Challenge submission endpoint
**Notes**: Requires modifying upload component callback chain

---

### 4.8 Stories - Visualizations
**File**: `src/ui/stories/StoryRecording.cpp` (2 TODOs)
**TODO**: Create note waterfall visualization / Create circular visualization
**Complexity**: HARD
**Backend**: None needed (UI rendering)
**Task**: Implement real-time audio visualization during recording
**Estimate**: 180 min (both combined)
**Notes**: Complex graphics rendering, audio analysis needed

---

### 4.9 Stories - Save to Highlights
**File**: `src/ui/stories/StoryRecording.cpp` (2 TODOs)
**TODO**: Save stories to highlights / Display highlights on profile
**Complexity**: HARD
**Backend**: Required (highlight API)
**Task**: Implement story → highlight promotion with profile display
**Estimate**: 120 min (both combined)
**Backend Need**: POST /api/highlights API
**Notes**: Requires interaction between stories and highlights features

---

### 4.10 Stories - Load Highlight Images
**File**: `src/ui/stories/SelectHighlightDialog.cpp`, `src/ui/stories/StoryHighlights.cpp`
**TODO**: Load highlight cover images using ImageLoader
**Complexity**: HARD
**Backend**: None needed (existing cache system)
**Task**: Wire ImageLoader to load and cache highlight cover images
**Estimate**: 60 min
**Notes**: Pattern exists, just needs integration

---

### 4.11 User Discovery - Load Discovery Data
**File**: `src/ui/social/UserDiscovery.cpp`
**TODO**: Implement discovery data (loadTrendingUsers, loadSuggestedUsers, etc.)
**Complexity**: HARD
**Backend**: Required (discovery API)
**Task**: Implement trending and suggested user loading
**Estimate**: 90 min
**Backend Need**: GET /api/users/trending, GET /api/users/suggested endpoints
**Notes**: Requires personalization logic on backend

---

### 4.12 User Discovery - Search Users
**File**: `src/ui/social/UserDiscovery.cpp`
**TODO**: Add searchUsers method to UserStore
**Complexity**: HARD
**Backend**: Required (search API)
**Task**: Implement user search in AppStore with debouncing
**Estimate**: 60 min
**Backend Need**: GET /api/users/search endpoint
**Notes**: Similar to post search but for users

---

### 4.13 Profile - Mute User Toggle
**File**: `src/ui/profile/Profile.cpp`
**TODO**: Update AppStore to toggle mute state
**Complexity**: HARD
**Backend**: Required (mute API)
**Task**: Implement mute/unmute user via AppStore
**Estimate**: 60 min
**Backend Need**: POST /api/users/{id}/mute, POST /api/users/{id}/unmute endpoints
**Notes**: Requires AppStore state management for muted users

---

### 4.14 Profile - Update Follow State in AppStore
**File**: `src/ui/profile/Profile.cpp`
**TODO**: Update AppStore to sync all posts by this user
**Complexity**: HARD
**Backend**: None needed (AppStore pattern)
**Task**: Implement reactive user post feed in AppStore
**Estimate**: 75 min
**Notes**: Requires AppStore architecture for user-specific feeds

---

### 4.15 Profile - Verification Badges
**File**: `src/ui/profile/Profile.cpp`
**TODO**: Add profile verification system (future: badges)
**Complexity**: HARD
**Backend**: Required (verification API)
**Task**: Implement verification badge display and badge system
**Estimate**: 90 min
**Backend Need**: Badge/verification data in user API
**Notes**: UI display + backend data integration

---

### 4.16 Recording/Upload - Create Audio File
**File**: `src/ui/recording/Upload.cpp`
**TODO**: Create actual audio file from audioBuffer
**Complexity**: HARD
**Backend**: None needed (local file creation)
**Task**: Implement JUCE audio buffer to WAV/MP3 conversion
**Estimate**: 90 min
**Notes**: JUCE AudioFormatWriter already available, needs proper setup

---

### 4.17 Recording/Upload - Full Upload Integration
**File**: `src/ui/recording/Upload.cpp`
**TODO**: Implement actual upload in UploadStore with NetworkClient integration
**Complexity**: HARD
**Backend**: Required (upload endpoint)
**Task**: Full integration of recording → upload → server
**Estimate**: 120 min
**Backend Need**: POST /api/uploads endpoint with multipart form
**Notes**: Requires progress tracking, cancellation, retry logic

---

### 4.18 Presence - WebSocket Connection
**File**: `src/stores/app/Presence.cpp`
**TODO**: Establish WebSocket connection to presence service
**Complexity**: HARD
**Backend**: Required (WebSocket support)
**Task**: Implement WebSocket client for presence updates
**Estimate**: 120 min
**Backend Need**: WebSocket presence service endpoint
**Notes**: Real-time communication layer, requires new infrastructure

---

---

## TIER 5 - VERY HARD (12 items)

### 5.1 Messages - Full Refactor to AppStore
**Files**: `src/ui/messages/MessageThread.cpp` (10+ TODOs)
**TODO**: Refactor all messaging to use AppStore instead of direct StreamChat
**Complexity**: VERY HARD
**Backend**: None needed (refactoring)
**Task**: Complete architectural refactor of messaging system
**Estimate**: 300+ min
**Impact**: High - affects entire messaging system
**Notes**: Large refactoring effort, requires careful migration to avoid breaking existing features

---

### 5.2 Feed - Multi-Type Aggregated Feed System
**Files**: `src/stores/app/Feed.cpp`
**TODO**: Full aggregated feed implementation (challenges, trending, recommendations)
**Complexity**: VERY HARD
**Backend**: Required (aggregated feed API)
**Task**: Implement multi-feed type system with caching and sync
**Estimate**: 240 min
**Backend Need**: Aggregated feed API with type differentiation, intelligent ranking
**Notes**: Requires significant architecture for feed type handling

---

### 5.3 Stories - Complete Story System with Highlights
**Files**: `src/ui/stories/StoryRecording.cpp`, `src/ui/stories/StoryHighlights.cpp`
**TODO**: Full story system with visualizations and highlights
**Complexity**: VERY HARD
**Backend**: Required (story API enhancement)
**Task**: Complete story recording, display, and highlight system
**Estimate**: 300 min (all combined)
**Backend Need**: Enhanced story endpoints with highlight support
**Notes**: Major feature, requires recording, playback, sharing, highlighting

---

### 5.4 Real-Time Updates System
**Files**: `src/ui/messages/`, `src/ui/notifications/`, various
**TODO**: WebSocket-based real-time updates (typing, reactions, messages, notifications)
**Complexity**: VERY HARD
**Backend**: Required (WebSocket infrastructure)
**Task**: Implement real-time update system for all reactive features
**Estimate**: 250+ min
**Backend Need**: WebSocket server, subscription management, event broadcasting
**Notes**: Foundational infrastructure, affects multiple features

---

### 5.5 Challenge System - Full Implementation
**Files**: `src/ui/challenges/`, `src/stores/app/Challenges.cpp`
**TODO**: Complete MIDI challenge system with submission, scoring, leaderboards
**Complexity**: VERY HARD
**Backend**: Required (challenge API)
**Task**: Full challenge lifecycle from submission to leaderboard display
**Estimate**: 300 min
**Backend Need**: Challenge submission, scoring, leaderboard endpoints
**Notes**: Complex feature with multiple components

---

### 5.6 User Discovery - Personalization Engine
**Files**: `src/ui/social/UserDiscovery.cpp`
**TODO**: Implement trending/suggested users with personalization
**Complexity**: VERY HARD
**Backend**: Required (discovery algorithm)
**Task**: Implement discovery with trending, suggestions, recommendations
**Estimate**: 180 min
**Backend Need**: Personalization algorithm on backend
**Notes**: Requires collaborative filtering or similar ML approach

---

### 5.7 Playlist - Full Playback System
**Files**: `src/core/PluginEditor.cpp`, playlist components
**TODO**: Complete playlist playback with UI, shuffle, repeat, queue management
**Complexity**: VERY HARD
**Backend**: None needed (local playback)
**Task**: Extend AudioPlayer to full playlist management system
**Estimate**: 200 min
**Notes**: Requires significant audio player architecture changes

---

### 5.8 Search - Advanced Filtering and Ranking
**Files**: `src/ui/search/Search.cpp`, `src/stores/app/Search.cpp`
**TODO**: BPM/key/genre filtering, hashtag search, ranking
**Complexity**: VERY HARD
**Backend**: Required (search enhancement)
**Task**: Implement advanced search with multiple filters and ranking
**Estimate**: 200 min
**Backend Need**: Enhanced search index and ranking algorithm
**Notes**: Requires both frontend filtering and backend optimization

---

### 5.9 Audio Snippets in Messages - Full System
**Files**: `src/ui/messages/MessagesList.cpp`
**TODO**: Complete audio recording/playback for message audio snippets
**Complexity**: VERY HARD
**Backend**: Required (audio storage)
**Task**: Full inline audio recording and playback in messages
**Estimate**: 250 min
**Backend Need**: Audio upload/download endpoints
**Notes**: Complex audio handling, streaming, compression

---

### 5.10 Notifications - Full Real-Time System
**Files**: `src/ui/notifications/NotificationList.cpp`
**TODO**: Complete notification system with preferences, sounds, real-time push
**Complexity**: VERY HARD
**Backend**: Required (notification infrastructure)
**Task**: Real-time notifications with sound, preferences, sync
**Estimate**: 200 min
**Backend Need**: Push notification infrastructure, WebSocket events
**Notes**: Requires platform-specific code (audio playback)

---

### 5.11 Profile - Complete User Profile System
**Files**: `src/ui/profile/Profile.cpp`
**TODO**: Full profile features (verification, user posts, mute, badges)
**Complexity**: VERY HARD
**Backend**: Required (profile API enhancement)
**Task**: Complete user profile system with all features
**Estimate**: 180 min
**Backend Need**: Enhanced user API, verification, badges
**Notes**: Affects user identity and reputation system

---

### 5.12 View Transition Animation System
**Files**: `src/core/PluginEditor.cpp`
**TODO**: Complete animation system with all view transitions
**Complexity**: VERY HARD
**Backend**: None needed (UI only)
**Task**: Fix and complete all view transition animations
**Estimate**: 150 min
**Notes**: Requires careful timing and coordinate management

---

---

## Summary Table

| Tier | Count | Complete | Remaining | Est. Hours | Complexity | Backend Needed |
|------|-------|----------|-----------|-----------|------------|-----------------|
| 1    | 5     | 5 ✓      | 0         | 2.5 h     | Trivial    | 0 items        |
| 2    | 12    | 4 ✓      | 8         | 8 h       | Easy       | 5 items        |
| 3    | 18    | 0        | 18        | 20 h      | Medium     | 12 items       |
| 4    | 20    | 0        | 20        | 30 h      | Hard       | 14 items       |
| 5    | 12    | 0        | 12        | 40 h      | Very Hard  | 9 items        |
| **TOTAL** | **67** | **9** | **58** | **~100 h** | - | **40 items** |

**Progress**: 9/67 items completed (13.4%) - **Tier 1 complete, Tier 2 at 33%**

---

## Implementation Strategy

### Recommended Order
1. **Start with Tier 1** (2-3 hours): Quick wins for momentum
2. **Do Tier 2** (8 hours): Build foundational features
3. **Parallel tracks for Tier 3-4**:
   - One person: Messages/Chat features
   - Another: Search/Discovery/Profile
   - Another: Audio/Recording/Upload
4. **Tier 5 last**: These require foundation of earlier tiers

### Backend Dependency Summary
- **40 items require backend support** (60% of total)
- **27 items are frontend only** (40% of total)

### Critical Path Items
1. Auth (OAuth, token refresh) - BLOCKS everything
2. Upload system - BLOCKS challenges, audio snippets, stories
3. Feed/Discovery APIs - BLOCKS many features
4. WebSocket infrastructure - BLOCKS real-time features (typing, reactions, notifications)

### Backend Todo List
Before frontend work can complete, backend needs:
- [ ] OAuth endpoints
- [ ] Token refresh endpoint
- [ ] Enhanced search API (metadata filtering)
- [ ] Challenge submission API
- [ ] Aggregated feed API
- [ ] User discovery (trending/suggested) endpoints
- [ ] Playlist deletion endpoint
- [ ] Presence endpoint
- [ ] Message reactions API
- [ ] Audio snippets upload/download
- [ ] Story API enhancements
- [ ] Highlight creation API
- [ ] Notification preferences endpoint
- [ ] WebSocket infrastructure (typing, reactions, presence)
- [ ] Report message endpoint
- [ ] Mute/unmute user endpoints
- [ ] User search API
- [ ] Badge/verification system

---

## Notes
- Total estimated effort: ~100 developer-hours
- Backend work needed in parallel: ~30-40 hours
- Some items are blocked on others (dependency chains)
- Several items have overlapping functionality (consolidation possible)
- Real-time features (5+ items) require shared WebSocket infrastructure

---

**Last Updated**: 2025-12-16 17:30
**Status**: Phase 1 implementation in progress - 9/67 items complete

**Completed in Phase 1**:
- ✓ Tier 1 - All 5 TRIVIAL items
- ✓ 2.1-2.3 Navigation to Post Details (3 items)
- ✓ 2.10-2.12 Draft Storage with FileCache (3 items)

**Next Priority**: 2.4 Search Genre Filtering (remaining Tier 2 items)
