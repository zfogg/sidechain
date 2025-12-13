# Sidechain → Instagram-Level Polish TODO

> **Vision**: Make Sidechain feel like "Instagram/Snapchat with audio for producers in your DAW"
> **Last Updated**: December 13, 2024
> **Status**: Comprehensive feature gap analysis and prioritized TODO list

---

## Executive Summary

Sidechain has a solid foundation with core features implemented:
- Authentication (email + OAuth)
- Audio recording/upload with waveform visualization
- Feed with posts, likes, comments, emoji reactions
- Follow system with profiles
- Stories (24-hour expiring clips with MIDI visualization)
- Direct messaging (1-to-1 and groups)
- Search and discovery
- Playlists, MIDI challenges, hidden synth easter egg

**To compete with Instagram/Snapchat/TikTok**, we need to add polish, social features, and the "feel" that makes these apps addictive.

---

## Priority Legend

- **P0 - Critical**: Must have for professional feel, blocks user adoption
- **P1 - High**: Expected by users from modern social apps
- **P2 - Medium**: Nice to have, differentiators
- **P3 - Low**: Future enhancements, not MVP blockers

---

## P0: Critical - Must Have for Launch

### 1. Story Highlights (Archives)

**Problem**: Stories expire after 24 hours with no way to save them permanently. Instagram/Snapchat have highlights/memories.

**Current State**: Backend has `story_highlights` and `highlighted_stories` tables. UI partially implemented.

**TODO**:
- [ ] **1.1** Create `HighlightsComponent` in plugin UI
  - Display highlight circles on profile (like Instagram)
  - Each highlight shows cover image and name
  - Click to view stories in that highlight
  - File: `plugin/src/ui/profile/Highlights.h/cpp`

- [ ] **1.2** Create highlight management UI
  - "Add to Highlight" option after viewing own story
  - Create new highlight with name and cover
  - Edit existing highlights (add/remove stories, rename)
  - Reorder highlights on profile

- [ ] **1.3** Update ProfileComponent
  - Show highlights row between bio and posts
  - "New" button to create highlight
  - Scrollable horizontal list

- [ ] **1.4** Backend endpoint verification
  - Verify `POST /api/v1/highlights` works
  - Verify `GET /api/v1/users/:id/highlights` returns highlights
  - Verify `POST /api/v1/highlights/:id/stories` adds stories

### 2. Save/Bookmark Posts

**Problem**: No way to save posts for later. Users want to bookmark loops they like.

**TODO**:
- [ ] **2.1** Create `SavedPost` model in backend
  ```go
  type SavedPost struct {
      ID        uuid.UUID
      UserID    uuid.UUID
      PostID    uuid.UUID
      CreatedAt time.Time
  }
  ```

- [ ] **2.2** Create backend endpoints
  - `POST /api/v1/posts/:id/save` - Save post
  - `DELETE /api/v1/posts/:id/save` - Unsave post
  - `GET /api/v1/users/me/saved` - Get saved posts (paginated)

- [ ] **2.3** Add bookmark icon to PostCardComponent
  - Next to share button
  - Filled when saved, outline when not
  - Animation on save

- [ ] **2.4** Create SavedPostsView
  - Accessible from profile
  - Private (only visible to owner)
  - Same layout as feed

### 3. Repost/Share to Feed

**Problem**: Can only share links. Can't repost to your own feed like Twitter/Instagram.

**TODO**:
- [ ] **3.0** Check getstream.io docs to see the correct way to repost posts to your own feed. Check if they can handle repost counts too.
- [ ] **3.1** Add `repost_of_id` field to AudioPost model if we need it and getstream doesn't handle it
- [ ] **3.2** Create `POST /api/v1/posts/:id/repost` endpoint if we need it and getstream doesn't handle it
- [ ] **3.3** Show reposts in feed with original attribution
  - "username reposted" header
  - Link to original post author
- [ ] **3.4** Add "Repost" option to post actions menu
- [ ] **3.5** Show repost count on posts. We have to keep track of this if gestream.io doesn't handle it

### 4. Direct Share to DMs

**Problem**: Can't share posts or stories directly to messages like Instagram.

