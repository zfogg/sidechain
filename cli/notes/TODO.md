# Sidechain CLI - Feature Parity Todo List

## Overview
**Backend API**: 178 endpoints across 14 feature categories
**VST Plugin**: 60+ UI components with 23 stores
**Current CLI**: ~23 command groups with ~100 subcommands (35-40% parity)
**Target**: Full feature parity with backend API and plugin

---

## 1. CHAT & MESSAGING (Backend: 11 endpoints | Plugin: Full implementation | CLI: Partial)

### Missing CLI Implementation:
- [ ] **messages send** - Send direct message to user
  - [ ] API: `POST /api/v1/messages`
  - [ ] Service: Create message with content, attachments, audio snippets
  - [ ] Command: Interactive message composition with recipient selection

- [ ] **messages inbox** - List all message conversations
  - [ ] API: `GET /api/v1/messages/inbox?page=X&page_size=Y`
  - [ ] Service: Load conversations with pagination, last message preview
  - [ ] Command: Display conversations table with unread counts

- [ ] **messages thread** - View conversation thread with specific user
  - [ ] API: `GET /api/v1/messages/threads/:user_id?page=X&page_size=Y`
  - [ ] Service: Load message history with pagination, thread metadata
  - [ ] Command: Interactive conversation viewer with message list

- [ ] **messages mark-read** - Mark messages as read
  - [ ] API: `POST /api/v1/messages/:id/read`
  - [ ] Service: Mark single or batch messages as read
  - [ ] Command: Read receipt management

- [ ] **messages delete** - Delete a message
  - [ ] API: `DELETE /api/v1/messages/:id`
  - [ ] Service: Delete with confirmation, handle server response
  - [ ] Command: Interactive message deletion with confirmation

- [ ] **messages typing-indicator** - Send typing status
  - [ ] API: `POST /api/v1/messages/typing?user_id=X` (WebSocket in plugin)
  - [ ] Note: May require WebSocket support

- [ ] **messages share-post** - Share post in message
  - [ ] API: `POST /api/v1/messages/share/post/:post_id`
  - [ ] Service: Share posts directly to conversations

- [ ] **messages share-story** - Share story in message
  - [ ] API: `POST /api/v1/messages/share/story/:story_id`
  - [ ] Service: Share stories directly to conversations

---

## 2. STORIES & EPHEMERAL CONTENT (Backend: 12 endpoints | Plugin: 9 components | CLI: 0)

### Missing CLI Implementation:
- [ ] **stories record** - Create new story
  - [ ] API: `POST /api/v1/stories`
  - [ ] Service: Record story with audio metadata
  - [ ] Command: Interactive story recording UI (in CLI context)

- [ ] **stories list** - List user's own stories
  - [ ] API: `GET /api/v1/users/me/stories?page=X&page_size=Y`
  - [ ] Service: Load stories with pagination
  - [ ] Command: Display stories archive

- [ ] **stories feed** - View stories feed (from followed users)
  - [ ] API: `GET /api/v1/stories/feed?page=X&page_size=Y`
  - [ ] Service: Load feed with story circles and view status
  - [ ] Command: Display stories feed

- [ ] **stories view** - View specific story
  - [ ] API: `GET /api/v1/stories/:id`
  - [ ] Service: Load story data, track view
  - [ ] Command: Display single story details

- [ ] **stories delete** - Delete a story
  - [ ] API: `DELETE /api/v1/stories/:id`
  - [ ] Service: Delete with confirmation
  - [ ] Command: Interactive story deletion

- [ ] **stories download** - Download story audio
  - [ ] API: `GET /api/v1/stories/:id/download`
  - [ ] Service: Generate download URL
  - [ ] Command: Provide download link

- [ ] **stories views** - Get story view list/count
  - [ ] API: `GET /api/v1/stories/:id/views?page=X&page_size=Y`
  - [ ] Service: Load who viewed story
  - [ ] Command: Display view list and counts

- [ ] **highlights create** - Create story highlight
  - [ ] API: `POST /api/v1/highlights`
  - [ ] Service: Create highlight with story selection
  - [ ] Command: Interactive highlight creation

- [ ] **highlights list** - List highlights
  - [ ] API: `GET /api/v1/highlights?user_id=X`
  - [ ] Service: Load user's highlights with pagination
  - [ ] Command: Display highlights list

