# WebSocket & Real-time Features Implementation Plan

Comprehensive todo list for fixing WebSocket architecture and real-time features.

## 1. Like/Comment Notifications via WebSocket

### 1.1 Wire Like Notifications
**File**: `backend/internal/handlers/feed.go`
**Task**: When a post is liked, broadcast to:
- Post owner (direct notification)
- Post owner's followers (if post is public)
- Broadcast the current like count for the post

**Steps**:
- [ ] Find like endpoint handler (POST /posts/:id/likes)
- [ ] After successful like insert, call `h.wsHandler.NotifyLike()`
- [ ] Include payload: `{post_id, user_id, username, emoji, like_count}`
- [ ] Check post visibility (only broadcast if public)
- [ ] Test: Send like, verify owner receives WebSocket event
- [ ] Test: Verify like count updates in real-time

**Related Code**:
- Already exists: `handler.go:NotifyLike()` - just needs to be called
- Already exists: Message type `MessageTypeLike` with payload struct

### 1.2 Wire Comment Notifications
**File**: `backend/internal/handlers/feed.go`
**Task**: When a post is commented on, broadcast to:
- Post owner (someone commented on their post)
- Followers of the post (who are watching comments)

**Steps**:
- [ ] Find comment endpoint handler (POST /posts/:id/comments)
- [ ] After successful comment insert, call `h.wsHandler.NotifyComment()`
- [ ] Include payload: `{post_id, user_id, username, comment_text, comment_count}`
- [ ] Store comment in database first, get new count
- [ ] Broadcast only to post owner (not all followers to reduce noise)
- [ ] Test: Send comment, verify owner receives notification
- [ ] Test: Verify comment count updates

**Related Code**:
- Check if `NotifyComment()` exists in `handler.go`
- If not, add it (model after `NotifyLike()`)

### 1.3 Wire Follow Notifications
**File**: `backend/internal/handlers/social.go`
**Task**: When user is followed, notify them

**Steps**:
- [ ] Find follow endpoint (POST /users/:id/follow)
- [ ] After follow insert, call `h.wsHandler.NotifyFollow()`
- [ ] Include payload: `{follower_id, follower_username, follower_avatar}`
- [ ] Verify already wired (might already be done)
- [ ] Test: Follow user, verify they get notification

### 1.4 Wire Unlike Notifications
**File**: `backend/internal/handlers/feed.go`
**Task**: When a post is unliked, update like count in real-time

**Steps**:
- [ ] Find unlike endpoint (DELETE /posts/:id/likes/:id)
- [ ] After unlike delete, call `h.wsHandler.BroadcastLikeCountUpdate()`
- [ ] Include payload: `{post_id, new_like_count}`
- [ ] Verify this is already implemented
- [ ] Test: Unlike post, verify count updates

---

## 2. Activity Feed Real-time Updates

### 2.1 Research getstream.io Activity API
**Task**: Understand how to get activity updates from Stream

**Steps**:
- [ ] Check backend Stream client initialization (backend/internal/stream/client.go or similar)
- [ ] Find how activities are created (posts, likes, follows)
- [ ] Research Stream API docs for activity subscriptions/webhooks
- [ ] Check if we can use Stream's API to trigger WebSocket events
- [ ] Determine: Should we use webhooks or poll Stream API?

### 2.2 Add Activity Update Message Type
**File**: `backend/internal/websocket/message.go`
**Task**: Define WebSocket message for activity updates

**Steps**:
- [ ] Add message type: `MessageTypeActivityUpdate`
- [ ] Define payload struct: `ActivityUpdatePayload`
- [ ] Include fields: `{activity_id, actor_id, actor_username, verb (posted/liked/followed), object_type, object_id, timestamp}`
- [ ] Add to message type enum

### 2.3 Wire Activity Events to WebSocket
**File**: `backend/internal/handlers/activity.go` (or where activities are created)
**Task**: Broadcast activity updates to followers

**Steps**:
- [ ] Identify where activities are logged to Stream (posts, likes, follows)
- [ ] After creating activity in Stream, call `h.wsHandler.BroadcastActivity()`
- [ ] Send to: followers of the actor (or specific followers watching this type of activity)
- [ ] Include full activity payload
- [ ] Test: Post something, verify followers get real-time update