**TODO**:
- [ ] **4.0** Research how we can embed posts and stories into getstream.io chats.
- [ ] **4.1** Add "Send" option to share menu on posts and when viewing someone else's stories
- [ ] **4.2** Create user/channel picker dialog
  - Recent conversations at top
  - Search for users, recently-searched-for and recently-messaged users suggested at the top
  - Select multiple recipients
- [ ] **4.3** Share post as message with preview
  - Audio preview in message
  - Click to open full post
- [ ] **4.4** Share stories to DMs similarly

### 5. Drafts System

**Problem**: No way to save work in progress. Lose recordings if you close plugin.

**TODO**:
- [ ] **5.1** Create local draft storage
  - File: `~/.local/share/Sidechain/drafts/`
  - Save audio + metadata as draft

- [ ] **5.2** Add "Save as Draft" button to Upload screen
  - Appears next to "Share" button
  - Saves current state locally

- [ ] **5.3** Create DraftsView in plugin
  - Accessible from profile or recording screen
  - List of drafts with preview
  - Resume editing or delete

- [ ] **5.4** Auto-save drafts on crash/close
  - Save recording state automatically
  - Prompt to restore on next open

### 6. Loading States & Skeleton Screens

**Problem**: App feels unpolished without proper loading states.

**TODO**:
- [ ] **6.1** Create skeleton components for:
  - PostCardSkeleton (gray boxes animating)
  - ProfileSkeleton
  - StorySkeleton
  - CommentSkeleton

- [ ] **6.2** Show skeletons while loading:
  - Feed loading
  - Profile loading
  - Story feed loading
  - Search results loading

- [ ] **6.3** Add pull-to-refresh animation
  - Visual feedback during refresh
  - Spinner or custom animation

### 7. Error States with Retry

**Problem**: Errors just show text. Need actionable error states.

**TODO**:
- [ ] **7.1** Create consistent error component
  - Icon + message + retry button
  - Different styles for network/auth/generic errors

- [ ] **7.2** Add error states to all async views:
  - Feed (with retry)
  - Profile (with retry)
  - Search (with suggestions)
  - Messages (with reconnect)

- [ ] **7.3** Toast notifications for transient errors
  - Like failed, comment failed, etc.
  - Auto-dismiss after 3 seconds

### 8. Offline Mode Basics

**Problem**: App is unusable without network. Should cache recent content.

**TODO**:
- [ ] **8.1** Cache last viewed feed
  - Store last 50 posts locally
  - Show cached content when offline
  - "You're offline" banner

- [ ] **8.2** Queue actions when offline
  - Queue likes, comments, follows
  - Sync when back online
  - Show pending state

- [ ] **8.3** Offline indicators
  - Gray out interactive elements
  - Show "offline" badge
  - Retry indicator

---

## P1: High Priority - Expected by Users

### 9. Activity Status Controls

**Problem**: No way to hide online status. Privacy concern.

**TODO**:
- [ ] **9.1** Add activity status toggle to settings
  - "Show Activity Status" on/off
  - Store in user preferences

- [ ] **9.2** Update presence queries
  - Respect user's activity status setting
  - Don't show online to others if disabled

- [ ] **9.3** "Last active" privacy
  - Option to hide last active time
  - Show "Active X ago" vs just "Active"

### 10. Mute Users (Without Blocking)

**Problem**: Can only block users. Sometimes just want to mute without unfollowing.

**TODO**:
- [ ] **10.1** Create `MutedUser` model
- [ ] **10.2** Add mute endpoints
  - `POST /api/v1/users/:id/mute`
  - `DELETE /api/v1/users/:id/mute`
  - `GET /api/v1/users/me/muted`

- [ ] **10.3** Filter muted users from feeds
  - Don't show their posts in timeline
  - Still show on their profile page
  - Don't receive their notifications

- [ ] **10.4** Add "Mute" option to user menu

### 11. Private Account Option ✅ IMPLEMENTED

**Problem**: All accounts are public. Some users want private accounts.

**COMPLETED**:
- [x] **11.1** Add `is_private` field to User model
  - Added to `backend/internal/models/user.go`