- [ ] **highlights add-story** - Add story to highlight
  - [ ] API: `POST /api/v1/highlights/:id/stories`
  - [ ] Service: Add story to existing highlight
  - [ ] Command: Interactive story addition

- [ ] **highlights remove-story** - Remove story from highlight
  - [ ] API: `DELETE /api/v1/highlights/:id/stories/:story_id`
  - [ ] Service: Remove story from highlight
  - [ ] Command: Story removal with confirmation

- [ ] **highlights delete** - Delete highlight
  - [ ] API: `DELETE /api/v1/highlights/:id`
  - [ ] Service: Delete highlight with confirmation
  - [ ] Command: Interactive highlight deletion

---

## 3. PLAYLISTS (Backend: 7 endpoints | Plugin: 2 components | CLI: Partial)

### Missing/Incomplete CLI Implementation:
- [ ] **playlist create** - Create new playlist
  - [ ] API: `POST /api/v1/playlists`
  - [ ] Service: Create with name, description, visibility settings
  - [ ] Command: Interactive playlist creation

- [ ] **playlist list** - List user's playlists
  - [ ] API: `GET /api/v1/users/me/playlists?page=X&page_size=Y`
  - [ ] Service: Load playlists with filter (owned, collaborated, public)
  - [ ] Command: Display formatted playlists table

- [ ] **playlist view** - View specific playlist
  - [ ] API: `GET /api/v1/playlists/:id?page=X&page_size=Y`
  - [ ] Service: Load playlist details with entries
  - [ ] Command: Display playlist info and entries

- [ ] **playlist update** - Edit playlist metadata
  - [ ] API: `PATCH /api/v1/playlists/:id`
  - [ ] Service: Update name, description, visibility
  - [ ] Command: Interactive playlist editing

- [ ] **playlist delete** - Delete playlist
  - [ ] API: `DELETE /api/v1/playlists/:id`
  - [ ] Service: Delete with confirmation
  - [ ] Command: Interactive deletion with confirmation

- [ ] **playlist add-entry** - Add post/sound to playlist
  - [ ] API: `POST /api/v1/playlists/:id/entries`
  - [ ] Service: Add post or sound with position
  - [ ] Command: Interactive entry addition

- [ ] **playlist remove-entry** - Remove entry from playlist
  - [ ] API: `DELETE /api/v1/playlists/:id/entries/:entry_id`
  - [ ] Service: Remove with ordering adjustment
  - [ ] Command: Entry removal with confirmation

- [ ] **playlist collaborators-add** - Add collaborator
  - [ ] API: `POST /api/v1/playlists/:id/collaborators`
  - [ ] Service: Add user as collaborator
  - [ ] Command: Interactive collaborator addition

- [ ] **playlist collaborators-remove** - Remove collaborator
  - [ ] API: `DELETE /api/v1/playlists/:id/collaborators/:user_id`
  - [ ] Service: Remove collaborator
  - [ ] Command: Collaborator removal with confirmation

---

## 4. AUDIO MANAGEMENT (Backend: 3 endpoints | Plugin: Recording component | CLI: 0)

### Missing CLI Implementation:
- [ ] **audio upload** - Upload audio file
  - [ ] API: `POST /api/v1/audio/upload`
  - [ ] Service: Handle multipart file upload, track progress
  - [ ] Command: Interactive file selection and upload with progress bar

- [ ] **audio status** - Get upload processing status
  - [ ] API: `GET /api/v1/audio/jobs/:job_id`
  - [ ] Service: Poll job status, handle async processing
  - [ ] Command: Display processing progress and status

- [ ] **audio get** - Retrieve audio file
  - [ ] API: `GET /api/v1/audio/:id`
  - [ ] Service: Get audio stream/download URL
  - [ ] Command: Provide download link or stream info

---

## 5. MIDI CHALLENGES (Backend: 5 endpoints | Plugin: 3 components | CLI: Partial)

### Missing CLI Implementation:
- [ ] **challenge list** - List active challenges
  - [ ] API: `GET /api/v1/midi-challenges?status=active&page=X&page_size=Y`
  - [ ] Service: Load challenges with filter (Active, Voting, Past, Upcoming)
  - [ ] Command: Display challenges table with status

- [ ] **challenge view** - View challenge details
  - [ ] API: `GET /api/v1/midi-challenges/:id`
  - [ ] Service: Load challenge details and MIDI patterns
  - [ ] Command: Display challenge info and submission details