### 2.4 Implement Activity Timeline Endpoint with Pagination
**File**: `backend/internal/handlers/activity.go`
**Task**: Ensure GET /activity/timeline returns paginated results (if not already)

**Steps**:
- [ ] Check if endpoint exists
- [ ] If not, implement: `GET /api/v1/activity/timeline?limit=20&offset=0`
- [ ] Fetch from Stream API: `timeline_feed.get_activities(limit, offset)`
- [ ] Return ordered by timestamp, newest first
- [ ] Include user info (username, avatar)

### 2.5 Web Frontend: Subscribe to Activity Updates
**File**: `web/src/hooks/useWebSocket.ts`
**Task**: Listen for activity updates and update store

**Steps**:
- [ ] Add listener for `MessageTypeActivityUpdate`
- [ ] Update activity feed store on message
- [ ] Prepend new activities to timeline (they're newest)
- [ ] Handle pagination: Don't re-fetch if we have the item
- [ ] Test: Follow someone, post something, verify feed updates in real-time

### 2.6 Plugin: Display Activity Timeline
**File**: `plugin/src/ui/feed/Timeline.cpp`
**Task**: Add activity timeline UI to plugin

**Steps**:
- [ ] Check if timeline UI exists
- [ ] If not, create it (show recent activities from following)
- [ ] Wire to feed store
- [ ] Listen for WebSocket activity updates
- [ ] Update UI in real-time

---

## 3. Live Engagement Metrics (Like/Comment Counts)

### 3.1 Add Engagement Update Message Type
**File**: `backend/internal/websocket/message.go`
**Task**: Define message for engagement metric updates

**Steps**:
- [ ] Add message type: `MessageTypeEngagementUpdate`
- [ ] Define payload: `{post_id, like_count, comment_count, repost_count}`
- [ ] Use this for all like/comment/repost updates
- [ ] This consolidates engagement metrics in one message

### 3.2 Broadcast Engagement Metrics After Actions
**File**: `backend/internal/handlers/feed.go`
**Task**: After like/unlike/comment/uncomment, send engagement update

**Steps**:
- [ ] After like/unlike, broadcast engagement update to all viewing the post
- [ ] After comment/uncomment, broadcast engagement update
- [ ] Include all current counts
- [ ] Target: All users who have post open (or all followers if public)
- [ ] Test: Like a post, verify other clients see count increase immediately

### 3.3 Web: Update UI on Engagement Changes
**File**: `web/src/components/feed/Post.tsx`
**Task**: React to engagement updates instantly

**Steps**:
- [ ] Add listener for `MessageTypeEngagementUpdate`
- [ ] Find post in feed by ID
- [ ] Update like_count, comment_count, repost_count
- [ ] Trigger re-render
- [ ] Test: Open post in two browser windows, like in one, verify other updates

### 3.4 Plugin: Update Engagement Display
**File**: `plugin/src/ui/feed/PostCard.cpp`
**Task**: Update displayed counts in real-time

**Steps**:
- [ ] Listen for engagement updates in AppStore
- [ ] Update post metrics
- [ ] Trigger UI repaint
- [ ] Test: Like from web/CLI, verify plugin shows new count

---

## 4. Typing Indicators

### 4.1 Add Typing Message Type
**File**: `backend/internal/websocket/message.go`
**Task**: Define typing indicator message

**Steps**:
- [ ] Add message type: `MessageTypeTyping` and `MessageTypeStoppedTyping`
- [ ] Define payload: `{post_id, user_id, username}`
- [ ] Design: Show typing for 5 seconds max (reset on each keystroke)

### 4.2 Backend Typing Handler
**File**: `backend/internal/websocket/handler.go`
**Task**: Register typing message handler

**Steps**:
- [ ] Register handler for incoming "typing" messages
- [ ] Validate payload has post_id
- [ ] Broadcast to other users viewing the same post: `{user typing on post X}`
- [ ] Auto-stop typing after 5 seconds of no activity
- [ ] Test: Send typing message, verify broadcast

### 4.3 Web: Send Typing Events
**File**: `web/src/components/Comments/CommentInput.tsx`
**Task**: Send typing indicators when user is composing

**Steps**:
- [ ] Add onChange handler to comment input
- [ ] Debounce: Send typing every 500ms while active
- [ ] Stop: Send `typing_stopped` on blur or submit
- [ ] Test: Start typing in comment, verify others see "X is typing..."

### 4.4 Web: Display Typing Indicators
**File**: `web/src/components/Comments/CommentList.tsx`
**Task**: Show "X is typing..." in comment section

**Steps**:
- [ ] Add listener for typing messages on post
- [ ] Filter by post_id
- [ ] Display below comment list: "X and Y are typing..."
- [ ] Remove after 5 seconds
- [ ] Test: Multiple users typing, verify display

### 4.5 Plugin: Send Typing Events
**File**: `plugin/src/ui/Comments/CommentInputField.cpp`
**Task**: Send typing indicators from DAW

**Steps**:
- [ ] Detect user input in comment field
- [ ] Debounce and send typing messages
- [ ] Send stopped_typing on blur
- [ ] Test: Type in plugin comment, verify others see indicator

---

## 5. Plugin Presence via getstream.io API

### 5.1 Research Stream Presence API
**Task**: Understand how to use getstream.io for presence

**Steps**:
- [ ] Check Stream documentation: https://getstream.io/get-started/
- [ ] Look for: User status, presence management, activity custom fields
- [ ] Research: Can we set custom user metadata like `in_studio`, `daw_type`, `last_active`?
- [ ] Determine: Is this done via activity feeds or separate API?
- [ ] **Decision point**: Use Stream's `user` metadata or `activity` verb for presence?

**Investigation Notes**:
- [ ] Check if Stream tracks online status natively
- [ ] Check if we can add custom fields to user object
- [ ] Example: `user.data = {online: true, in_studio: true, daw: "ableton"}`
- [ ] Or: Create activity with verb `in_studio` and verb `left_studio`

### 5.2 Add Presence Update Endpoint
**File**: `backend/internal/handlers/presence.go` (or create it)
**Task**: Create API endpoint to update presence via Stream

**Steps**:
- [ ] Create endpoint: `PUT /api/v1/me/presence`
- [ ] Payload: `{status: "online"|"offline", in_studio: bool, daw_type: string, last_activity: timestamp}`
- [ ] Update in getstream.io via Stream API
- [ ] Example: `client.user(user_id).update({online: true, in_studio: true, daw: "ableton"})`
- [ ] Return updated presence

### 5.3 Plugin: Detect DAW and Send Presence
**File**: `plugin/src/core/PluginEditor.cpp` or new `Presence.cpp`
**Task**: Detect when DAW opens and report presence to backend

**Steps**:
- [ ] On plugin initialization: Detect DAW name (Ableton, Logic, Cubase, etc.)
  - [ ] Use JUCE: `juce::SystemStats::getOperatingSystemName()` (OS)
  - [ ] Detect DAW by looking at parent process (platform-specific)
  - [ ] Or: Accept as parameter from DAW at plugin load
- [ ] Call: `PUT /me/presence {in_studio: true, daw_type: "ableton"}`
- [ ] On plugin unload: `PUT /me/presence {in_studio: false}`
- [ ] On periodic check (every 30s): Update `last_activity: now`
- [ ] Test: Load plugin in DAW, verify backend has presence data

**Sub-tasks**:
- [ ] Research DAW detection mechanism for Windows/Mac/Linux
- [ ] Implement cross-platform DAW name detection
- [ ] Add presence interval timer
- [ ] Handle network errors gracefully

### 5.4 Web: Fetch and Display Presence
**File**: `web/src/hooks/usePresence.ts`
**Task**: Fetch presence data from getstream.io and display it

**Steps**:
- [ ] Create hook: `usePresence(userId)` - fetch user presence from Stream
- [ ] Example: Query Stream `user(userId).get()` to get custom metadata
- [ ] Return: `{online: bool, in_studio: bool, daw_type: string, last_active: timestamp}`
- [ ] Render on user profile and in messages
- [ ] Test: Load plugin, check web - should show "in_studio: true"

---

## 6. UI Indicators for Presence in Messaging & Account Views

### 6.1 Messaging: Show Who's Online
**File**: `web/src/components/Messages/ConversationList.tsx`
**Task**: Show online status in message thread list

**Steps**:
- [ ] Fetch presence for each conversation participant
- [ ] Display green dot or "online" badge next to name
- [ ] Show last_active timestamp: "Active 2 hours ago"
- [ ] Update in real-time when presence changes (via WebSocket)
- [ ] Test: Message someone, see online/offline status

### 6.2 Messaging: Show In-Studio Status
**File**: `web/src/components/Messages/ChatHeader.tsx`
**Task**: Show when recipient is in DAW

**Steps**:
- [ ] In message header, show: "🎚️ In Ableton Live" if in_studio
- [ ] Show DAW icon based on `daw_type` field
- [ ] Example icons: 🎚️ Ableton, 🎹 Logic, 🎼 Cubase
- [ ] Show "Away from DAW" if online but not in_studio
- [ ] Test: Load plugin, open message - should show DAW status

### 6.3 Account View: Show Presence
**File**: `web/src/pages/UserProfile.tsx`
**Task**: Show presence on user profile page

**Steps**:
- [ ] Under user bio/header, add presence indicator
- [ ] If mutual follows and user is in_studio, show: "🎚️ Currently in Ableton Live"
- [ ] If online but not in_studio: "🟢 Online"
- [ ] If offline: "⚪ Last active 2 hours ago"
- [ ] Update in real-time
- [ ] Test: Follow someone, open their profile - see presence

### 6.4 Plugin: Show Presence of Followed Users
**File**: `plugin/src/ui/social/UserProfile.cpp`
**Task**: Show who's online and in DAW (in plugin)

**Steps**:
- [ ] Create presence panel in user profile
- [ ] Query Stream for user presence data
- [ ] Display: "Online", "In Logic Pro", "Offline - Last active 30m ago"
- [ ] Update every 30 seconds or on WebSocket presence change
- [ ] Test: Load plugin, view profile of online user

### 6.5 Activity Feed: Show Presence Context
**File**: `web/src/components/Activity/ActivityItem.tsx`
**Task**: Show if actor is currently online

**Steps**:
- [ ] When displaying activity (X posted, X liked), check if X is online
- [ ] Show green dot next to username if online
- [ ] Show DAW icon if in_studio
- [ ] Example: "🟢 john (in Ableton Live) liked your post"
- [ ] Test: Multiple activities, some actors online, some offline

---

## 7. Activity Timeline Real-time Updates (Already Partially Covered)

### 7.1 Backend: Fetch Activities from Stream
**File**: `backend/internal/handlers/activity.go`
**Task**: Ensure activities are properly queried from getstream.io

**Steps**:
- [ ] Verify endpoint exists: `GET /api/v1/activity/timeline`
- [ ] Fetch from Stream: `timeline_feed.get(limit=20, offset=0)`
- [ ] Include full user data (avatar, username, bio)
- [ ] Include engagement data (like_count, comment_count)
- [ ] Test: Verify activities in correct order

### 7.2 Backend: Broadcast Activities via WebSocket
**File**: `backend/internal/websocket/handler.go`
**Task**: When activity created in Stream, push to followers

**Steps**:
- [ ] Hook into activity creation (posts, likes, follows)
- [ ] Call `h.wsHandler.BroadcastActivity(activity)`
- [ ] Send to: followers of actor
- [ ] Include: actor info, object, timestamp
- [ ] Test: Create activity, verify broadcast

### 7.3 Web: Real-time Activity Feed
**File**: `web/src/hooks/useActivityFeed.ts`
**Task**: Combine activity polling with real-time WebSocket updates

**Steps**:
- [ ] Initial load: Fetch from endpoint `GET /activity/timeline`
- [ ] Subscribe to WebSocket: Listen for activity updates
- [ ] On new activity: Prepend to feed
- [ ] Handle duplicate: Check if activity already in feed (by ID)
- [ ] Test: Post something, verify it appears instantly in followers' feeds

### 7.4 Plugin: Activity Feed Updates
**File**: `plugin/src/ui/feed/ActivityFeed.cpp`
**Task**: Show real-time activity in plugin

**Steps**:
- [ ] Listen to WebSocket activity updates
- [ ] Update activity store
- [ ] Refresh UI
- [ ] Test: Activity appears in plugin when someone you follow acts

---

## 8. Testing & Validation

### 8.1 Manual Testing Checklist
- [ ] Test like notification: Like post, verify owner gets WebSocket event
- [ ] Test comment notification: Comment on post, verify owner notified
- [ ] Test engagement updates: Like count updates in real-time across clients
- [ ] Test typing indicators: Type comment, others see "X is typing..."
- [ ] Test presence: Load plugin, verify presence appears on web
- [ ] Test activity timeline: Post something, verify followers see it instantly
- [ ] Test multi-client: Open 2 browser tabs, perform actions, verify both update
- [ ] Test plugin-web sync: Like from web, verify plugin count updates
- [ ] Test presence in messaging: Message someone in DAW, see status
- [ ] Test profile presence: Visit profile of online user, see DAW status

### 8.2 Integration Testing
**File**: `backend/tests/websocket_integration_test.go`
**Task**: Test multiple clients and real-time scenarios

**Tests to Write**:
- [ ] Test: Multiple clients receive like notification
- [ ] Test: Activity broadcast to followers only
- [ ] Test: Typing indicator timeout (stops after 5s inactivity)
- [ ] Test: Presence update persists in Stream
- [ ] Test: Engagement counts eventually consistent
- [ ] Test: Client reconnection resynchronizes state

### 8.3 Performance Testing
**Task**: Load test WebSocket with multiple concurrent clients

**Tests**:
- [ ] [ ] Simulate 1000 concurrent connections
- [ ] [ ] Send 100 likes/minute across 10 posts
- [ ] [ ] Measure message latency (sender to receiver)
- [ ] [ ] Check CPU/memory under load
- [ ] [ ] Verify no message loss

---

## 9. Documentation

### 9.1 Update WebSocket Docs
**File**: `docs/WEBSOCKET_REALTIME.md` (create new)
**Task**: Document all real-time features

**Content**:
- [ ] Message type reference (all types, payloads)
- [ ] Client subscription patterns
- [ ] Error handling
- [ ] Performance considerations
- [ ] Troubleshooting

### 9.2 Update Architecture Docs
**File**: `docs/ARCHITECTURE.md`
**Task**: Add real-time feature architecture section

**Content**:
- [ ] Real-time flow diagrams
- [ ] Integration with getstream.io
- [ ] Notification patterns
- [ ] Scaling considerations

---

## Priority & Sequencing

**Phase 1 (Week 1)**: Core Engagement
1. Like/comment notifications (1.1, 1.2, 1.3, 1.4) - IMPLEMENT FIRST
2. Engagement metrics (3.1, 3.2, 3.3, 3.4)
3. Web testing (8.1 partial)

**Phase 2 (Week 2)**: Activity & Presence
4. Activity feed real-time (2.1-2.6)
5. Plugin presence via Stream (5.1-5.4)
6. Presence UI (6.1-6.5)

**Phase 3 (Week 3)**: Polish
7. Typing indicators (4.1-4.5)
8. Testing & validation (8.2-8.3)
9. Documentation (9.1-9.2)

---

## Implementation Notes

### Key Files to Modify
- `backend/internal/handlers/feed.go` - Wire like/comment notifications
- `backend/internal/handlers/activity.go` - Wire activity broadcasts
- `backend/internal/websocket/handler.go` - Add handlers for new message types
- `backend/internal/websocket/message.go` - Define new message types
- `web/src/hooks/useWebSocket.ts` - Add listeners
- `web/src/components/feed/Post.tsx` - Update UI for engagement
- `plugin/src/network/NetworkClient.cpp` - Wire WebSocket listeners
- `plugin/src/ui/` - Add UI components for presence/typing

### Dependencies to Check
- [ ] getstream.io Go SDK version (supports presence?)
- [ ] WebSocket library supports message size limits
- [ ] React Query or SWR for invalidation on engagement changes

### Risk Areas
- [ ] Presence consistency across clients
- [ ] Race conditions in engagement count updates
- [ ] WebSocket message loss during reconnects
- [ ] Typing indicator false positives

---

## Estimation

| Task | Est. Hours | Priority |
|------|-----------|----------|
| Like/comment notifications | 3 | P0 |
| Engagement metrics | 2 | P0 |
| Activity feed real-time | 4 | P0 |
| Plugin presence (Stream API) | 4 | P1 |
| Presence UI indicators | 5 | P1 |
| Typing indicators | 3 | P2 |
| Testing & docs | 4 | P2 |
| **Total** | **25 hours** | |

---