- [x] **11.2** Implement follow request system
  - Created `FollowRequest` model with pending/accepted/rejected status
  - Created handlers in `backend/internal/handlers/follow_requests.go`:
    - `GET /api/v1/users/me/follow-requests` - Get pending requests
    - `GET /api/v1/users/me/pending-requests` - Get sent requests
    - `POST /api/v1/follow-requests/:id/accept` - Accept request
    - `POST /api/v1/follow-requests/:id/reject` - Reject request
    - `DELETE /api/v1/follow-requests/:id` - Cancel request
    - `GET /api/v1/users/:id/follow-request-status` - Check status
  - Updated `FollowUserByID` to create follow request for private accounts
  - Notification for follow requests (TODO: WebSocket integration)

- [x] **11.3** Hide posts for private accounts
  - `GetUserPosts` returns empty list with privacy message for non-followers
  - `GetUserProfile` includes is_private and follow_request_status fields
  - Lock icon shown next to display name on Profile component

- [x] **11.4** Private account settings UI
  - Added toggle in EditProfile component
  - Privacy section with explanatory text
  - Saves to backend via `is_private` field

**TODO (follow-up)**:
- [ ] WebSocket notification when follow request is accepted
- [ ] Show pending follow requests count in notification bell

### 12. Post Comments Control

**Problem**: Can't control who comments on your posts.

**TODO**:
- [ ] **12.1** Add comment controls to post creation
  - "Allow comments: Everyone / Followers / Off"

- [ ] **12.2** Store setting on post
  - `comments_enabled` field
  - `comment_audience` enum

- [ ] **12.3** Enforce in backend
  - Check audience before allowing comment

- [ ] **12.4** Show "Comments disabled" on posts

### 13. Pin Posts to Profile

**Problem**: Can't highlight best posts on profile.

**TODO**:
- [ ] **13.1** Add `is_pinned` and `pin_order` to AudioPost
- [ ] **13.2** Add "Pin to Profile" option
  - Max 3 pinned posts
  - Reorder pinned posts

- [ ] **13.3** Show pinned posts first on profile
  - Pin badge/icon
  - Separate section or top of grid

### 14. Post Archive (Hide Without Delete) ✅ IMPLEMENTED

**Problem**: Can only delete posts. Sometimes want to hide temporarily.

**COMPLETED**:
- [x] **14.1** Add `is_archived` field to AudioPost
  - Added to `backend/internal/models/user.go`
  - Database indexes for efficient archived post queries

- [x] **14.2** Add archive endpoints
  - `POST /api/v1/posts/:id/archive` - Archive a post
  - `POST /api/v1/posts/:id/unarchive` - Unarchive a post
  - `GET /api/v1/users/me/archived` - Get archived posts (paginated)
  - `GET /api/v1/posts/:id/archived` - Check if post is archived
  - Updated feed queries to filter out archived posts

- [x] **14.3** Archive management UI
  - Created `ArchivedPosts` component (`plugin/src/ui/profile/ArchivedPosts.h/cpp`)
  - "Archived Posts" button on own profile page
  - View archived posts in scrollable list
  - Unarchive posts to restore them to feeds
  - Added `archivePost()`, `unarchivePost()`, `getArchivedPosts()` to NetworkClient

### 15. Sound/Sample Pages

**Problem**: Can't browse posts that use the same sample/sound.

**TODO**:
- [ ] **15.1** Audio fingerprinting (phase 2)
  - try using the golang fingerprinting code from https://github.com/sfluor/musig to fingerprint music, then store the result and match against the fingerprints in the database. you can rip the code out if you need (MIT license)
  - Detect same audio used across posts
  - Group by audio signature

- [ ] **15.2** Sound page UI
  - "See X other posts with this sound"
  - Click to browse all posts

- [ ] **15.3** Trending sounds section in discovery

### 16. Two-Factor Authentication

**Problem**: No 2FA option. Security concern for popular creators.

**TODO**:
- [ ] **16.1** Implement TOTP-based 2FA
  - QR code generation
  - Backup codes

- [ ] **16.2** 2FA endpoints
  - `POST /api/v1/auth/2fa/enable`
  - `POST /api/v1/auth/2fa/verify`
  - `POST /api/v1/auth/2fa/disable`