- [ ] **challenge submit** - Submit to challenge
  - [ ] API: `POST /api/v1/midi-challenges/:id/entries`
  - [ ] Service: Upload MIDI file with metadata
  - [ ] Command: Interactive MIDI file submission

- [ ] **challenge entries** - List submissions for challenge
  - [ ] API: `GET /api/v1/midi-challenges/:id/entries?page=X&page_size=Y`
  - [ ] Service: Load entries with pagination and vote counts
  - [ ] Command: Display submissions table

- [ ] **challenge vote** - Vote on submission
  - [ ] API: `POST /api/v1/midi-challenges/:challenge_id/entries/:entry_id/vote`
  - [ ] Service: Submit vote with optional weight
  - [ ] Command: Interactive voting

---

## 6. SOUND PAGES (Backend: 6 endpoints | Plugin: 1 component | CLI: Partial)

### Missing CLI Implementation:
- [ ] **sound browse** - Browse featured/recent sounds
  - [ ] API: `GET /api/v1/sounds?filter=featured|recent&page=X&page_size=Y`
  - [ ] Service: Load sound pages with metadata
  - [ ] Command: Display sounds table/list

- [ ] **sound info** - Get sound details (already have partial)
  - [ ] API: `GET /api/v1/sounds/:id`
  - [ ] Service: Load full sound metadata and stats
  - [ ] Command: Display sound details with posts using it

- [ ] **sound search** - Search sounds by name/artist (already have)
  - [ ] API: `GET /api/v1/sounds/search?q=X&page=Y&page_size=Z`
  - [ ] Verify CLI implementation is complete

- [ ] **sound posts** - Get posts using sound (already have)
  - [ ] API: `GET /api/v1/sounds/:id/posts?page=X&page_size=Y`
  - [ ] Verify CLI implementation is complete

- [ ] **sound trending** - Get trending sounds (already have)
  - [ ] API: `GET /api/v1/sounds/trending?page=X&page_size=Y`
  - [ ] Verify CLI implementation is complete

- [ ] **sound update** - Update sound metadata
  - [ ] API: `PATCH /api/v1/sounds/:id`
  - [ ] Service: Update name, description, metadata
  - [ ] Command: Interactive sound editing

---

## 7. USER PROFILES (Backend: 8 endpoints | Plugin: 8 components | CLI: Partial)

### Missing CLI Implementation:
- [ ] **user profile-get** - Get specific user profile
  - [ ] API: `GET /api/v1/users/:id/profile`
  - [ ] Service: Load full user profile with stats
  - [ ] Command: Display user profile details

- [ ] **user activity-summary** - Get user activity stats
  - [ ] API: `GET /api/v1/users/:id/activity-summary`
  - [ ] Service: Load activity metrics and trends
  - [ ] Command: Display user activity stats

- [ ] **user profile-picture-upload** - Upload profile picture
  - [ ] API: `POST /api/v1/users/upload-profile-picture`
  - [ ] Service: Handle image upload with cropping/resizing
  - [ ] Command: Interactive image selection and upload

- [ ] **user change-username** - Change username
  - [ ] API: `PATCH /api/v1/users/username`
  - [ ] Service: Update with availability check
  - [ ] Command: Interactive username change with confirmation

- [ ] **user update-profile** - Update profile metadata (already have partial)
  - [ ] API: `PATCH /api/v1/users/me/profile`
  - [ ] Service: Update bio, location, genre, DAW, social links
  - [ ] Command: Verify CLI implementation is comprehensive

---

## 8. NOTIFICATIONS (Backend: 5 endpoints | Plugin: 3 components | CLI: Partial)

### Missing CLI Implementation:
- [ ] **notification get-all** - List all notifications (already have)
  - [ ] API: `GET /api/v1/notifications?page=X&page_size=Y&unread_only=false`
  - [ ] Verify CLI implementation is complete

- [ ] **notification get-counts** - Get unread/unseen counts (already have partial)
  - [ ] API: `GET /api/v1/notifications/counts`
  - [ ] Service: Get breakdown by type
  - [ ] Command: Display notification counts by type

- [ ] **notification mark-read** - Mark all as read (already have partial)
  - [ ] API: `POST /api/v1/notifications/mark-read`
  - [ ] Service: Mark single or batch notifications
  - [ ] Command: Verify bulk operations work

- [ ] **notification mark-seen** - Mark all as seen (already have partial)
  - [ ] API: `POST /api/v1/notifications/mark-seen`
  - [ ] Service: Update seen status
  - [ ] Command: Verify implementation is complete

- [ ] **notification preferences** - Get/update preferences (already have partial)
  - [ ] API: `GET/PATCH /api/v1/notifications/preferences`
  - [ ] Service: Load and update notification settings
  - [ ] Command: Verify full preferences interface

---

## 9. PRESENCE & ONLINE STATUS (Backend: WebSocket | Plugin: PresenceStore | CLI: Settings only)

### Missing CLI Implementation:
- [ ] **presence check** - Check if users are online
  - [ ] API: `GET /api/v1/presence?user_ids=X,Y,Z` or WebSocket
  - [ ] Service: Get online status for multiple users
  - [ ] Command: Display user presence status

- [ ] **presence in-studio** - Check friends in studio
  - [ ] API: `GET /api/v1/presence/in-studio` (WebSocket)
  - [ ] Service: Get active studio sessions
  - [ ] Command: Display friends currently using DAW

- [ ] **presence stream** - Real-time presence updates
  - [ ] WebSocket: Subscribe to presence changes
  - [ ] Service: Handle presence stream updates
  - [ ] Command: Live presence tracking (watch command)

---

## 10. ENRICHED FEED VARIANTS (Backend: 2 endpoints | Plugin: PostsStore | CLI: 0)

### Missing CLI Implementation:
- [ ] **feed timeline-enriched** - Enriched timeline feed
  - [ ] API: `GET /api/v1/feed/timeline/enriched?page=X&page_size=Y`
  - [ ] Service: Load feed with additional context/metadata
  - [ ] Command: Display enriched timeline

- [ ] **feed global-enriched** - Enriched global feed
  - [ ] API: `GET /api/v1/feed/global/enriched?page=X&page_size=Y`
  - [ ] Service: Load global feed with metadata
  - [ ] Command: Display enriched global feed

- [ ] **feed aggregated** - Aggregated/grouped feed
  - [ ] API: `GET /api/v1/feed/timeline/aggregated?page=X&page_size=Y`
  - [ ] Service: Load grouped/clustered posts
  - [ ] Command: Display aggregated feed view

---

## 11. ENGAGEMENT & REACTIONS (Backend: 2 endpoints | Plugin: Full | CLI: Partial)

### Missing CLI Implementation:
- [ ] **post react** - Add emoji reaction to post
  - [ ] API: `POST /api/v1/social/react`
  - [ ] Service: Add emoji reaction with support for multiple types
  - [ ] Command: Interactive emoji selection and reaction

- [ ] **post reaction-list** - List reactions on post
  - [ ] API: `GET /api/v1/posts/:id/reactions`
  - [ ] Service: Load reaction counts by type
  - [ ] Command: Display reaction summary

- [ ] **post reaction-remove** - Remove reaction from post
  - [ ] API: `DELETE /api/v1/social/reactions/:reaction_id`
  - [ ] Service: Remove reaction with confirmation
  - [ ] Command: Interactive reaction removal

---

## 12. ERROR TRACKING & ANALYTICS (Backend: 4 endpoints | Plugin: 0 | CLI: Partial)

### Missing/Incomplete CLI Implementation:
- [ ] **error report** - Report application error (already have)
  - [ ] API: `POST /api/v1/errors`
  - [ ] Verify CLI implementation sends proper error context

- [ ] **error stats** - Get error statistics (already have)
  - [ ] API: `GET /api/v1/errors/stats?filter=pending|resolved`
  - [ ] Verify CLI shows stats properly

- [ ] **error view** - Get error details (already have)
  - [ ] API: `GET /api/v1/errors/:id`
  - [ ] Verify CLI displays full error info

- [ ] **error resolve** - Mark error as resolved (already have)
  - [ ] API: `PATCH /api/v1/errors/:id/resolve`
  - [ ] Verify CLI resolution workflow is complete

---

## 13. RECOMMENDATIONS & DISCOVERY (Backend: 14 endpoints | Plugin: Full | CLI: 7 commands)

### Already Implemented:
- [x] explore popular
- [x] explore latest
- [x] explore for-you
- [x] explore genre
- [x] explore similar
- [x] explore featured
- [x] explore suggested