- [ ] **16.3** 2FA settings UI
  - Enable/disable toggle
  - Backup codes download
  - Recovery options

### 17. Notification Preferences ✅ IMPLEMENTED

**Problem**: All-or-nothing notifications. Users want granular control.

**COMPLETED**:
- [x] **17.1** Create notification preferences model
  - Created `NotificationPreferences` model in `backend/internal/models/user.go`
  - Stores per-user preferences for: likes, comments, follows, mentions, DMs, stories, reposts, challenges
  - All notifications enabled by default
  - Auto-migrates with database startup

- [x] **17.2** Notification settings UI
  - Created `NotificationSettings` component in plugin (`plugin/src/ui/profile/NotificationSettings.h/cpp`)
  - Toggle for each notification type organized by category:
    - Social: Likes, Comments, New Followers, Mentions
    - Content: Stories, Reposts
    - Activity: Direct Messages, MIDI Challenges
  - Accessible from Profile view via "Notifications" button
  - Changes save immediately to backend

- [x] **17.3** Apply preferences in backend
  - Created notification preferences handler (`backend/internal/handlers/notification_preferences.go`)
  - Endpoints: `GET/PUT /api/v1/notifications/preferences`
  - Stream client checks preferences before sending notifications
  - Skipped notifications logged with 🔕 indicator

### 18. Keyboard Shortcuts & Accessibility

**Problem**: Limited keyboard navigation. Not accessible.

**TODO**:
- [ ] **18.1** Global keyboard shortcuts
  - `J/K` - Navigate posts
  - `L` - Like current post
  - `C` - Open comments
  - `Space` - Play/pause
  - `R` - Start recording
  - `Escape` - Close modals/go back

- [ ] **18.2** Focus management
  - Tab navigation through all elements
  - Focus indicators
  - Aria labels (conceptually)

- [ ] **18.3** Document shortcuts
  - Help screen with all shortcuts
  - Accessible via `?` key


### 19. Let users download the audio and midi of a post or story

Problem: users might wanna do stuff like use what they find in the plugin in their DAW to make music, or just have the file.
Solution: unlike instagram and tiktok, we'll provide file downloads to our media.

**TODO**:
- [ ] **19.1** File download endpoints for the backend for a post's audio and midi
  - the midi needs to be formatted as a midi file DAWs can import
- [ ] **19.2** Plugin needs to implement these endpoints in the ui to let users download the audio and midi of any post or story in the plugin.
---

## P2: Medium Priority - Differentiators

### 19. Remix Chains (Complete)

**Current State**: Backend has `remix_of_post_id` but UI not complete.

**TODO**:
- [ ] **19.1** Add "Remix" button to posts
  - Opens remix recording interface
  - Shows original post info

- [ ] **19.2** Remix chain visualization
  - Tree view of remixes
  - Navigate between remixes
  - "Original" badge on source

- [ ] **19.3** Remix notifications
  - Notify when someone remixes your post
  - Link to remix

### 20. Interactive Story Elements

**Problem**: Stories are passive. Instagram has polls, questions, quizzes.

**TODO**:
- [ ] **20.1** Key guess poll
  - "Guess the key" interactive element
  - Reveal answer after voting

- [ ] **20.2** "Ask me anything" for producers
  - Question sticker in stories
  - Answers appear as new stories

- [ ] **20.3** Reaction slider
  - "How fire is this beat?" 1-100 slider
  - See average response

### 21. Collaboration Mode

**Problem**: No way to collaborate on content (like TikTok duet).

**TODO**:
- [ ] **21.1** Side-by-side collaboration
  - Record alongside another post
  - Split screen playback

- [ ] **21.2** Layered collaboration
  - Add your layer to existing post
  - Mix of original + your addition

- [ ] **21.3** Collaboration requests
  - Request to collab with someone
  - Accept/deny system

### 22. Analytics Dashboard (Creator Tools)

**Problem**: No insights into performance.

**TODO**:
- [ ] **22.1** Basic analytics
  - Views, plays, play duration
  - Likes, comments, saves
  - Follower growth

- [ ] **22.2** Content analytics
  - Best performing posts
  - Peak activity times
  - Audience demographics