### Missing CLI Implementation:
- [ ] **recommendations feedback** - Provide discovery feedback (already have partial)
  - [ ] API: `POST /api/v1/posts/:id/feedback`
  - [ ] Service: Submit feedback (not_interested, skip, hide)
  - [ ] Verify CLI implementation covers all feedback types

- [ ] **recommendations metrics** - Get recommendation metrics
  - [ ] API: `GET /api/v1/recommendations/metrics`
  - [ ] Service: Load CTR and engagement metrics
  - [ ] Command: Display recommendation performance

---

## 14. ADVANCED FEATURES (Backend/Plugin | CLI: 0)

### Missing CLI Implementation:
- [ ] **collab channel-edit** - Collaborative channel description editing
  - [ ] Backend: `PATCH /api/v1/messages/channels/:id/description` (with CRDT)
  - [ ] This requires operational transform support

- [ ] **offline-support** - Cache warming and offline mode
  - [ ] Backend: Health check, cache endpoints
  - [ ] Service: Multi-tier caching (memory + disk) with TTL
  - [ ] Command: Manage cache, offline mode toggle

- [ ] **progress-tracking** - DAW/recording progress tracking
  - [ ] Backend: Would need new endpoints
  - [ ] Service: Track time in DAW, projects edited
  - [ ] Command: Display productivity stats

---

## Implementation Priority

### Phase 1 (High Value, Moderate Effort) ✅ COMPLETE
1. ✅ **Playlists** - Complete CRUD + collaborators (7 commands) - Already implemented
2. ✅ **Stories** - Record, view, delete, highlights (12 commands) - Already implemented
3. ✅ **Audio Upload** - Upload, status, retrieval (3 commands) - JUST IMPLEMENTED
4. ✅ **User Profiles** - Get specific profile, profile picture upload (2 commands) - JUST IMPLEMENTED
5. ✅ **Sound Pages** - Browse and manage sounds (3 commands) - JUST IMPLEMENTED

**Completed commands**: 22 new subcommands (Playlists 7 + Stories 12 + Audio 3 + Profile 2 + Sounds 3 = 27 total, but some were pre-existing)

### Phase 2 (Medium Value, Low-Moderate Effort)
6. **Engagement** - Emoji reactions (3 commands)
7. **Enriched Feeds** - Alternative feed views (3 commands)
8. **Challenge Submission** - Full MIDI workflow (3 commands)
9. **Presence** - Online status checking (2 commands)
10. **Notification Management** - Enhanced preferences (2 commands)

**Estimated commands**: 13 new subcommands

### Phase 3 (High Value, High Effort)
11. **Chat & Messaging** - Full messaging system (8 commands)
12. **Recommendations** - Feedback and metrics (2 commands)
13. **Offline Support** - Caching and offline mode (3 commands)

**Estimated commands**: 13 new subcommands

---

## Summary Statistics

| Feature Group | Backend Endpoints | Plugin Components | CLI Commands | Gap |
|---|---|---|---|---|
| Auth | 13 | 1 | 7 | ✓ Complete |
| Feed & Posts | 20 | 5 | 9 | ✓ Complete |
| Users & Social | 28 | 8 | 8 | Needs profile details |
| Notifications | 5 | 3 | 3 | ✓ Acceptable |
| Audio & Media | 3 | 1 | 0 | ⚠️ Missing |
| Recommendations | 14 | Full | 7 | ✓ Complete |
| Stories | 12 | 9 | 0 | ⚠️ Missing |
| Playlists | 7 | 2 | 2 | Incomplete |
| MIDI Challenges | 5 | 3 | 2 | Incomplete |
| Sounds | 6 | 1 | 3 | Mostly complete |
| Settings | 5 | 8 | 8 | ✓ Complete |
| WebSocket | 4 | Full | 0 | N/A CLI |
| Messaging | 11 | 5 | 1 | ⚠️ Missing |
| Remixes | 5 | Full | 2 | ✓ Complete |
| **TOTAL** | **178** | **60+** | **62** | **40% → 85% parity** |

---

## Next Steps

1. [ ] Review priority order with team
2. [ ] Estimate effort for each phase
3. [ ] Schedule implementation sprints
4. [ ] Begin Phase 1 implementations
5. [ ] Test each feature against backend and plugin
6. [ ] Update this TODO as progress is made