- [ ] **22.3** Analytics UI
  - Graphs and charts
  - Time period filters
  - Export data

### 23. Verified Badges

**Problem**: No verification system for established producers.

**TODO**:
- [ ] **23.1** Add `is_verified` field to User
- [ ] **23.2** Display verified badge
  - Checkmark next to name
  - On posts, profile, comments

- [ ] **23.3** Verification request system (phase 2)
  - Manual review process
  - Criteria documentation

### 24. Multiple Accounts

**Problem**: Can't switch between accounts.

**TODO**:
- [ ] **24.1** Account switcher UI
  - List of logged-in accounts
  - Quick switch dropdown

- [ ] **24.2** Secure multi-account storage
  - Separate tokens per account
  - Clear all accounts option

- [ ] **24.3** "Add Account" flow
  - Login to additional account
  - Max 5 accounts

### 25. QR Code Profile Sharing

**Problem**: Sharing profiles requires copying links.

**TODO**:
- [ ] **25.1** Generate profile QR code
  - Unique QR for each user
  - Scannable to open profile

- [ ] **25.2** QR code viewer in app
  - Scan QR to follow/view profile
  - Use device camera or file

- [ ] **25.3** QR in profile UI
  - "Share Profile" → show QR
  - Download QR option

---

## P3: Low Priority - Future Enhancements

### 26. Post Scheduling

**TODO**:
- [ ] Schedule posts for later
- [ ] Calendar view of scheduled posts
- [ ] Edit/cancel scheduled posts

### 27. Data Export (GDPR)

**TODO**:
- [ ] "Download Your Data" option
- [ ] Export all posts, comments, DMs
- [ ] JSON or ZIP format

### 28. Close Friends Lists

**TODO**:
- [ ] Create "Close Friends" list
- [ ] Stories visible only to close friends
- [ ] Green ring indicator

### 29. Story Templates

**TODO**:
- [ ] Pre-designed story layouts
- [ ] Add text, stickers, filters
- [ ] Save custom templates

### 30. AI-Powered Features

**TODO**:
- [ ] Auto-generate post descriptions
- [ ] BPM/key detection improvements
- [ ] Similar sound recommendations

---

## UI/UX Polish Checklist

### Animations & Transitions

- [ ] Smooth view transitions (slide, fade)
- [ ] Like button bounce animation
- [ ] Pull-to-refresh animation
- [ ] Story progress bar
- [ ] Typing indicator animation
- [ ] Loading skeleton shimmer
- [ ] Error shake animation
- [ ] Success confetti (for milestones)

### Visual Consistency

- [ ] Consistent button styles (primary, secondary, ghost)
- [ ] Consistent spacing (8pt grid)
- [ ] Consistent typography scale
- [ ] Icon style guide (outlined vs filled)
- [ ] Color palette documentation
- [ ] Dark mode refinements

### Micro-interactions

- [ ] Button hover states (all buttons)
- [ ] Input focus states
- [ ] Card hover elevation
- [ ] Avatar hover zoom
- [ ] Link hover underline
- [ ] Tab switch animation

### Empty States

- [ ] Empty feed: "Follow producers to see their posts"
- [ ] Empty profile posts: "Share your first loop!"
- [ ] Empty saved: "Save posts to see them here"
- [ ] Empty messages: "Start a conversation"
- [ ] Empty search: "Try searching for..."
- [ ] Empty notifications: "No notifications yet"

### Sound Design (Optional but Impactful)

- [ ] Like sound effect
- [ ] Comment sound effect
- [ ] Message sent sound
- [ ] New notification sound
- [ ] Recording start/stop sounds
- [ ] Upload success sound

---

## Technical Debt & Cleanup

### Code Quality

- [ ] Remove hardcoded localhost URLs in AuthComponent
- [ ] Fix responsive layout issues in ProfileSetup
- [ ] Add missing unit tests (see Phase 4.5 in PLAN.md)
- [ ] Increase code coverage to 50%+

### Performance

- [ ] Lazy load images/avatars
- [ ] Virtualize long lists (feed, comments)
- [ ] Optimize waveform rendering
- [ ] Cache audio playback data
- [ ] Reduce bundle size

### Stability

- [ ] Add crash reporting (Sentry integration)
- [ ] Add analytics tracking
- [ ] Add feature flags
- [ ] A/B testing framework

---

## Integration Checklist (What's Already Done)

### Fully Implemented
- [x] Email/password authentication
- [x] OAuth (Google, Discord)
- [x] Audio recording with waveform
- [x] Audio upload and processing
- [x] Feed with post cards
- [x] Likes with animation
- [x] Emoji reactions
- [x] Comments with threading (1 level)
- [x] Follow/unfollow system
- [x] User profiles with editing
- [x] User discovery and search
- [x] Stories (record, view, MIDI visualization)
- [x] Story expiration (24 hours)
- [x] Direct messaging (1-to-1)
- [x] Group messaging
- [x] Message threading and replies
- [x] Audio snippets in messages
- [x] Notifications with bell and badge
- [x] Real-time WebSocket updates
- [x] Collaborative playlists
- [x] MIDI challenges
- [x] Hidden synth easter egg
- [x] MIDI download from posts
- [x] Project file sharing
- [x] Content moderation (report, block)
- [x] Drop to Track feature
- [x] BPM/Key auto-detection

### Partially Implemented (Needs Completion)
- [~] Story highlights (backend done, UI needed)
- [~] Remix chains (backend partially done, UI needed)
- [~] Presence system migration to getstream.io
- [~] Typing indicators (stub exists)
- [~] Read receipts (stub exists)

### Not Started
- [ ] Save/bookmark posts
- [ ] Repost to feed
- [ ] Share to DMs
- [ ] Drafts system
- [ ] Offline mode
- [ ] Mute users
- [ ] Private accounts
- [ ] Pin posts
- [ ] 2FA
- [ ] Analytics
- [ ] Verified badges
- [ ] Multiple accounts

---

## Implementation Priority Matrix

| Priority | Feature | Effort | Impact | Dependencies |
|----------|---------|--------|--------|--------------|
| P0 | Story Highlights UI | Medium | High | Backend ready |
| P0 | Save/Bookmark | Low | High | New feature |
| P0 | Loading Skeletons | Low | High | None |
| P0 | Error States | Low | High | None |
| P0 | Drafts | Medium | High | Local storage |
| P1 | Repost | Medium | Medium | Feed changes |
| P1 | Share to DMs | Medium | Medium | Messages work |
| P1 | Mute Users | Low | Medium | Backend simple |
| P1 | Pin Posts | Low | Medium | Profile changes |
| P1 | Notification Prefs | Medium | Medium | Settings UI |
| P2 | Remix Chains UI | High | Medium | Backend ready |
| P2 | Analytics | High | Medium | New system |
| P2 | Verified Badges | Low | Low | Manual process |

---

## Suggested Sprint Plan

### Sprint 1: Foundation Polish (1-2 weeks)
1. Loading skeletons for all views
2. Error states with retry
3. Story highlights UI
4. Save/bookmark posts

### Sprint 2: Social Features (1-2 weeks)
1. Drafts system
2. Repost to feed
3. Share posts to DMs
4. Mute users

### Sprint 3: Profile Enhancements (1 week)
1. Pin posts to profile
2. Archive posts
3. Notification preferences
4. Activity status toggle

### Sprint 4: Creator Tools (2 weeks)
1. Basic analytics
2. Remix chains UI completion
3. Post scheduling (basic)
4. 2FA implementation

### Sprint 5: Polish & Launch Prep (1 week)
1. Animation polish
2. Sound design
3. Performance optimization
4. Bug fixes and testing

---

## Success Metrics

### User Engagement
- DAU/MAU ratio > 50%
- Average session length > 5 minutes
- Posts per active user > 0.5/week
- Stories viewed per session > 3

### Social Features
- Follow ratio (followers/following) balanced
- Comment rate > 5% of views
- Like rate > 15% of views
- Save rate > 3% of views

### Technical
- App crash rate < 1%
- Feed load time < 500ms
- Audio playback start < 100ms
- Upload success rate > 99%

---

*This TODO list should be treated as a living document. Update as features are completed and new requirements emerge.*
