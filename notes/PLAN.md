# Sidechain Development Plan

> **Vision**: Instagram Reels / TikTok / YouTube Shorts — but for musicians, with audio.
> Share loops directly from your DAW, discover what other producers are making.

**Last Updated**: December 5, 2024
**Developer**: Solo project
**Target**: Professional launch with real users

---

## How to Test Each Phase

> **Quick Reference**: Commands and UI interactions to verify each phase works correctly.

### Test Commands Summary
```bash
# Backend tests (from project root)
make test              # Run all backend tests
make test-unit         # Unit tests only
make test-integration  # Integration tests only (requires database)
make test-coverage     # Generate coverage report

# Plugin tests (from project root)
make test-plugin-unit     # Run plugin unit tests (42 test cases)
make test-plugin-coverage # Generate plugin coverage report

# Manual testing
make backend-dev       # Start backend at localhost:8787
make plugin            # Build plugin for manual testing
```

### Phase-by-Phase Testing

#### Phase 1: Foundation
| Section | How to Test |
|---------|-------------|
| **1.1 Build System** | `make plugin` - should build VST3 without errors |
| **1.2 getstream.io** | `make test-unit` - run `stream/client_test.go` (17 tests) |
| **1.3 OAuth** | Start backend, open plugin, click Google/Discord login in UI |
| **1.4 NetworkClient** | `make test-plugin-unit` - NetworkClientTest (24 tests) |

#### Phase 2: Audio Capture
| Section | How to Test |
|---------|-------------|
| **2.1 DAW Integration** | Load plugin in AudioPluginHost, click Record, verify waveform appears |
| **2.2 Audio Encoding** | `make test-plugin-unit` - AudioCaptureTest (17 tests) |
| **2.3 Upload Flow** | Record audio, enter metadata, click Upload - verify success message |
| **2.4 Backend Processing** | `make test-unit` - `queue/audio_jobs_test.go` (5 tests) + `queue/ffmpeg_test.go` (3 tests) |

#### Phase 3: Feed Experience
| Section | How to Test |
|---------|-------------|
| **3.1 Feed Data** | `make test-plugin-unit` - FeedDataTest (16 tests) |
| **3.2 Post Card** | Open plugin feed tab, verify posts display with waveform, avatar, metadata |
| **3.3 Audio Playback** | Click play on any post, verify audio plays with waveform progress |
| **3.4 Social Interactions** | Click heart icon on post, verify animation plays and count updates |

#### Phase 4: User Profiles
| Section | How to Test |
|---------|-------------|
| **4.1 Profile Data** | `make test-unit` - `handlers/auth_test.go` (11 tests for profile picture) |
| **4.2 Profile UI** | Click profile avatar in header, verify profile page loads with stats |
| **4.3 Follow System** | Click Follow button on another user, verify button changes to "Following" |
| **4.4 User Discovery** | Click search icon, type username, verify results appear |

#### Phase 5: Real-time Features
| Section | How to Test |
|---------|-------------|
| **5.1 WebSocket** | `make test-unit` - `websocket/websocket_test.go` (16 tests) |
| **5.2 Plugin WS Client** | Check connection indicator (green = connected) in plugin header |
| **5.3 Presence** | Open plugin, verify your presence shows "in studio" to followers |
| **5.4 Notifications** | Have another user like your post, verify bell badge increments |

#### Phase 6-12: Advanced Features
| Phase | How to Test |
|-------|-------------|
| **6: Comments** | (Not yet implemented) - Will need `comments_test.go` tests |
| **6.5: Messaging** | Open plugin → Click message icon → Start conversation → Send message → Verify real-time delivery |
| **7: Search** | (Not yet implemented) - Test via search bar in plugin UI |
| **7.5: Stories** | Record story in plugin → View in feed → Verify MIDI visualization → Wait 24 hours → Verify expiration |
| **8: Polish** | Load plugin in multiple DAWs (Ableton, FL Studio, Logic) |
| **9: Infrastructure** | `docker-compose up` - verify all services start |
| **10: Launch** | Run installer on fresh system, verify plugin loads |
| **11: Beta** | Collect feedback from beta testers |
| **12: Public** | Monitor error rates in production dashboard |

### Manual Testing Checklist

**Authentication Flow:**
- [x] Open plugin → Welcome screen shows
- [ ] Click "Sign Up" → Registration form works
- [ ] Click "Log In" → Login with credentials works
- [x] Click Google/Discord → OAuth flow completes
- [x] Close/reopen plugin → Stays logged in

**Recording Flow:**
- [x] Click Record → Red indicator appears
- [x] Wait 5+ seconds → Timer updates
- [ ] Click Stop → Waveform preview shows
- [ ] Click Play preview → Audio plays back
- [ ] Enter BPM/key → Fields accept input
- [ ] Click Upload → Progress bar shows
- [ ] Upload completes → Success confirmation

**Feed Flow:**
- [ ] Switch to Global tab → Posts load
- [ ] Switch to Timeline tab → Following posts show (or empty state)
- [ ] Scroll down → More posts load (pagination)
- [ ] Click play on post → Audio plays
- [ ] Click waveform → Seeks to position
- [ ] Click heart → Like animation plays
- [ ] Click share → URL copied to clipboard

**Profile Flow:**
- [ ] Click profile avatar → Profile page opens
- [ ] Click Edit Profile → Edit form opens
- [ ] Change bio → Save succeeds
- [ ] Upload profile picture → Picture updates
- [ ] Click followers count → Followers list shows

---

## UI Navigation Audit (Dec 5, 2024)

> **CRITICAL FINDING**: Several core user journeys are broken or inaccessible due to missing navigation elements.

### Navigation Summary

**Current App Views** (defined in `PluginEditor.h`):
- `Authentication` - Login/register screens
- `ProfileSetup` - First-time profile setup
- `PostsFeed` - Main feed with tabs (Following/Trending/Global)
- `Recording` - Audio capture screen
- `Upload` - Post metadata and share
- `Profile` - User profile view
- `Discovery` - User search/discovery

**Header Elements** (visible on all post-login screens):
| Element | Action | Status |
|---------|--------|--------|
| Logo | Goes to feed | ✅ Works |
| Search icon | Opens Discovery | ✅ Works |
| Profile avatar | Opens ProfileSetup | ✅ Works |
| **Record button** | **MISSING** | ❌ **No way to navigate to Recording!** |

### Critical Navigation Gaps

#### 1. Cannot Navigate to Recording Screen
**Problem**: The "Start Recording" button ONLY appears when the feed is EMPTY. Once there are posts in the global feed, **there is no way to create a new post**.

**Current behavior** (`PostsFeedComponent.cpp:712`):
```cpp
// Record button only shows in EMPTY state!
if (feedState == FeedState::Empty && getRecordButtonBounds().contains(pos))
{
    if (onStartRecording)
        onStartRecording();
}
```

**Required Fix**: Add a floating action button (FAB) or header button to navigate to Recording from any screen.

**Status**: See **Phase 1.5.1** for detailed implementation plan.

**Proposed Solutions**:
1. **Option A**: Add "+ Record" button to HeaderComponent (next to search icon) - **Recommended**
2. **Option B**: Add floating action button (FAB) to PostsFeedComponent (Instagram-style)
3. **Option C**: Add bottom navigation bar with Home/Record/Profile

#### 2. Password Reset Missing
**Problem**: No "Forgot Password" link on login screen, no password reset endpoint.

**Files affected**: `AuthComponent.cpp` line 602

**Required**:
- Add "Forgot Password?" link to login form
- Create backend endpoint `POST /api/v1/auth/reset-password`
- Create password reset email flow

#### 3. Cannot Unlike Posts
**Problem**: Clicking heart toggles to liked state, but clicking again doesn't unlike (no API call for unlike).

**Current** (`PostsFeedComponent.cpp:492-503`):
```cpp
card->onLikeToggled = [this, card](const FeedPost& post, bool liked) {
    // Optimistic UI update works...
    if (liked && networkClient != nullptr)
    {
        networkClient->likePost(post.id);  // Only likes, never unlikes!
    }
};
```

**Required Fix**: Call `networkClient->unlikePost(post.id)` when `!liked`.

**Status**: See **Phase 1.5.2** for detailed implementation plan.

### User Journey Test Flows

#### Journey 1: First-Time User Registration → First Post
| Step | Action | Expected | Actual |
|------|--------|----------|--------|
| 1 | Open plugin | See welcome screen | ✅ |
| 2 | Click "Sign Up" | See registration form | ✅ |
| 3 | Fill form, submit | Account created, profile setup | ✅ |
| 4 | Skip/complete profile setup | Go to feed | ✅ |
| 5 | **Find record button** | See way to record | ❌ **No button visible!** |
| 6 | Record audio | See waveform, timer | ⚠️ Unreachable |
| 7 | Stop recording | See preview | ⚠️ Unreachable |
| 8 | Fill metadata | Enter BPM/key/title | ⚠️ Unreachable |
| 9 | Click Share | Post created | ⚠️ Unreachable |

#### Journey 2: Returning User → Like a Post → Comment
| Step | Action | Expected | Actual |
|------|--------|----------|--------|
| 1 | Open plugin | Auto-login, see feed | ✅ |
| 2 | Click heart on post | Heart fills red, count increments | ✅ |
| 3 | Click heart again | Heart unfills, count decrements | ❌ **No unlike!** |
| 4 | Click comment icon | Comments panel slides in | ✅ |
| 5 | Type comment | Text input works | ✅ |
| 6 | Submit comment | Comment appears in list | ✅ |

#### Journey 3: Test Emoji Reactions
| Step | Action | Expected | Actual |
|------|--------|----------|--------|
| 1 | Login, see feed | Posts visible | ✅ |
| 2 | Long-press heart icon | Emoji picker appears | ✅ |
| 3 | Select emoji | Reaction sent to API | ✅ |
| 4 | See reaction on post | Post shows your reaction | ⚠️ Check FeedPost model |

### What's Actually Working (Verified in Code)

| Feature | Component | Backend API | Status |
|---------|-----------|-------------|--------|
| Login (email/password) | `AuthComponent` | `POST /auth/login` | ✅ |
| Registration | `AuthComponent` | `POST /auth/register` | ✅ |
| OAuth (Google/Discord) | `AuthComponent` | `GET /auth/{provider}` | ✅ |
| View feed (Global) | `PostsFeedComponent` | `GET /feed/global` | ✅ |
| View feed (Timeline) | `PostsFeedComponent` | `GET /feed/timeline` | ✅ |
| Play audio | `PostCardComponent` | CDN streaming | ✅ |
| Like post | `PostCardComponent` | `POST /social/like` | ✅ |
| Unlike post | `PostCardComponent` | `DELETE /social/like` | ❌ Not wired |
| Emoji reactions | `EmojiReactionsPanel` | `POST /social/react` | ✅ |
| View comments | `CommentsPanelComponent` | `GET /posts/:id/comments` | ✅ |
| Create comment | `CommentsPanelComponent` | `POST /posts/:id/comments` | ✅ |
| Delete comment | `CommentsPanelComponent` | `DELETE /comments/:id` | ✅ |
| Follow user | `PostCardComponent` | `POST /social/follow` | ✅ |
| Unfollow user | `PostCardComponent` | - | ❌ Not wired |
| View profile | `ProfileComponent` | `GET /users/:id/profile` | ✅ |
| Edit profile | `EditProfileComponent` | `PUT /users/me` | ✅ |
| Upload profile pic | `ProfileSetupComponent` | `POST /users/upload-profile-picture` | ✅ |
| **Recording** | `RecordingComponent` | - | ⚠️ No navigation |
| **Uploading** | `UploadComponent` | `POST /audio/upload` | ⚠️ No navigation |
| User discovery | `UserDiscoveryComponent` | `GET /search/users` | ✅ |
| Notifications | `NotificationListComponent` | `GET /notifications` | ✅ |

### Priority Fixes for Testable MVP

**P0 - Blocking (Must fix to test core flow):**
1. ❌ **Add Record button to Header or Feed** - Cannot create posts (see Phase 3.4.11)
2. ❌ **Wire up unlikePost()** - Cannot toggle likes off (see Phase 3.4.12)
3. ❌ **Auto-seed development data** - Fresh users see empty feed (see Phase 1.5)
4. ❌ **Wire Messages UI into navigation** - Messages component exists but inaccessible (see Phase 6.5.3.6)

**P1 - Important (Expected behavior missing):**
5. ❌ Wire up unfollowUser() - Cannot unfollow
6. ❌ Add "Forgot Password" link - Standard auth feature
7. ❌ Add logout confirmation - Currently logs out immediately

**P2 - Polish:**
8. ⚠️ Show loading states consistently
9. ⚠️ Error messages for failed operations
10. ⚠️ Profile completion progress indicator

---

## API Endpoint Testing Reference

> **Complete mapping of all 57 backend endpoints to plugin UI actions.**
> ✅ = Fully wired up in plugin UI
> ⚠️ = Partially implemented or TODO in code
> ❌ = Not yet connected to UI (backend ready, plugin needs work)

### Health & Infrastructure

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/health` | GET | ✅ | Connection indicator in header turns green when backend is reachable |

### Authentication Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/auth/register` | POST | ✅ | Click "Sign Up" → Fill form → Submit → Account created, redirects to profile setup |
| `/api/v1/auth/login` | POST | ✅ | Click "Log In" → Enter credentials → Submit → Logged in, shows feed |
| `/api/v1/auth/google` | GET | ✅ | Click Google icon → Browser opens Google OAuth page |
| `/api/v1/auth/google/callback` | GET | ✅ | (Browser) Complete Google login → Redirects back to callback URL |
| `/api/v1/auth/discord` | GET | ✅ | Click Discord icon → Browser opens Discord OAuth page |
| `/api/v1/auth/discord/callback` | GET | ✅ | (Browser) Complete Discord login → Redirects back to callback URL |
| `/api/v1/auth/oauth/poll` | GET | ✅ | After OAuth redirect, plugin polls this → Shows "Logging in..." → Success |
| `/api/v1/auth/me` | GET | ✅ | Open plugin when logged in → Header shows username and avatar |

### Audio Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/audio/upload` | POST | ✅ | Record audio → Fill metadata → Click Upload → Progress bar → Success |
| `/api/v1/audio/status/:job_id` | GET | ⚠️ | After upload, plugin should poll this (check if implemented) |
| `/api/v1/audio/:id` | GET | ✅ | Click play on any post → Audio streams and plays through DAW |

### Feed Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/feed/timeline` | GET | ✅ | Click "Timeline" tab → Shows posts from users you follow |
| `/api/v1/feed/timeline/enriched` | GET | ⚠️ | Plugin uses basic timeline endpoint; enriched version available |
| `/api/v1/feed/timeline/aggregated` | GET | ❌ | Not wired up in plugin UI yet |
| `/api/v1/feed/global` | GET | ✅ | Click "Global" tab → Shows all public posts |
| `/api/v1/feed/global/enriched` | GET | ⚠️ | Plugin uses basic global endpoint; enriched version available |
| `/api/v1/feed/trending` | GET | ❌ | Not wired up in plugin UI yet |
| `/api/v1/feed/post` | POST | ✅ | Complete upload flow → New post appears in your profile and global feed |

### Notification Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/notifications` | GET | ✅ | Click bell icon → Notification panel opens with list of notifications |
| `/api/v1/notifications/counts` | GET | ✅ | Bell icon badge shows unseen count (red number) |
| `/api/v1/notifications/read` | POST | ✅ | Click on a notification → Badge count decreases |
| `/api/v1/notifications/seen` | POST | ✅ | Open notification panel → All notifications marked as seen → Badge clears |

### Social Interaction Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/social/follow` | POST | ✅ | Click "Follow" button on user card → Button changes to "Following" |
| `/api/v1/social/unfollow` | POST | ❌ | **NOT IMPLEMENTED** - Plugin has TODO comment, needs `unfollowUser()` method |
| `/api/v1/social/like` | POST | ✅ | Click heart icon on post → Heart fills red → Like count increments |
| `/api/v1/social/like` | DELETE | ❌ | **NOT IMPLEMENTED** - Unlike endpoint not wired up, can only like (no toggle off) |
| `/api/v1/social/react` | POST | ✅ | Emoji reactions work via `likePost(activityId, emoji)` |

### User Profile Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/users/me` | GET | ✅ | Click profile avatar in header → Your profile page loads with all data |
| `/api/v1/users/me` | PUT | ✅ | EditProfileComponent calls `networkClient->put("/profile", ...)` → Save works |
| `/api/v1/users/username` | PUT | ❌ | **NOT IMPLEMENTED** - No username change UI in EditProfileComponent |
| `/api/v1/users/upload-profile-picture` | POST | ✅ | Edit Profile → Click avatar → Select image → Upload → New picture shows |
| `/api/v1/users/:id/profile` | GET | ⚠️ | ProfileComponent uses this but navigation may be inconsistent |
| `/api/v1/users/:id/profile-picture` | GET | ✅ | User avatars display throughout the app via proxy endpoint |
| `/api/v1/users/:id/posts` | GET | ⚠️ | Profile shows user posts but may not use this dedicated endpoint |
| `/api/v1/users/:id/followers` | GET | ❌ | **NOT IMPLEMENTED** - No followers list UI, just shows count |
| `/api/v1/users/:id/following` | GET | ❌ | **NOT IMPLEMENTED** - No following list UI, just shows count |
| `/api/v1/users/:id/activity` | GET | ❌ | **NOT IMPLEMENTED** - Activity summary not shown in profile |
| `/api/v1/users/:id/similar` | GET | ✅ | NetworkClient has `getSimilarUsers()` method |
| `/api/v1/users/:id/follow` | POST | ⚠️ | ProfileComponent has follow button using alternate endpoint |
| `/api/v1/users/:id/follow` | DELETE | ❌ | **NOT IMPLEMENTED** - Unfollow from profile not working |

### Discovery & Search Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/search/users` | GET | ✅ | UserDiscoveryComponent: Type in search → Results appear |
| `/api/v1/discover/trending` | GET | ✅ | UserDiscoveryComponent: "Trending" section loads on open |
| `/api/v1/discover/featured` | GET | ✅ | UserDiscoveryComponent: "Featured Producers" section loads |
| `/api/v1/discover/suggested` | GET | ✅ | UserDiscoveryComponent: "Suggested for You" section loads |
| `/api/v1/discover/genres` | GET | ✅ | Genre dropdown populates with available genres |
| `/api/v1/discover/genre/:genre` | GET | ✅ | Select genre filter → Shows only producers in that genre |

### Comment Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/posts/:id/comments` | POST | ❌ | **NOT IMPLEMENTED** - CommentComponent.h exists but not wired to API |
| `/api/v1/posts/:id/comments` | GET | ❌ | **NOT IMPLEMENTED** - Comments panel has TODO in PostsFeedComponent |
| `/api/v1/comments/:id/replies` | GET | ❌ | **NOT IMPLEMENTED** - Threaded replies not built |
| `/api/v1/comments/:id` | PUT | ❌ | **NOT IMPLEMENTED** - Comment editing not built |
| `/api/v1/comments/:id` | DELETE | ❌ | **NOT IMPLEMENTED** - Comment deletion not built |
| `/api/v1/comments/:id/like` | POST | ❌ | **NOT IMPLEMENTED** - Comment likes not built |
| `/api/v1/comments/:id/like` | DELETE | ❌ | **NOT IMPLEMENTED** - Comment unlike not built |

### WebSocket Endpoints

| Endpoint | Method | Status | Plugin UI Test |
|----------|--------|--------|----------------|
| `/api/v1/ws` | GET (WS) | ✅ | Plugin connects on login → Connection indicator green |
| `/api/v1/ws/connect` | GET (WS) | ✅ | Alternative WS endpoint → Same as above |
| `/api/v1/ws/metrics` | GET | ❌ | Admin only - not exposed in plugin |
| `/api/v1/ws/online` | POST | ⚠️ | Presence sent via WebSocket, not HTTP POST |
| `/api/v1/ws/presence` | POST | ⚠️ | Presence sent via WebSocket, not HTTP POST |
| `/api/v1/ws/friends-in-studio` | GET | ❌ | **NOT IMPLEMENTED** - No "friends in studio" indicator in UI |

---

### Implementation Gaps Summary

**Critical (breaks core functionality):**
- ❌ `DELETE /api/v1/social/like` - Can't unlike posts (only like)
- ❌ `POST /api/v1/social/unfollow` - Can't unfollow users

**Important (expected features missing):**
- ❌ All comment endpoints - Comments icon is clickable but does nothing
- ❌ Followers/Following lists - Shows counts but can't view who
- ❌ Username change - No UI for `PUT /api/v1/users/username`

**Nice to have (not critical for MVP):**
- ❌ Aggregated timeline, Trending feed
- ❌ Activity summary on profiles
- ❌ Friends in studio indicator

### Testing Tips

**Two-Account Testing:**
Many social features require two accounts to test properly:
1. Create two test accounts (e.g., `testuser1` and `testuser2`)
2. Log in as `testuser1` in one plugin instance
3. Log in as `testuser2` in another (or use a second DAW)
4. Test: `testuser1` follows `testuser2` → `testuser2` gets notification

**Network Tab Debugging:**
The plugin's `HttpErrorHandler` (debug builds only) shows popups for failed requests:
- Build with `make plugin-debug`
- Failed requests show endpoint, status code, and error message

**curl Testing:**
For direct API testing, get a JWT token first:
```bash
# Login and get token
TOKEN=$(curl -s -X POST http://localhost:8787/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"test@example.com","password":"password123"}' | jq -r '.token')

# Use token for authenticated requests
curl http://localhost:8787/api/v1/users/me -H "Authorization: Bearer $TOKEN"
curl http://localhost:8787/api/v1/feed/global -H "Authorization: Bearer $TOKEN"
curl http://localhost:8787/api/v1/notifications -H "Authorization: Bearer $TOKEN"
```

---

## Status Report (Dec 5, 2024)

### Test Results
- **Plugin tests**: 42 test cases / 390 assertions passing (AudioCapture 17, FeedPost/FeedDataManager 16, NetworkClient 24+14 ProfilePicture, PluginEditor 5)
- **Backend tests**: 63+ test functions passing (auth 11 new + 1 suite, audio 3, queue 5, stream 17+, websocket 16, comments 20+, integration 10)
- **CI/CD**: All platform builds succeeding (macOS Intel/ARM64, Linux, Windows)

### Progress Summary
Completed through **Phase 5.4** (Notifications UI). The core functionality is taking shape:
- Full authentication flow (email/password + OAuth)
- Audio capture and recording with waveform visualization
- Feed system with post cards and playback
- User profiles with follow/unfollow system
- Profile editing with social links
- **Notification system** (Stream.io backend + plugin UI with bell, badge, read/seen)
- **Aggregated feeds** (trending, timeline grouping, user activity summary)
- **Presence system** (online/offline, "in studio" status)

### Test Coverage Gaps Identified
- ✅ **Profile picture upload** - Tests added (see 4.1.11)
  - Backend: 11 tests in `handlers/auth_test.go` (MockS3Uploader pattern)
  - Plugin: 14 tests in `tests/NetworkClientTest.cpp` (MIME types, error cases)

### Login Flow Issues
- ❌ `AuthComponent.cpp` has hardcoded `localhost:8787` URLs (should use NetworkClient)
- ❌ `ProfileSetupComponent.cpp` has hardcoded button positions (not responsive)
- ❌ OAuth polling lacks real-time status feedback
- See **8.3.11 Login Flow Improvements** for remediation plan

### Next Steps
1. ~~**Phase 4.1.11** - Profile picture upload tests~~ ✅ Complete (25 tests added)
2. **Phase 1.5** (CRITICAL) - Critical bug fixes for testable MVP (see below)
3. **Phase 4.5** (CRITICAL) - Comprehensive backend test coverage (see below)
4. **Phase 8.3.11** - Login flow and UI improvements
5. **Phase 6** (Comments & Community) - Comment system backend and UI
6. **Phase 5.5** (Real-time Feed Updates) - WebSocket push for new posts, likes

### Critical: Test Coverage Gaps (Dec 5, 2024)

| Package | Current | Target | Priority |
|---------|---------|--------|----------|
| `handlers` | 1.1% | 60%+ | 🔴 Critical |
| `stream` | 2.4% | 50%+ | 🔴 Critical |
| `auth` | 0.0% | 70%+ | 🔴 Critical |
| `websocket` | 9.4% | 40%+ | 🟡 High |
| `storage` | 0.0% | 40%+ | 🟡 High |
| `queue` | 14.2% | 50%+ | 🟡 High |
| `audio` | 16.8% | 40%+ | 🟢 Medium |

**`handlers.go` (2224 lines, 40+ endpoints) has NO tests** - this is the biggest gap.

---

## Phase 1.5: Critical Bug Fixes for Testable MVP (NEW - PRIORITY)

> **Status**: These fixes are blocking core user flows and must be completed before meaningful testing can occur.
> **Duration**: 1-2 days
> **Priority**: 🔴 CRITICAL - Blocks all user testing

### 1.5.1 Fix Recording Navigation (CRITICAL)

> **Problem**: The "Start Recording" button ONLY appears when the feed is EMPTY. Once there are posts in the global feed, **there is no way to create a new post**.
>
> **Current behavior** (`PostsFeedComponent.cpp:712`):
> ```cpp
> // Record button only shows in EMPTY state!
> if (feedState == FeedState::Empty && getRecordButtonBounds().contains(pos))
> {
>     if (onStartRecording)
>         onStartRecording();
> }
> ```
>
> **Impact**:
> - Fresh user logs in → sees empty feed → can record ✅
> - User runs seed data → feed has posts → **cannot record** ❌
> - User follows others → feed has posts → **cannot record** ❌

**Required Fix**: Add navigation to Recording screen from header or floating action button.

- [ ] 1.5.1.1 Add "Record" button to `HeaderComponent` (next to search icon)
  - File: `plugin/source/ui/common/HeaderComponent.cpp`
  - Add button with microphone icon
  - Position: Right side of header, between search and profile avatar
  - Style: Match existing header button styling

- [ ] 1.5.1.2 Wire up record button callback in `PluginEditor.cpp`
  - File: `plugin/source/PluginEditor.cpp`
  - Connect button click to `showView(AppView::Recording)`
  - Ensure button is visible on all post-login screens (Feed, Profile, Discovery)

- [ ] 1.5.1.3 Test recording flow from populated feed
  - Start with seed data loaded (feed has posts)
  - Click record button in header
  - Verify Recording screen opens
  - Complete recording and upload flow
  - Verify new post appears in feed

**Alternative Options** (if header button doesn't fit design):
- Option B: Add floating action button (FAB) to `PostsFeedComponent` (Instagram-style)
- Option C: Add bottom navigation bar with Home/Record/Profile/Messages

### 1.5.2 Fix Unlike Posts (MEDIUM)

> **Problem**: Clicking the heart icon toggles the UI to "liked" state, but clicking again doesn't unlike. The code only calls `likePost()`, never `unlikePost()`.
>
> **Current Code** (`PostsFeedComponent.cpp:492-503`):
> ```cpp
> card->onLikeToggled = [this, card](const FeedPost& post, bool liked) {
>     // Optimistic UI update works...
>     if (liked && networkClient != nullptr)
>     {
>         networkClient->likePost(post.id);  // Only likes, never unlikes!
>     }
> };
> ```

**Required Fix**: Call `unlikePost()` when `liked == false`.

- [ ] 1.5.2.1 Update like toggle handler in `PostsFeedComponent.cpp`
  - File: `plugin/source/ui/feed/PostsFeedComponent.cpp`
  - Add `else if (!liked && networkClient != nullptr)` branch
  - Call `networkClient->unlikePost(post.id)` when unliking
  - Ensure optimistic UI update works for both like and unlike

- [ ] 1.5.2.2 Verify `NetworkClient::unlikePost()` method exists
  - File: `plugin/source/network/NetworkClient.cpp`
  - If missing, implement `DELETE /api/v1/social/like/:post_id` call
  - Add error handling for unlike failures

- [ ] 1.5.2.3 Test like/unlike toggle
  - Click heart on post → Verify like API called, count increments
  - Click heart again → Verify unlike API called, count decrements
  - Refresh feed → Verify like state persists correctly

### 1.5.3 Auto-seed Development Data (HIGH)

> **Problem**: When a fresh user logs in, they see an **empty feed** with no posts. There's no seed data by default, so:
> - Cannot test feed scrolling
> - Cannot test post interactions (like, comment)
> - Cannot test audio playback
> - Cannot see what the app looks like with content
>
> **Current State**:
> - Seed data system exists (`backend/cmd/seed/main.go`)
> - Must be run manually: `cd backend && go run cmd/seed/main.go dev`
> - Seed data creates: 20 users, 50 posts, 100 comments, follow relationships, hashtags

**Required Fix**: Auto-seed on first backend startup (development only).

- [ ] 1.5.3.1 Add seed check to backend startup (development mode only)
  - File: `backend/cmd/server/main.go`
  - Check if running in development mode (`ENV=development` or `--dev` flag)
  - On first startup, check if seed data exists (query user count)
  - If no seed data, automatically run seed command
  - Log seed status: "Seed data found" or "Seeding database..."

- [ ] 1.5.3.2 Alternative: Add seed to `make dev` command
  - File: `backend/Makefile` or root `Makefile`
  - Add `seed-dev` target that runs seed command
  - Update `dev` target to run seed before starting server
  - Example: `make dev` → `make seed-dev && go run cmd/server/main.go`

- [ ] 1.5.3.3 Document seed data in README
  - File: `backend/README.md` or root `README.md`
  - Add section: "Development Setup"
  - Explain that seed data auto-loads in dev mode
  - Document manual seed commands for testing

- [ ] 1.5.3.4 Test auto-seed flow
  - Fresh database (no users)
  - Start backend in dev mode
  - Verify seed data is created automatically
  - Verify plugin shows populated feed after login

**Note**: Seed data should be **idempotent** - running it multiple times won't create duplicates (checks for existing data).

### 1.5.4 Wire Messages UI into Navigation (MEDIUM)

> **Problem**: `MessagesListComponent` exists but is **not instantiated** in `PluginEditor.cpp`. There's no way to access chat from the UI.
>
> **Files**:
> - `plugin/source/ui/messages/MessagesListComponent.cpp` - Exists ✅
> - `plugin/source/network/StreamChatClient.cpp` - Exists ✅
> - **Missing**: Integration in `PluginEditor.cpp` ❌

**Required Fix**: Add Messages to navigation and instantiate component.

- [ ] 1.5.4.1 Add messages icon to `HeaderComponent`
  - File: `plugin/source/ui/common/HeaderComponent.cpp`
  - Add message icon button (next to search icon)
  - Show unread badge (total unread count across all channels)
  - Click to open messages list view

- [ ] 1.5.4.2 Instantiate `MessagesListComponent` in `PluginEditor.cpp`
  - File: `plugin/source/PluginEditor.cpp`
  - Add `MessagesListComponent` member variable
  - Initialize in constructor with `StreamChatClient` reference
  - Add to view switching logic (new `AppView::Messages`)

- [ ] 1.5.4.3 Wire up navigation to messages view
  - Connect header message icon click to `showView(AppView::Messages)`
  - Ensure messages view is accessible from all screens
  - Test navigation: Header → Messages → Back to Feed

- [ ] 1.5.4.4 Test messages functionality
  - Open messages view
  - Verify conversations list loads
  - Start conversation with another user
  - Send message and verify real-time delivery

**Note**: This is a partial implementation. Full messaging features are in Phase 6.5. This fix just makes the existing UI accessible.

---

## Current State Assessment

### What's Working
- VST3 plugin loads in DAWs (tested in AudioPluginHost)
- Email/password registration and login
- JWT authentication with token persistence
- Professional dark theme UI with consistent styling
- PostgreSQL database with migrations
- S3 storage configuration
- CMake build system with AudioPluginHost
- **Full audio capture pipeline** (lock-free recording, circular buffer, level metering)
- **Waveform visualization** (real-time during recording, preview after)
- **Recording UI** (start/stop, level meters, time display, preview playback)
- **Audio playback engine** (streaming, caching, seek, volume control)
- **Feed with post cards** (waveform, play/pause, like, share)
- **User profiles** (avatar, stats, posts, follow/unfollow)
- **GitHub Actions CI/CD** (macOS/Linux/Windows builds, release automation)
- **Codecov integration** (component-based coverage tracking)

### What's Stubbed/Incomplete
- Upload UI flow (BPM/key input, progress indicator)
- Real-time WebSocket updates
- Search functionality

### Recently Completed
- **Phase 3.3 Audio Playback Engine** (AudioPlayer with streaming, LRU cache, seek)
- **Phase 3.4 Social Interactions** (likes with animation, share to clipboard)
- **Phase 4.2 Profile UI** (ProfileComponent, EditProfileComponent)
- **Backend profile system** (follow stats, user posts, profile editing)
- Plugin NetworkClient HTTP implementation (GET, POST, PUT, DELETE, multipart uploads)
- getstream.io Feeds V2 API integration (activities, reactions, follows)
- OAuth with Google/Discord (token exchange, user info, account linking)

### What's Missing for MVP
- Upload metadata UI (BPM, key, genre selection)
- Basic search (users, posts)
- Production deployment

### Critical Bugs Blocking Testing (See Phase 1.5)
- ❌ **Cannot navigate to Recording when feed has posts** - Record button only appears in empty state
- ❌ **Unlike posts doesn't work** - UI toggles but API call missing
- ❌ **Fresh users see empty feed** - Seed data must be run manually
- ❌ **Messages UI not accessible** - Component exists but not wired into navigation

---

## Phase 1: Foundation Completion
**Goal**: Get real data flowing through the system
**Duration**: 2 weeks

> **Note**: See **Phase 1.5** for critical bug fixes that must be completed before testing.

### 1.1 Build System Finalization

> **Testing**: Run `make plugin` from project root. Success = VST3 builds without errors.
> Also run `make plugin-debug` to verify debug builds work.

- [x] 1.1.1 Migrate from Projucer to CMake
- [x] 1.1.2 Add JUCE 8.0.11 as dependency (deps/JUCE)
- [x] 1.1.3 Configure CMakeLists.txt for VST3/AU/Standalone builds
- [x] 1.1.4 Add AudioPluginHost to CMake configuration
- [x] 1.1.5 Set up `cmake --install` for platform-specific VST3 paths
- [x] 1.1.6 Create GitHub Actions CI for macOS/Linux/Windows
- [x] 1.1.7 Test CI builds on all three platforms
- [x] 1.1.8 Add build badges to README (Codecov badge and sunburst graph)

### 1.2 getstream.io Integration (Critical Path)

> **Testing**: Run `cd backend && go test ./internal/stream/... -v` for 17+ unit tests.
> For integration tests with real getstream.io API: `go test ./internal/stream/... -tags=integration -v`
> Manual: Start backend, create a post via plugin, verify it appears in global feed.

> **Architecture Decision: getstream.io Feed Types**
>
> getstream.io supports three feed types, each with different capabilities:
>
> | Feed Type | Purpose | Key Features |
> |-----------|---------|--------------|
> | **Flat** | Chronological activities | Can be followed, supports ranking |
> | **Aggregated** | Grouped activities | "X and 5 others liked your loop" |
> | **Notification** | User notifications | Built-in read/unread tracking |
>
> **Current Implementation (Flat only):**
> - `user` - User's own posts (flat)
> - `timeline` - Posts from followed users (flat)
> - `global` - All posts discovery feed (flat)
>
> **Planned Additions:**
> - `notification` - Likes, follows, comments with read/unread (notification type) ✅ Created in dashboard
> - `timeline_aggregated` - Grouped timeline view (aggregated type)
> - `trending` - Genre/time-based trending (aggregated type)
>
> **Important**: Aggregation format is applied at **write time**. Changing the format only affects
> new activities, not historical data. Plan aggregation strategy before production data exists!

#### 1.2.1 Core Feeds (Complete)

- [x] 1.2.1.1 Create  getstream.io account and obtain API credentials
- [x] 1.2.1.2 Configure  getstream.io Feeds V2 (V3 is beta, not production-ready; using stream-go2/v8)
- [x] 1.2.1.3 Define feed groups: `user` (personal), `timeline` (following), `global` (all) in getstream dashboard
- [x] 1.2.1.4 Implement `CreateLoopActivity()` with real getstream.io API call (stream/client.go:95-168)
- [x] 1.2.1.5 Implement `GetUserTimeline()` with real getstream.io query (stream/client.go:170-198)
- [x] 1.2.1.6 Implement `GetGlobalFeed()` with pagination (stream/client.go:200-226)
- [x] 1.2.1.7 Implement `FollowUser()` / `UnfollowUser()` operations (stream/client.go:228-264)
- [x] 1.2.1.8 Implement `AddReaction()` for likes and emoji reactions (stream/client.go:266-316)
- [x] 1.2.1.9 Add getstream.io user creation on registration (auth/oauth.go:231-236)
- [x] 1.2.1.10 Write integration tests for getstream.io client (stream_integration_test.go - 9 tests)
- [x] 1.2.1.11 Remove mock data from stream/client.go - replaced with real API calls
- [x] 1.2.1.12 Write getstream.io client unit tests (client_test.go - 17 tests covering activity structs, feeds, notifications)

#### 1.2.2 Notification Feed (New - Dashboard Created)

> **Stream.io Notification Feeds** provide automatic grouping with read/seen tracking.
> Unlike flat feeds, they return `Unseen` and `Unread` counts, plus `IsSeen`/`IsRead` per group.
> This eliminates the need to build custom notification tracking in PostgreSQL.

**Dashboard Configuration (Complete):**
- [x] 1.2.2.1 Create `notification` feed group in getstream.io dashboard (type: Notification)
- [x] 1.2.2.2 Configure aggregation format: `{{ verb }}_{{ time.strftime("%Y-%m-%d") }}`
  - Groups by action type and day: "5 people liked your loops today"
  - Alternative format for target grouping: `{{ verb }}_{{ target }}_{{ time.strftime("%Y-%m-%d") }}`
  - This groups: "3 people liked [specific loop] today"

**Backend Implementation (Complete):**
- [x] 1.2.2.3 Add `FeedGroupNotification = "notification"` constant to stream/client.go (line 22)
- [x] 1.2.2.4 Implement `GetNotifications()` method using `NotificationFeed` type (lines 809-866):

```go
// NotificationGroup represents a grouped notification from getstream.io
type NotificationGroup struct {
    ID            string      `json:"id"`
    Group         string      `json:"group"`           // The aggregation group key
    Verb          string      `json:"verb"`            // "like", "follow", "comment"
    ActivityCount int         `json:"activity_count"`  // Total activities in group
    ActorCount    int         `json:"actor_count"`     // Unique actors
    Activities    []*Activity `json:"activities"`      // The actual activities (max 15)
    IsRead        bool        `json:"is_read"`
    IsSeen        bool        `json:"is_seen"`
    CreatedAt     string      `json:"created_at"`
    UpdatedAt     string      `json:"updated_at"`      // When group was last updated
}

type NotificationResponse struct {
    Groups  []*NotificationGroup `json:"groups"`
    Unseen  int                  `json:"unseen"`  // Total unseen notification groups
    Unread  int                  `json:"unread"`  // Total unread notification groups
}

// GetNotifications retrieves notifications with read/unread state
func (c *Client) GetNotifications(userID string, limit int) (*NotificationResponse, error) {
    ctx := context.Background()

    notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, userID)
    if err != nil {
        return nil, fmt.Errorf("failed to get notification feed: %w", err)
    }

    resp, err := notifFeed.GetActivities(ctx,
        stream.WithActivitiesLimit(limit),
        stream.WithEnrichReactionCounts(),
        stream.WithEnrichRecentReactionsLimit(3),
    )
    if err != nil {
        return nil, fmt.Errorf("failed to get notifications: %w", err)
    }

    // Parse the notification-specific response
    result := &NotificationResponse{
        Groups: make([]*NotificationGroup, 0),
        Unseen: resp.Unseen,  // getstream.io provides this automatically!
        Unread: resp.Unread,  // getstream.io provides this automatically!
    }

    for _, group := range resp.Results {
        notifGroup := &NotificationGroup{
            ID:            group.ID,
            Group:         group.Group,
            ActivityCount: group.ActivityCount,
            ActorCount:    group.ActorCount,
            IsRead:        group.IsRead,
            IsSeen:        group.IsSeen,
            CreatedAt:     group.CreatedAt.Format(time.RFC3339),
            UpdatedAt:     group.UpdatedAt.Format(time.RFC3339),
        }

        // Extract verb from first activity
        if len(group.Activities) > 0 {
            notifGroup.Verb = group.Activities[0].Verb
        }

        // Convert activities
        for _, act := range group.Activities {
            notifGroup.Activities = append(notifGroup.Activities, convertStreamActivity(&act))
        }

        result.Groups = append(result.Groups, notifGroup)
    }

    return result, nil
}
```

- [x] 1.2.2.5 Implement `MarkNotificationsRead()` and `MarkNotificationsSeen()` methods (lines 871-916):

```go
// MarkNotificationsRead marks notification groups as read
func (c *Client) MarkNotificationsRead(userID string, groupIDs []string) error {
    ctx := context.Background()
    notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, userID)
    if err != nil { return err }

    var markOpt stream.GetActivitiesOption
    if len(groupIDs) > 0 {
        markOpt = stream.WithNotificationsMarkRead(groupIDs...)
    } else {
        markOpt = stream.WithNotificationsMarkAllRead()
    }
    _, err = notifFeed.GetActivities(ctx, markOpt)
    return err
}

// MarkNotificationsSeen marks notification groups as seen (clears badge)
func (c *Client) MarkNotificationsSeen(userID string, groupIDs []string) error {
    ctx := context.Background()
    notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, userID)
    if err != nil { return err }

    var markOpt stream.GetActivitiesOption
    if len(groupIDs) > 0 {
        markOpt = stream.WithNotificationsMarkSeen(groupIDs...)
    } else {
        markOpt = stream.WithNotificationsMarkAllSeen()
    }
    _, err = notifFeed.GetActivities(ctx, markOpt)
    return err
}
```

- [x] 1.2.2.6 Implement `AddToNotificationFeed()` helper (lines 941-972) plus `NotifyLike()`, `NotifyFollow()`, `NotifyComment()`, `NotifyMention()` (lines 974-1030):

```go
// AddToNotificationFeed adds a notification activity for a user
// Called when someone likes, follows, or comments on their content
func (c *Client) AddToNotificationFeed(targetUserID, verb, objectID, actorID string, extra map[string]any) error {
    ctx := context.Background()

    // Don't notify yourself
    if targetUserID == actorID {
        return nil
    }

    notifFeed, err := c.feedsClient.NotificationFeed(FeedGroupNotification, targetUserID)
    if err != nil {
        return fmt.Errorf("failed to get notification feed: %w", err)
    }

    activity := stream.Activity{
        Actor:  fmt.Sprintf("user:%s", actorID),
        Verb:   verb,    // "like", "follow", "comment"
        Object: objectID, // "loop:123" or "user:456"
        Extra:  extra,
    }

    _, err = notifFeed.AddActivity(ctx, activity)
    if err != nil {
        return fmt.Errorf("failed to add notification: %w", err)
    }

    fmt.Printf("🔔 Notification: %s %s %s → user:%s\n", actorID, verb, objectID, targetUserID)
    return nil
}
```

- [x] 1.2.2.7 Update `AddReactionWithEmoji()` to also notify post owner - `AddReactionWithNotification()` in client.go:388-410
- [x] 1.2.2.8 Update `FollowUser()` to notify the followed user - `NotifyFollow()` called in client.go:303-307
- [x] 1.2.2.9 Create API endpoints for notifications (handlers.go:1091-1188, main.go:190-198):
  - `GET /api/v1/notifications` - Get paginated notifications with unseen/unread counts
  - `GET /api/v1/notifications/counts` - Get just unseen/unread counts for badge
  - `POST /api/v1/notifications/read` - Mark notifications as read
  - `POST /api/v1/notifications/seen` - Mark notifications as seen (clears badge)
- [x] 1.2.2.10 Write integration tests for notification feed (client_test.go:298-367)

#### 1.2.3 Aggregated Feeds (New)

> **Stream.io Aggregated Feeds** group activities by a configurable format.
> Each group contains up to 15 activities. Great for "X and 5 others did Y" displays.
>
> **Aggregation Format Syntax** (Jinja2-style):
> - `{{ actor }}` - The user performing the action
> - `{{ verb }}` - The action type (posted, liked, etc.)
> - `{{ target }}` - The object receiving the action
> - `{{ time.strftime("%Y-%m-%d") }}` - Date formatting
> - Supports conditionals: `{% if verb == 'follow' %}...{% endif %}`

**Dashboard Configuration (Needs manual setup in getstream.io dashboard):**
- [x] 1.2.3.1 Create `timeline_aggregated` feed group (type: Aggregated)
  - Aggregation format: `{{ actor }}_{{ verb }}_{{ time.strftime("%Y-%m-%d") }}`
  - Result: "BeatMaker123 posted 3 loops today"

- [x] 1.2.3.2 Create `trending` feed group (type: Aggregated)
  - Aggregation format: `{{ extra.genre }}_{{ time.strftime("%Y-%m-%d") }}`
  - Result: "15 new Electronic loops today"

- [x] 1.2.3.3 Create `user_activity` feed group (type: Aggregated) - for profile activity summary
  - Aggregation format: `{{ verb }}_{{ time.strftime("%Y-%W") }}`
  - Result: "Posted 5 loops this week"

**Backend Implementation (Complete):**
- [x] 1.2.3.4 Add aggregated feed constants (stream/client.go:24-27):

```go
const (
    FeedGroupUser               = "user"                // Flat - user's posts
    FeedGroupTimeline           = "timeline"            // Flat - following feed
    FeedGroupGlobal             = "global"              // Flat - all posts
    FeedGroupNotification       = "notification"        // Notification - alerts
    FeedGroupTimelineAggregated = "timeline_aggregated" // Aggregated - grouped timeline
    FeedGroupTrending           = "trending"            // Aggregated - by genre/time
    FeedGroupUserActivity       = "user_activity"       // Aggregated - profile summary
)
```

- [x] 1.2.3.5 Implement `ActivityGroup` type for aggregated responses (stream/client.go:1036-1052 as `AggregatedGroup`):

```go
// ActivityGroup represents a group of related activities from an aggregated feed
type ActivityGroup struct {
    ID            string      `json:"id"`
    Group         string      `json:"group"`           // The aggregation key
    Verb          string      `json:"verb"`            // Common verb for the group
    ActivityCount int         `json:"activity_count"`  // Total activities (may be > len(Activities))
    ActorCount    int         `json:"actor_count"`     // Unique actors in group
    Activities    []*Activity `json:"activities"`      // Up to 15 most recent
    CreatedAt     string      `json:"created_at"`
    UpdatedAt     string      `json:"updated_at"`
}

type AggregatedFeedResponse struct {
    Groups   []*ActivityGroup `json:"groups"`
    NextPage string           `json:"next_page,omitempty"`
}
```

- [x] 1.2.3.6 Implement `GetAggregatedTimeline()` (stream/client.go:1054-1078):

```go
// GetAggregatedTimeline returns timeline grouped by actor and day
// Shows "BeatMaker123 posted 3 loops today" style grouping
func (c *Client) GetAggregatedTimeline(userID string, limit int) (*AggregatedFeedResponse, error) {
    ctx := context.Background()

    aggFeed, err := c.feedsClient.AggregatedFeed(FeedGroupTimelineAggregated, userID)
    if err != nil {
        return nil, fmt.Errorf("failed to get aggregated timeline: %w", err)
    }

    resp, err := aggFeed.GetActivities(ctx,
        stream.WithActivitiesLimit(limit),
        stream.WithEnrichReactionCounts(),
    )
    if err != nil {
        return nil, fmt.Errorf("failed to query aggregated timeline: %w", err)
    }

    result := &AggregatedFeedResponse{
        Groups: make([]*ActivityGroup, 0, len(resp.Results)),
    }

    for _, group := range resp.Results {
        ag := &ActivityGroup{
            ID:            group.ID,
            Group:         group.Group,
            ActivityCount: group.ActivityCount,
            ActorCount:    group.ActorCount,
            CreatedAt:     group.CreatedAt.Format(time.RFC3339),
            UpdatedAt:     group.UpdatedAt.Format(time.RFC3339),
        }

        if len(group.Activities) > 0 {
            ag.Verb = group.Activities[0].Verb
        }

        for _, act := range group.Activities {
            ag.Activities = append(ag.Activities, convertStreamActivity(&act))
        }

        result.Groups = append(result.Groups, ag)
    }

    return result, nil
}
```

- [x] 1.2.3.7 Implement `GetTrendingByGenre()` for discovery page - `GetTrendingFeed()` at stream/client.go:1080-1102
- [x] 1.2.3.8 Implement `GetUserActivitySummary()` for profile pages (stream/client.go:1104-1125)
- [x] 1.2.3.9 Update `CreateLoopActivity()` to also post to aggregated feeds via "To" field (stream/client.go:169-191):

```go
// In CreateLoopActivity, update the "To" targets:
globalFeed, _ := c.feedsClient.FlatFeed(FeedGroupGlobal, "main")
trendingFeed, _ := c.feedsClient.AggregatedFeed(FeedGroupTrending, "main")
userActivityFeed, _ := c.feedsClient.AggregatedFeed(FeedGroupUserActivity, userID)

streamActivity.To = []string{
    globalFeed.ID(),       // Flat global discovery
    trendingFeed.ID(),     // Aggregated trending by genre
    userActivityFeed.ID(), // Aggregated user activity summary
}
```

- [x] 1.2.3.10 Create API endpoints for aggregated feeds (handlers.go:1190-1272, main.go:183,186,225):
  - `GET /api/v1/feed/timeline/aggregated` - Grouped timeline
  - `GET /api/v1/feed/trending` - Trending by genre
  - `GET /api/v1/users/:id/activity` - User activity summary
- [x] 1.2.3.11 Update `FollowUser()` to also follow for aggregated timeline (stream/client.go:296-300, FollowAggregatedFeed at lines 1172-1197)
- [x] 1.2.3.12 Write integration tests for aggregated feeds (client_test.go:332-367)

### 1.3 OAuth Completion

> **Testing**: Run `cd backend && go test ./internal/auth/... -v` for auth service tests.
> Manual: Open plugin → Click Google icon → Browser opens → Complete OAuth → Plugin shows logged in.
> Test account unification: Create account with email, then login with same email via OAuth.

- [x] 1.3.1 Register Google OAuth application (console.developers.google.com) (client secret added to backend/.env)
- [x] 1.3.2 Register Discord OAuth application (discord.com/developers)
- [x] 1.3.3 Implement `exchangeGoogleCode()` token exchange (oauth.go:187 - uses oauth2.Config.Exchange)
- [x] 1.3.4 Implement `getGoogleUserInfo()` profile fetch (oauth.go:186-216)
- [x] 1.3.5 Implement `exchangeDiscordCode()` token exchange (oauth.go:220 - uses oauth2.Config.Exchange)
- [x] 1.3.6 Implement `getDiscordUserInfo()` profile fetch (oauth.go:218-254)
- [x] 1.3.7 Test account unification (OAuth + native same email) (service_test.go:TestAccountUnification, TestReverseAccountUnification)
- [x] 1.3.8 Add OAuth error handling and user feedback (handlers/auth.go:226-268 - getOAuthErrorMessage, parseOAuthError)
- [x] 1.3.9 Store OAuth tokens for future refresh (oauth.go:104-124 - updateOAuthTokens)
- [x] 1.3.10 Write auth service unit tests (service_test.go - TestAuthServiceSuite with registration, login, account unification)

### 1.4 Plugin NetworkClient Implementation

> **Testing**: Run `make test-plugin-unit` to run NetworkClientTest.cpp (24 tests + 14 profile picture tests).
> Tests cover: config, auth token handling, HTTP methods, multipart uploads, MIME type detection.
> Manual: Check connection indicator in plugin header (green = connected, red = disconnected).

- [x] 1.4.1 Implement HTTP GET method with JUCE URL class (NetworkClient.cpp:513-628)
- [x] 1.4.2 Implement HTTP POST method with JSON body (NetworkClient.cpp:545-557)
- [x] 1.4.3 Implement HTTP POST multipart/form-data for file uploads (NetworkClient.cpp:707-793)
- [x] 1.4.4 Add Authorization header injection from stored token (NetworkClient.cpp:533-536)
- [x] 1.4.5 Implement JSON response parsing with juce::var (NetworkClient.cpp:600)
- [x] 1.4.6 Add request timeout handling (30s default) (NetworkClient.h:38, NetworkClient.cpp:542)
- [x] 1.4.7 Add retry logic for network failures (3 attempts) (NetworkClient.cpp:523-612)
- [x] 1.4.8 Implement async request queue (background thread) (juce::Thread::launch throughout)
- [x] 1.4.9 Add request cancellation support (NetworkClient.cpp:57-68, shuttingDown flag)
- [x] 1.4.10 Write connection status indicator (red/yellow/green) (ConnectionIndicator.h)
- [x] 1.4.11 Make API base URL configurable (dev/prod) (NetworkClient.h:42-51 Config struct)
- [x] 1.4.12 Write NetworkClient unit tests (NetworkClientTest.cpp - 24 tests covering config, auth, HTTP, multipart)

---

## Phase 2: Audio Capture Pipeline
**Goal**: Capture audio from DAW and upload to backend
**Duration**: 2 weeks

### 2.1 DAW Audio Integration ✅

> **Testing**: Run `make test-plugin-unit` to run AudioCaptureTest.cpp (17 tests).
> Tests cover: recording start/stop, level metering, waveform generation, audio export, buffer processing.
> Manual: Load plugin in AudioPluginHost → Click Record → Play audio through → Click Stop → Verify waveform.

- [x] 2.1.1 Study JUCE AudioProcessor processBlock() documentation
- [x] 2.1.2 Create circular buffer for audio capture (30 seconds @ 48kHz) - CircularAudioBuffer class with lock-free design
- [x] 2.1.3 Wire AudioCapture into PluginProcessor::processBlock() - AudioCapture::processBlock() called from processor
- [x] 2.1.4 Implement recording start/stop with UI button - RecordingComponent with start/stop controls
- [x] 2.1.5 Add recording state indicator (red dot, time elapsed) - RecordingComponent displays recording time
- [x] 2.1.6 Implement max recording length (60 seconds) - Configurable maxRecordingSeconds
- [x] 2.1.7 Add level metering during recording - LevelMeterComponent with real-time updates
- [x] 2.1.8 Handle sample rate changes gracefully - prepareToPlay() reinitializes buffers
- [x] 2.1.9 Handle buffer size changes gracefully - prepareToPlay() handles buffer changes
- [ ] 2.1.10 Test with mono/stereo/surround bus configurations
- [x] 2.1.11 Write AudioCapture unit tests (AudioCaptureTest.cpp - 17 tests covering recording, metering, waveform, export, processing)

### 2.2 Audio Encoding (Plugin Side)

> **Testing**: Included in AudioCaptureTest.cpp.
> Manual: Record audio → Preview plays back correctly → Export WAV/FLAC creates valid file in temp directory.
> Verify: Check exported file with audio player, confirm correct sample rate and bit depth.

- [x] 2.2.1 Research JUCE audio format writers (WAV, FLAC)
- [x] 2.2.2 Implement WAV export from circular buffer - AudioCapture::saveBufferToWavFile() with 16/24/32-bit support
- [x] 2.2.3 Add FLAC compression option (smaller uploads) - AudioCapture::saveBufferToFlacFile() with quality control
- [x] 2.2.4 Calculate audio duration and display to user - formatDuration(), formatDurationWithMs(), estimateFileSize()
- [x] 2.2.5 Generate simple waveform preview (client-side) - WaveformComponent with peak analysis
- [x] 2.2.6 Add trim controls (start/end points) - AudioCapture::trimBuffer(), trimBufferByTime()
- [x] 2.2.7 Implement fade in/out (50ms) to avoid clicks - applyFadeIn/Out() with Linear/Exponential/SCurve
- [x] 2.2.8 Normalize audio to -1dB peak before upload - normalizeBuffer() with dB target
- [x] 2.2.9 Store temporary files in OS temp directory - Uses juce::File::getSpecialLocation
- [x] 2.2.10 Clean up temp files after successful upload

### 2.3 Upload Flow ✅

> **Testing**: Manual testing required (UI-heavy feature).
> Steps: Record audio → Enter title, BPM (tap tempo or manual), key (auto-detect or manual), genre → Upload.
> Verify: Progress bar shows, success message appears, post appears in your profile and global feed.

- [x] 2.3.1 Design upload UI (title, BPM, key, genre selector) - UploadComponent with dark theme, waveform preview
- [x] 2.3.2 Implement BPM detection (tap tempo or auto-detect) - handleTapTempo() with 4-tap averaging, DAW BPM via AudioPlayHead
- [x] 2.3.3 Implement key detection (basic pitch analysis) - libkeyfinder integration with auto-detect button, manual dropdown (24 keys in Camelot order)
- [x] 2.3.4 Add genre dropdown (electronic, hip-hop, rock, etc.) - 12 producer-friendly genres
- [x] 2.3.5 Create upload progress indicator (0-100%) - Simulated progress bar with Timer callbacks
- [ ] 2.3.6 Implement chunked upload for large files - Not needed (backend handles streaming)
- [ ] 2.3.7 Add upload cancellation support - Basic cancel (returns to editing)
- [x] 2.3.8 Handle upload failures with retry option - Error state with tap-to-retry
- [x] 2.3.9 Show success confirmation with post preview - Shows title, genre, BPM, checkmark
- [x] 2.3.10 Clear recording buffer after successful upload - onUploadComplete callback triggers navigation

### 2.4 Backend Audio Processing ✅

> **Testing**: Run `cd backend && go test ./internal/queue/... -v` for audio job tests (5 tests) and FFmpeg tests (3 tests).
> Manual: Upload audio via plugin → Check backend logs for FFmpeg processing → Verify MP3 output in S3.
> API: `GET /api/v1/audio/status/:job_id` returns processing status (pending/processing/ready/error).

- [x] 2.4.1 Verify FFmpeg installation in deployment environment - CheckFFmpegAvailable() in queue/ffmpeg_helpers.go
- [x] 2.4.2 Implement background job queue (goroutines + channels) - AudioQueue with worker pool in queue/audio_jobs.go
- [x] 2.4.3 Process uploaded WAV/FLAC to MP3 (128kbps) - runFFmpegNormalize() with libmp3lame
- [x] 2.4.4 Apply loudness normalization (-14 LUFS) - loudnorm filter with I=-14:TP=-1:LRA=7
- [x] 2.4.5 Generate waveform data (JSON array of peaks) - generateSVGFromSamples() extracts float32 peaks
- [x] 2.4.6 Generate waveform SVG for embed - generateWaveformSVG() creates bar-style SVG
- [x] 2.4.7 Extract audio metadata (duration, sample rate) - extractAudioDuration() uses ffprobe JSON
- [x] 2.4.8 Upload processed MP3 to S3 - s3Uploader.UploadAudio() in processJob()
- [x] 2.4.9 Upload waveform assets to S3 - s3Uploader.UploadWaveform() in processJob()
- [x] 2.4.10 Update AudioPost record with S3 URLs - updateAudioPostComplete() with GORM
- [x] 2.4.11 Update AudioPost status (processing → ready) - updateAudioPostStatus() transitions state
- [x] 2.4.12 Notify plugin of processing completion (webhook or poll) - GET /api/v1/audio/status/:job_id endpoint
- [x] 2.4.13 Write unit tests for audio processing (ffmpeg_test.go - 3 tests, audio_jobs_test.go - 5 tests)
- [x] 2.4.14 Write integration tests for audio pipeline (audio_integration_test.go - 1 test)

---

## Phase 3: Feed Experience
**Goal**: Browse and interact with posts in the plugin
**Duration**: 2 weeks

### 3.1 Feed Data Layer

> **Testing**: Run `make test-plugin-unit` to run FeedDataTest.cpp (16 tests).
> Tests cover: FeedPost JSON parsing, FeedDataManager caching, pagination, error states.
> Manual: Open plugin feed → Verify posts load → Scroll down → Verify more posts load (pagination).

- [x] 3.1.1 Create FeedPost model in C++ (id, user, audio_url, waveform, etc.) - FeedPost.h/cpp with full JSON serialization
- [x] 3.1.2 Implement feed fetch from backend (paginated) - FeedDataManager with NetworkClient integration
- [x] 3.1.3 Parse JSON response into FeedPost objects - FeedPost::fromJson() with robust parsing
- [x] 3.1.4 Implement local cache (SQLite or JSON file) - JSON file cache in ~/.local/share/Sidechain/cache/
- [x] 3.1.5 Add cache invalidation strategy (5 minute TTL) - Configurable cacheTTLSeconds
- [x] 3.1.6 Implement pull-to-refresh gesture - Refresh button in PostsFeedComponent
- [x] 3.1.7 Add infinite scroll pagination - FeedDataManager::loadMorePosts() with offset tracking
- [x] 3.1.8 Handle empty feed state - PostsFeedComponent::drawEmptyState() with different messages per feed type
- [x] 3.1.9 Handle error states (network, server) - PostsFeedComponent::drawErrorState() with retry button
- [x] 3.1.10 Implement feed type switching (global, following) - Tab UI with Timeline/Global switching
- [x] 3.1.11 Write FeedPost and FeedDataManager unit tests (FeedDataTest.cpp - 16 tests)

### 3.2 Post Card Component

> **Testing**: Manual testing (visual component).
> Verify: Each post shows avatar (or initials), username, timestamp, waveform, BPM/key badges, social buttons.
> Test: Click play button → Audio starts → Waveform progress indicator moves.

- [x] 3.2.1 Design post card layout (avatar, username, waveform, controls) - PostCardComponent.h/cpp
- [x] 3.2.2 Implement user avatar display (circular, with fallback) - drawAvatar() with initials fallback
- [x] 3.2.3 Display username and post timestamp - drawUserInfo() with DAW badge
- [x] 3.2.4 Render waveform visualization - drawWaveform() with playback progress
- [x] 3.2.5 Add play/pause button overlay - drawPlayButton() with state toggle
- [x] 3.2.6 Display BPM and key badges - drawMetadataBadges() with genre tags
- [x] 3.2.7 Add like button with count - drawSocialButtons() with heart icon
- [x] 3.2.8 Add comment count indicator - Comment count in social buttons
- [x] 3.2.9 Add share button (copy link) - Share button bounds and callback
- [x] 3.2.10 Add "more" menu (report, hide, etc.) - More button bounds and callback
- [ ] 3.2.11 Implement card tap to expand details
- [ ] 3.2.12 Show post author online status (Phase 6.5.2.7) - Query getstream.io Chat presence for post author, show green dot on avatar if online

### 3.3 Audio Playback Engine

> **Testing**: Manual testing (audio output required).
> Test: Click play on post → Audio plays through DAW output → Click waveform to seek → Position updates.
> Test: Play post → Click another post → First post stops, second plays.
> Test: Enable auto-play → First post ends → Next post starts automatically.

- [x] 3.3.1 Research JUCE audio playback in plugin context - AudioFormatReaderSource + ResamplingAudioSource mixed into processBlock
- [x] 3.3.2 Create AudioPlayer class with transport controls - AudioPlayer.h/cpp with play/pause/stop/seek
- [x] 3.3.3 Implement audio streaming from URL (no full download) - Downloads to memory, caches for replay
- [x] 3.3.4 Add playback progress indicator on waveform - PostCardComponent::setPlaybackProgress() with callback integration
- [x] 3.3.5 Implement seek by tapping waveform - onWaveformClicked callback connected to AudioPlayer::seekToNormalizedPosition()
- [x] 3.3.6 Add volume control (separate from DAW) - AudioPlayer::setVolume() with atomic float
- [x] 3.3.7 Implement auto-play next post option - setAutoPlayEnabled(), setPlaylist(), playNext/playPrevious
- [x] 3.3.8 Handle audio focus (pause when DAW plays) - onDAWTransportStarted/Stopped with wasPlayingBeforeDAW state
- [x] 3.3.9 Add keyboard shortcuts (space = play/pause, arrows, M for mute) - KeyListener in PostsFeedComponent
- [x] 3.3.10 Cache recently played audio (memory limit) - LRU cache with 50MB limit, eviction strategy
- [ ] 3.3.11 Pre-buffer next post for seamless playback - preloadAudio() method available

### 3.4 Social Interactions

> **Testing**: Manual testing with two accounts.
> Test: Click heart → Animation plays → Count increments → Refresh feed → Like persists.
> Test: Click Follow → Button changes to "Following" → Check user's profile → Following count increased.
> Test: Click share → Clipboard contains link → Open link (future: web player).

- [x] 3.4.1 Implement like/unlike toggle (optimistic UI) - PostCardComponent handles UI state, callback wired up
- [x] 3.4.2 Add like animation (heart burst) - Timer-based animation with 6 expanding hearts, scale effect, ring
- [x] 3.4.3 Implement emoji reactions panel - EmojiReactionsPanel.cpp with long-press to show, 6 music-themed emojis
- [x] 3.4.4 Show reaction counts and types - drawReactionCounts() displays top 3 emoji reactions with counts below like button
- [x] 3.4.5 Implement follow/unfollow from post card - Follow button in PostCardComponent with optimistic UI
- [x] 3.4.6 Add "following" indicator on posts from followed users - isFollowing field, button shows "Following" state
- [x] 3.4.7 Implement play count tracking - NetworkClient::trackPlay() called on playback start
- [x] 3.4.8 Track listen duration (for algorithm) - Backend endpoint POST /api/v1/social/listen-duration, plugin tracks from start to stop
- [x] 3.4.9 Implement "Add to DAW" button (download to project folder) - File chooser downloads audio to user-selected location
- [x] 3.4.10 Add post sharing (generate shareable link) - Copies https://sidechain.live/post/{id} to clipboard
- [ ] 3.4.11 Add Record button to HeaderComponent (CRITICAL - see Phase 1.5.1) - Cannot navigate to Recording when feed has posts
- [ ] 3.4.12 Fix unlike posts functionality (see Phase 1.5.2) - Unlike API call missing when toggling like off

---

## Phase 4: User Profiles
**Goal**: Complete user identity and discovery
**Duration**: 1.5 weeks

### 4.1 Profile Data Model ✅

> **Testing**: Run `cd backend && go test ./internal/handlers/... -v` for handlers tests.
> Profile picture tests: `go test ./internal/handlers/... -run Profile -v` (11 tests in auth_test.go).
> API: `GET /api/v1/users/:id/profile` returns merged PostgreSQL + getstream.io data.

> **Architecture Decision**: Follower/following counts and likes are stored in getstream.io (source of truth).
> PostgreSQL stores only user profile metadata (bio, location, links). This avoids data sync issues.

- [x] 4.1.1 Extend User model: bio, location, genres, social_links (PostgreSQL)
- [x] 4.1.2 Add profile_picture_url field (S3 URL in PostgreSQL)
- [x] 4.1.3 Implement GetFollowStats() in Stream client (follower_count, following_count from getstream.io)
- [x] 4.1.4 Implement GetEnrichedActivities() with reaction counts (likes from getstream.io)
- [x] 4.1.5 Create migration for profile fields (bio, location, genres, social_links, profile_picture_url)
- [x] 4.1.6 Implement GET /api/users/:id/profile endpoint (merges PostgreSQL + getstream.io data)
- [x] 4.1.7 Implement PUT /api/users/profile endpoint (updates PostgreSQL profile fields)
- [x] 4.1.8 Add username change with uniqueness check - PUT /api/v1/users/username with validation (3-30 chars, alphanumeric/underscore/hyphen)
- [x] 4.1.9 Implement profile picture upload and crop - POST /upload-profile-picture persists URL to profile_picture_url field
- [ ] 4.1.10 Add profile verification system (future: badges)

#### 4.1.11 Profile Picture Upload Tests ✅

> **Test Coverage**: Profile picture upload now has comprehensive unit tests.
> - Plugin: `NetworkClient::uploadProfilePicture()` (NetworkClient.cpp:544-664) - 14 tests
> - Backend: `UploadProfilePicture` handler (auth.go:461-529) - 11 tests
> - S3: Mock interface `ProfilePictureUploader` (storage/interface.go)

**Backend Unit Tests (handlers/auth_test.go):**
- [x] 4.1.11.1 Create `handlers/auth_test.go` test file for auth handlers
- [x] 4.1.11.2 Test `UploadProfilePicture` with valid image (JPEG, PNG, GIF, WebP)
- [x] 4.1.11.3 Test `UploadProfilePicture` with invalid file type rejection (TXT, PDF)
- [x] 4.1.11.4 Test `UploadProfilePicture` with file size limits (max 10MB)
- [x] 4.1.11.5 Test `UploadProfilePicture` updates user's profile_picture_url in database
- [x] 4.1.11.6 Test `UploadProfilePicture` without authentication (401 response)
- [x] 4.1.11.7 Mock S3 uploader for isolated unit tests (MockS3Uploader struct)

**Plugin Unit Tests (tests/NetworkClientTest.cpp):**
- [x] 4.1.11.8 Add `uploadProfilePicture` tests to `NetworkClientTest.cpp` (14 new tests)
- [x] 4.1.11.9 Test unauthenticated upload returns immediately with failure
- [x] 4.1.11.10 Test non-existent file returns failure callback
- [x] 4.1.11.11 Test MIME type detection for JPEG, PNG, GIF, WebP
- [x] 4.1.11.12 Test edge cases: empty file, long filename, spaces, unicode characters

### 4.2 Profile UI (Plugin) ✅

> **Testing**: Run `make test-plugin-unit` to run PluginEditorTest.cpp (5 tests).
> Manual: Click profile avatar in header → Profile page loads → Verify stats, bio, social links display.
> Test: Click "Edit Profile" → Modal opens → Change bio → Save → Verify changes persist.

- [x] 4.2.1 Design profile view layout - ProfileComponent.h/cpp with full layout
- [x] 4.2.2 Display profile header (avatar, name, bio) - drawHeader(), drawAvatar(), drawUserInfo()
- [x] 4.2.3 Show follower/following counts (tappable) - drawStats() with clickable bounds
- [x] 4.2.4 Display user's posts in grid/list - PostCardComponent array with scroll
- [x] 4.2.5 Add follow/unfollow button - handleFollowToggle() with optimistic UI
- [ ] 4.2.12 Show online/offline status on profile (Phase 6.5.2.7) - Query getstream.io Chat presence, show green dot if online
- [x] 4.2.6 Implement edit profile modal (own profile) - EditProfileComponent.h/cpp
- [x] 4.2.7 Add social links display (Instagram, SoundCloud, etc.) - drawSocialLinks() with icons
- [x] 4.2.8 Show genre tags - drawGenreTags() with badge styling
- [x] 4.2.9 Display "member since" date - drawMemberSince()
- [x] 4.2.10 Add profile sharing (copy link) - shareProfile() copies URL to clipboard
- [x] 4.2.11 Write PluginEditor UI tests (PluginEditorTest.cpp - 5 tests covering initialization, auth, processor)
- [ ] 4.2.12 Show online/offline status on profile (Phase 6.5.2.7) - Query getstream.io Chat presence, show green dot/badge if online, "last active X ago" if offline

### 4.3 Follow System ✅

> **Testing**: Manual testing with two accounts.
> Test: User A follows User B → User A's timeline shows User B's posts.
> Test: User A unfollows User B → User B's posts disappear from User A's timeline.
> Test: Check mutual follow detection ("follows you" badge).

> **Architecture Decision**: getstream.io is the source of truth for follows.
> No PostgreSQL Follow table needed - getstream.io's feed follow system handles everything.

- [x] 4.3.1 ~~Create Follow model~~ → Using getstream.io feed follows (no PostgreSQL table)
- [x] 4.3.2 Add follow/unfollow endpoints (stream/client.go:FollowUser/UnfollowUser - already done!)
- [x] 4.3.3 Implement GET /api/users/:id/followers endpoint (paginated, via getstream.io GetFollowers)
- [x] 4.3.4 Implement GET /api/users/:id/following endpoint (paginated, via getstream.io GetFollowing)
- [x] 4.3.5 ~~Update getstream.io feed subscriptions on follow~~ → Automatic (Stream.io handles this)
- [x] 4.3.6 Add "suggested users to follow" endpoint - GET /api/v1/discover/suggested (genre-based + fallback to popular)
- [x] 4.3.7 Implement mutual follow detection ("follows you") via CheckIsFollowing()
- [x] 4.3.8 Add follow notifications - getstream.io NotifyFollow() + real-time WebSocket push via wsHandler.NotifyFollow()
- [ ] 4.3.9 Implement bulk follow import (future: follow your discord friends)

### 4.4 User Discovery ✅

> **Testing**: Manual testing via plugin UI.
> Test: Click search icon → Type username → Results appear → Click result → Profile opens.
> API tests: `curl http://localhost:8787/api/v1/search/users?q=test` (requires auth header).
> Test trending: `curl http://localhost:8787/api/v1/discover/trending`

- [x] 4.4.1 Implement user search endpoint (by username) - handlers.go:SearchUsers (GET /api/v1/search/users?q=)
- [x] 4.4.2 Add search UI in plugin (search bar) - UserDiscoveryComponent.h/cpp with TextEditor search box
- [x] 4.4.3 Show recent searches - Persisted to ~/.local/share/Sidechain/recent_searches.txt
- [x] 4.4.4 Implement trending users algorithm - handlers.go:GetTrendingUsers (GET /api/v1/discover/trending)
- [x] 4.4.5 Add "featured producers" section - handlers.go:GetFeaturedProducers (GET /api/v1/discover/featured)
- [x] 4.4.6 Implement genre-based user discovery - handlers.go:GetUsersByGenre (GET /api/v1/discover/genre/:genre)
- [x] 4.4.7 Add "producers you might like" recommendations - handlers.go:GetSuggestedUsers (GET /api/v1/discover/suggested)
- [x] 4.4.8 Show users with similar BPM/key preferences - handlers.go:GetSimilarUsers (GET /api/v1/users/:id/similar)
- [ ] 4.4.9 Show online status in user discovery (Phase 6.5.2.7) - Query getstream.io Chat presence for discovered users, show online indicators

---

## Phase 4.5: Comprehensive Backend Testing (NEW - CRITICAL)
**Goal**: Achieve 50%+ code coverage across all backend packages
**Priority**: CRITICAL - Low test coverage is a deployment blocker

> **Current State (Dec 5, 2024)**:
> - `handlers.go` (2224 lines, 40+ endpoints) has **NO tests**
> - Overall backend coverage is under 10%
> - Only `comments_test.go` and `auth_test.go` exist for handlers

### 4.5.1 Core Handlers Tests (`handlers_test.go`)

> **Testing Pattern**: Use testify suite with PostgreSQL test database and mock getstream.io client.
> Each test should setup/teardown cleanly. Use table-driven tests for similar endpoints.

**Feed Endpoints (Priority 1):**
- [ ] 4.5.1.1 Test `GetTimeline` - authenticated user, pagination, empty feed
- [ ] 4.5.1.2 Test `GetGlobalFeed` - pagination, ordering
- [ ] 4.5.1.3 Test `CreatePost` - valid post, missing fields, audio URL validation
- [ ] 4.5.1.4 Test `GetEnrichedTimeline` - enrichment with user data
- [ ] 4.5.1.5 Test `GetEnrichedGlobalFeed` - enrichment with reactions

**Social Endpoints (Priority 1):**
- [ ] 4.5.1.6 Test `FollowUser` - follow, already following, self-follow rejection
- [ ] 4.5.1.7 Test `UnfollowUser` - unfollow, not following error
- [ ] 4.5.1.8 Test `FollowUserByID` / `UnfollowUserByID` - path param versions
- [ ] 4.5.1.9 Test `LikePost` - like, already liked, invalid post
- [ ] 4.5.1.10 Test `UnlikePost` - unlike, not liked error
- [ ] 4.5.1.11 Test `EmojiReact` - various emoji types

**Profile Endpoints (Priority 2):**
- [ ] 4.5.1.12 Test `GetProfile` / `GetMyProfile` - own profile vs other user
- [ ] 4.5.1.13 Test `UpdateProfile` - valid update, validation errors
- [ ] 4.5.1.14 Test `ChangeUsername` - unique check, format validation
- [ ] 4.5.1.15 Test `GetUserProfile` - public profile data
- [ ] 4.5.1.16 Test `GetUserFollowers` / `GetUserFollowing` - pagination

**Notification Endpoints (Priority 2):**
- [ ] 4.5.1.17 Test `GetNotifications` - unseen/unread counts
- [ ] 4.5.1.18 Test `GetNotificationCounts` - badge count
- [ ] 4.5.1.19 Test `MarkNotificationsRead` / `MarkNotificationsSeen`

**Discovery Endpoints (Priority 3):**
- [ ] 4.5.1.20 Test `SearchUsers` - query, empty results
- [ ] 4.5.1.21 Test `GetTrendingUsers` - ordering, limits
- [ ] 4.5.1.22 Test `GetFeaturedProducers` - curated list
- [ ] 4.5.1.23 Test `GetSuggestedUsers` - recommendations
- [ ] 4.5.1.24 Test `GetUsersByGenre` - genre filter

**Audio Endpoints (Priority 3):**
- [ ] 4.5.1.25 Test `UploadAudio` - valid upload, invalid file type
- [ ] 4.5.1.26 Test `GetAudioProcessingStatus` - pending, complete, error states
- [ ] 4.5.1.27 Test `GetAudio` - existing audio, 404 for missing

**Aggregated Feed Endpoints (Priority 3):**
- [ ] 4.5.1.28 Test `GetAggregatedTimeline` - grouping
- [ ] 4.5.1.29 Test `GetTrendingFeed` - trending algorithm
- [ ] 4.5.1.30 Test `GetUserActivitySummary` - activity grouping

### 4.5.2 Auth Service Tests (`auth/service_test.go`)

> **Note**: `auth_test.go` in handlers tests the HTTP layer. This tests the service logic.

- [ ] 4.5.2.1 Test `RegisterNativeUser` - success, duplicate email, duplicate username
- [ ] 4.5.2.2 Test `AuthenticateNativeUser` - correct password, wrong password, non-existent user
- [ ] 4.5.2.3 Test `GenerateJWT` - token structure, claims, expiry
- [ ] 4.5.2.4 Test `ValidateJWT` - valid token, expired token, tampered token
- [ ] 4.5.2.5 Test `FindUserByEmail` - found, not found
- [ ] 4.5.2.6 Test `GetUserFromToken` - valid, invalid, expired
- [ ] 4.5.2.7 Test OAuth token storage and refresh

### 4.5.3 Stream Client Tests (`stream/client_test.go`)

> **Testing Pattern**: Mock getstream.io API responses for unit tests.
> Integration tests can use real API with test credentials.

- [ ] 4.5.3.1 Test `CreateLoopActivity` - activity creation, "To" targets
- [ ] 4.5.3.2 Test `GetUserTimeline` / `GetGlobalFeed` - response parsing
- [ ] 4.5.3.3 Test `FollowUser` / `UnfollowUser` - feed follow operations
- [ ] 4.5.3.4 Test `AddReaction` / `RemoveReaction` - like/unlike
- [ ] 4.5.3.5 Test `GetNotifications` - notification parsing, unseen/unread
- [ ] 4.5.3.6 Test `MarkNotificationsRead` / `MarkNotificationsSeen`
- [ ] 4.5.3.7 Test `GetFollowStats` - follower/following counts
- [ ] 4.5.3.8 Test `GetAggregatedTimeline` - aggregation grouping
- [ ] 4.5.3.9 Test error handling - network errors, API errors

### 4.5.4 WebSocket Tests (`websocket/websocket_test.go`)

> Existing tests cover 9.4%. Extend coverage.

- [ ] 4.5.4.1 Test presence broadcast - online/offline notifications
- [ ] 4.5.4.2 Test message routing - handler registration and dispatch
- [ ] 4.5.4.3 Test rate limiting - token bucket behavior
- [ ] 4.5.4.4 Test reconnection handling - state cleanup

### 4.5.5 Storage Tests (`storage/s3_test.go`)

> Use mock S3 client or localstack for isolated tests.

- [ ] 4.5.5.1 Test `UploadAudio` - key generation, content type
- [ ] 4.5.5.2 Test `UploadWaveform` - SVG upload
- [ ] 4.5.5.3 Test `UploadProfilePicture` - extension validation
- [ ] 4.5.5.4 Test `DeleteFile` - deletion
- [ ] 4.5.5.5 Test URL generation - CDN base URL

### 4.5.6 Audio Queue Tests (`queue/audio_jobs_test.go`)

> Existing tests cover 14.2%. Extend coverage.

- [ ] 4.5.6.1 Test job submission and processing
- [ ] 4.5.6.2 Test FFmpeg normalization - command generation
- [ ] 4.5.6.3 Test waveform generation - peak extraction
- [ ] 4.5.6.4 Test error handling - FFmpeg failures
- [ ] 4.5.6.5 Test job status updates

### 4.5.7 Plugin Tests (Target: 80+ test cases)

> Currently: 42 test cases / 390 assertions

**NetworkClient Tests:**
- [ ] 4.5.7.1 Test `fetchTimeline` - JSON parsing, error handling
- [ ] 4.5.7.2 Test `fetchGlobalFeed` - pagination
- [ ] 4.5.7.3 Test `createPost` - post creation flow
- [ ] 4.5.7.4 Test `followUser` / `unfollowUser` - state changes
- [ ] 4.5.7.5 Test `likePost` / `unlikePost` - optimistic updates

**Component Tests:**
- [ ] 4.5.7.6 Test `PostCardComponent` - rendering, interactions
- [ ] 4.5.7.7 Test `ProfileComponent` - data display
- [ ] 4.5.7.8 Test `NotificationBellComponent` - badge updates

---

## Phase 5: Real-time Features
**Goal**: Live updates and presence
**Duration**: 2 weeks

### 5.1 WebSocket Infrastructure ✅

> **Testing**: Run `cd backend && go test ./internal/websocket/... -v` for WebSocket tests (16 tests).
> Tests cover: message types, hub, rate limiting, client lifecycle.
> Manual: Open plugin → Check connection indicator (green = connected).

- [x] 5.1.1 Choose WebSocket library (coder/websocket - modern alternative to gorilla/websocket)
- [x] 5.1.2 Create WebSocket server handler (internal/websocket/handler.go)
- [x] 5.1.3 Implement connection authentication (JWT in query param or Authorization header)
- [x] 5.1.4 Create connection manager (track active connections) - Hub struct in hub.go
- [x] 5.1.5 Implement heartbeat/ping-pong for connection health (60s timeout, 54s ping interval)
- [x] 5.1.6 Handle reconnection on disconnect (client cleanup, hub unregister)
- [x] 5.1.7 Implement message routing (by type) - RegisterHandler system
- [x] 5.1.8 Add rate limiting per connection (token bucket, 10/sec + 20 burst)
- [x] 5.1.9 Implement graceful shutdown (drain connections) - Hub.Shutdown with context
- [x] 5.1.10 Add WebSocket metrics (connections, messages/sec) - MetricsSnapshot struct
- [x] 5.1.11 Write unit tests for WebSocket infrastructure (websocket_test.go - 16 tests)

### 5.2 Plugin WebSocket Client ✅

> **Testing**: Manual testing (requires backend running).
> Test: Open plugin → Connection indicator turns green within 5 seconds.
> Test: Stop backend → Indicator turns red → Restart backend → Indicator turns green (auto-reconnect).
> Test: Check backend logs for WebSocket heartbeat messages.

- [x] 5.2.1 Implement WebSocket client in C++ (JUCE StreamingSocket with RFC 6455 framing) - WebSocketClient.h/cpp
- [x] 5.2.2 Connect on plugin load (after auth) - PluginEditor::connectWebSocket() called in onLoginSuccess/loadLoginState
- [x] 5.2.3 Implement reconnection with exponential backoff - scheduleReconnect() with 2^n delay, jitter, max delay cap
- [x] 5.2.4 Parse incoming JSON messages - processTextMessage() with juce::JSON::parse()
- [x] 5.2.5 Route messages to appropriate handlers - MessageType enum, parseMessageType(), handleWebSocketMessage()
- [x] 5.2.6 Send heartbeat messages - sendHeartbeat() at configurable interval + WebSocket ping frames
- [x] 5.2.7 Handle connection state UI (connected/disconnected) - handleWebSocketStateChange() updates ConnectionIndicator
- [x] 5.2.8 Queue messages when disconnected, send on reconnect - queueMessage()/flushMessageQueue() with max size limit

### 5.3 Presence System ⚠️ DEPRECATED - Replaced by getstream.io Chat Presence

> **Architecture Change**: The custom presence system (Phase 5.3) is being replaced with getstream.io Chat's built-in presence API (Phase 6.5.2.7). This provides:
> - Automatic online/offline tracking (no custom timeout logic needed)
> - Real-time presence updates via WebSocket
> - `last_active` timestamps automatically maintained
> - Custom status support (for "in studio" status)
> - Works app-wide, not just in messaging

> **Migration Plan**:
> - Phase 6.5.2.7 will implement getstream.io presence throughout the app
> - Custom PresenceManager can be removed after migration
> - All presence indicators will use getstream.io Chat presence data

- [x] 5.3.1 Track user online/offline status - PresenceManager with UserPresence struct, OnClientConnect/OnClientDisconnect (⚠️ Will be replaced)
- [x] 5.3.2 Track "in studio" status (plugin open in DAW) - StatusInStudio with DAW field, MessageTypeUserInStudio handler (⚠️ Will use getstream.io user.status field)
- [x] 5.3.3 Broadcast presence to followers - broadcastToFollowers() via getstream.io GetFollowers (⚠️ Will use getstream.io presence events)
- [x] 5.3.4 Show online indicator on avatars (green dot) - UserCardComponent::drawAvatar() with green/cyan dot (✅ Keep UI, change data source)
- [x] 5.3.5 Add "X friends in studio" indicator - GET /api/v1/ws/friends-in-studio endpoint (⚠️ Will query getstream.io presence)
- [x] 5.3.6 Implement presence persistence (Redis or in-memory) - In-memory map + database sync (is_online, last_active_at) (⚠️ getstream.io handles this)
- [x] 5.3.7 Handle presence timeout (5 minutes no heartbeat = offline) - runTimeoutChecker() with configurable duration (⚠️ getstream.io handles this automatically)
- [x] 5.3.8 Add DAW detection (show which DAW user is using) - DAW field in UserPresence, sent in presence payload (⚠️ Will use getstream.io user.extra_data)

### 5.4 Live Notifications (Stream.io Notification Feed)

> **Testing**: Manual testing with two accounts.
> Test: User B likes User A's post → User A sees bell badge increment → Click bell → Notification shows.
> Test: Click notification → Marked as read → Badge count decreases.
> Test: Click "Mark all as read" → All notifications marked read → Badge shows 0.

> **Architecture Decision**: Use getstream.io Notification Feeds instead of custom database storage.
>
> **Why getstream.io Notifications?**
> - Built-in `Unseen` and `Unread` counts (no custom tracking needed)
> - Automatic grouping: "5 people liked your loop" happens automatically
> - `IsSeen` / `IsRead` state per notification group
> - Same API patterns as other feeds (consistent code)
> - Scales automatically with getstream.io infrastructure
>
> **What we DON'T need to build:**
> - ~~Notification database table~~ → getstream.io stores it
> - ~~Unread count tracking~~ → `resp.Unseen` / `resp.Unread` provided
> - ~~Grouping logic~~ → Aggregation format handles it
> - ~~Mark as read/seen queries~~ → `WithNotificationsMarkRead()` / `WithNotificationsMarkSeen()`
>
> **Prerequisites**: Complete Phase 1.2.2 (Notification Feed backend implementation)

#### 5.4.1 Plugin Notification UI ✅

- [x] 5.4.1.1 Create `NotificationBellComponent` with animated badge (NotificationBellComponent.h/cpp)
- [x] 5.4.1.2 Implement badge drawing with unseen count (red circle, "99+" for overflow)
- [x] 5.4.1.3 Create `NotificationListComponent` for displaying grouped notifications (NotificationListComponent.h/cpp)
- [x] 5.4.1.4 Implement notification row rendering with read/unread styling (NotificationRowComponent)
- [x] 5.4.1.5 Add notification panel to main plugin UI (dropdown modal in PluginEditor.cpp)
- [x] 5.4.1.6 Implement notification polling (every 30 seconds when plugin is open)

#### 5.4.2 Mark as Read/Seen Integration ✅

- [x] 5.4.2.1 Mark as seen when notification panel is opened (clears badge) - PluginEditor::showNotificationPanel()
- [x] 5.4.2.2 Mark as read when notification is clicked - onNotificationClicked callback
- [x] 5.4.2.3 Add "Mark all as read" button - NotificationListComponent header

#### 5.4.3 Backend API Endpoints ✅

- [x] 5.4.3.1 Implement `GET /api/v1/notifications` handler (handlers.go:1091-1130)
- [x] 5.4.3.2 Implement `POST /api/v1/notifications/seen` handler (handlers.go:1164-1188)
- [x] 5.4.3.3 Implement `POST /api/v1/notifications/read` handler (handlers.go:1142-1162)
- [x] 5.4.3.4 Register notification routes in router (main.go:190-198)

#### 5.4.4 Optional Enhancements

- [ ] 5.4.4.1 Implement notification sound option (user preference)
- [ ] 5.4.4.2 Add notification preferences (mute specific types)
- [ ] 5.4.4.3 Consider WebSocket push for real-time updates (polling is sufficient for MVP)

### 5.5 Real-time Feed Updates ✅

- [x] 5.5.1 Push new posts to followers via WebSocket - notifyNewPostToFollowers() called after post processing completes, sends NewPostPayload to all followers via WebSocket
- [x] 5.5.2 Show "new posts" toast in feed - handleNewPostNotification() increments pendingNewPostsCount, shows toast if user scrolled down, auto-refreshes if at top. drawNewPostsToast() renders clickable toast
- [x] 5.5.3 Update like counts in real-time - notifyLikeCountUpdate() broadcasts LikeCountUpdate message, handleLikeCountUpdate() updates PostCardComponent like count
- [x] 5.5.4 Update follower counts in real-time - notifyFollowerCountUpdate() sends FollowerCountUpdate after follow/unfollow, handleFollowerCountUpdate() updates profile (placeholder for now)
- [x] 5.5.5 Implement optimistic updates (instant UI, confirm later) - onLikeToggled updates UI immediately, then calls API with callback to confirm/revert
- [x] 5.5.6 Handle conflicts (stale data resolution) - Optimistic update callbacks check server response, revert on failure. WebSocket updates override optimistic updates if they differ

---

## Phase 6: Comments & Community
**Goal**: Enable conversation around posts
**Duration**: 1.5 weeks

### 6.1 Comment System Backend

> **Testing**: Run `cd backend && go test ./internal/handlers/... -run Comment -v` for comment handler tests.
> Tests exist in `handlers/comments_test.go` (20+ tests).
> API: `POST /api/v1/posts/:id/comments` to create, `GET /api/v1/posts/:id/comments` to list.

- [x] 6.1.1 Create Comment model (id, post_id, user_id, content, created_at)
- [x] 6.1.2 Add parent_id for threaded replies
- [x] 6.1.3 Create POST /posts/{id}/comments endpoint
- [x] 6.1.4 Create GET /posts/{id}/comments endpoint (paginated)
- [x] 6.1.5 Implement comment editing (within 5 minutes)
- [x] 6.1.6 Implement comment deletion
- [x] 6.1.7 Add comment likes
- [x] 6.1.8 Implement @mentions with notification
- [x] 6.1.9 Add comment count to post response
- [x] 6.1.10 Index comments for search

### 6.2 Comment UI ✅

> **Testing**: Manual testing in plugin (when implemented).
> Test: Click comment icon on post → Comment section expands → Type comment → Submit → Comment appears.
> Test: Click reply → Reply field appears → Submit reply → Threaded under parent.

- [x] 6.2.1 Design comment section (expandable from post) - CommentsPanelComponent slides in from side
- [x] 6.2.2 Implement comment list rendering - CommentRowComponent renders each comment with infinite scroll
- [x] 6.2.3 Add comment input field - TextEditor with TextEditorStyler, multiline support
- [x] 6.2.4 Show user avatar and username per comment - drawAvatar() and drawUserInfo() implemented
- [x] 6.2.5 Display relative timestamps ("2h ago") - Uses TimeUtils::formatTimeAgoShort()
- [x] 6.2.6 Implement reply threading (1 level deep) - parentId handling, REPLY_INDENT, setIsReply()
- [x] 6.2.7 Add comment actions menu (edit, delete, report) - PopupMenu with edit/delete for own comments, report for others
- [x] 6.2.8 Implement @mention autocomplete - Real-time mention detection, user search, keyboard/mouse selection
- [x] 6.2.9 Add emoji picker for comments - Emoji button using EmojiReactionsBubble
- [ ] 6.2.10 Show "typing" indicator (future) - Deferred to future phase

### 6.3 Content Moderation ✅

> **Testing**: Manual testing (sensitive feature).
> Test: Click "..." menu on post → Click "Report" → Select reason → Submit → Verify backend receives report.
> Test: Block user → Verify their posts no longer appear in your feed.

- [x] 6.3.1 Implement post reporting endpoint - POST /api/v1/posts/:id/report
- [x] 6.3.2 Implement comment reporting endpoint - POST /api/v1/comments/:id/report
- [x] 6.3.3 Implement user reporting endpoint - POST /api/v1/users/:id/report
- [x] 6.3.4 Create Report model (reporter, target, reason, status) - models.Report with ReportReason, ReportStatus, ReportTargetType enums
- [x] 6.3.5 Add report reasons enum (spam, harassment, copyright, etc.) - ReportReason enum with 6 reasons
- [x] 6.3.6 Implement user blocking - POST /api/v1/users/:id/block, DELETE /api/v1/users/:id/block, GET /api/v1/users/me/blocks
- [x] 6.3.7 Hide content from blocked users - Filters applied to GetTimeline, GetGlobalFeed, GetComments, GetUserPosts
- [x] 6.3.8 Add profanity filter (basic word list) - containsProfanity() checks comments and post titles
- [x] 6.3.9 Create moderation admin endpoint - GET /api/v1/moderation/reports, GET /api/v1/moderation/reports/:id, PUT /api/v1/moderation/reports/:id (basic implementation, needs admin auth in production)
- [x] 6.3.10 Implement content takedown flow - POST /api/v1/moderation/takedown (basic implementation, needs admin auth in production)

---

## Phase 6.5: Direct Messaging (getstream.io Chat)
**Goal**: Enable private 1-on-1 and group messaging between producers
**Duration**: 2 weeks
**Prerequisites**: Phase 6 (Comments) complete, getstream.io Chat client initialized

> **Architecture Decision**: The C++ plugin will connect **directly** to getstream.io Chat API (REST + WebSocket), not through our backend. This is simpler, more scalable, and follows getstream.io's recommended pattern.
>
> **Backend Role**: Only generates user tokens (one endpoint). All messaging operations happen directly between plugin and getstream.io.
>
> **Plugin Role**:
> - Uses REST API for operations (create channels, send messages, query, etc.)
> - Uses WebSocket for real-time updates (new messages, typing indicators, read receipts)
> - All communication is plugin ↔ getstream.io (no backend proxy needed)
>
> **Presence Integration**: getstream.io Chat's presence API will be used **app-wide** (not just for messaging). This replaces the custom presence system (Phase 5.3) with getstream.io's built-in presence tracking. See section 6.5.2.7 for details.

> **Testing**:
> - Backend: `cd backend && go test ./internal/stream/... -run Token -v`
> - Manual: Open plugin → Click message icon → Start conversation → Send message → Verify real-time delivery
> - Integration: Test with two plugin instances → Verify messages appear instantly via WebSocket

### 6.5.1 Backend Token Generation

> **Only Backend Responsibility**: Generate getstream.io user tokens securely (API secret must stay server-side)

> **Architecture**: The plugin connects directly to getstream.io Chat API. The backend only generates authentication tokens. All messaging operations (create channels, send messages, etc.) happen directly between the plugin and getstream.io.

- [x] 6.5.1.1 Create `GET /api/v1/auth/stream-token` endpoint
  - Returns: `{ "token": "jwt_token", "api_key": "stream_api_key", "user_id": "user_id" }`
  - Implementation: Use `ChatClient.CreateToken(userID, expiration)` from getstream.io Go SDK
  - Token expires in 24 hours (renewable)
  - API key is safe to expose (only secret must stay server-side)
  - Location: Added to `backend/internal/handlers/auth.go` - `GetStreamToken()` handler
  - Route: Registered in `backend/cmd/server/main.go` under auth routes

- [x] 6.5.1.2 Ensure users are created in getstream.io Chat on registration
  - Already implemented in `stream.CreateUser()` - called during user registration
  - Verify users exist before generating tokens - `GetStreamToken()` calls `CreateUser()` if needed
  - If user doesn't exist in getstream.io, create them before generating token

### 6.5.2 Plugin getstream.io Chat Integration

> **getstream.io Chat Features to Leverage**:
> - Direct messages (1-on-1 channels)
> - Group channels (for collaboration)
> - Message reactions (emoji)
> - Typing indicators (via WebSocket)
> - Read receipts
> - Message threading
> - File attachments (audio snippets)
> - URL enrichment (preview links)
> - Message search
> - Real-time WebSocket updates

#### 6.5.2.1 getstream.io Chat Client Setup

- [x] 6.5.2.1.1 Create `StreamChatClient` class (StreamChatClient.h/cpp)
  - Stores: API key, user token, user ID
  - Manages REST API calls to `https://chat.stream-io-api.com`
  - Manages WebSocket connection to getstream.io (structure in place, needs full implementation)
  - Handles authentication headers: `Stream-Auth-Type: jwt`, `Authorization: {token}`
  - Added to CMakeLists.txt build system

- [x] 6.5.2.1.2 Implement token fetching from backend
  - Call `GET /api/v1/auth/stream-token` after user login
  - Store token, API key, and user ID
  - Handle token expiration and renewal (structure in place)

- [~] 6.5.2.1.3 Implement WebSocket connection to getstream.io
  - ⚠️ **BLOCKED**: ASIO/websocketpp incompatible with C++23 (`std::result_of` removed)
  - **WORKAROUND IMPLEMENTED**: Polling-based real-time updates (see 6.5.2.4.1)
  - WebSocket URL built: `wss://chat.stream-io-api.com/?api_key={key}&authorization={token}&user_id={userId}`
  - Event parsing implemented: `handleWebSocketMessage()`, `parseWebSocketEvent()`
  - Ready to enable once ASIO issues are resolved

#### 6.5.2.2 Channel Management (REST API)

- [x] 6.5.2.2.1 Implement create direct message channel
  - `POST https://chat.stream-io-api.com/channels/messaging/{channel_id}?api_key={key}`
  - Headers: `Stream-Auth-Type: jwt`, `Authorization: {token}`
  - Body: `{ "members": [user_id, target_user_id] }`
  - Channel ID format: sorted user IDs (e.g., `user1_user2` if user1 < user2)
  - Implemented: `createDirectChannel()` with `generateDirectChannelId()` helper

- [x] 6.5.2.2.2 Implement create group channel
  - `POST https://chat.stream-io-api.com/channels/team/{channel_id}?api_key={key}`
  - Body: `{ "members": [...], "data": { "name": "Group Name" } }`
  - Implemented: `createGroupChannel()` method

- [x] 6.5.2.2.3 Implement query channels (list user's channels)
  - `GET https://chat.stream-io-api.com/channels?api_key={key}&filter={filter}&sort={sort}`
  - Filter: `{"members": {"$in": [user_id]}}`
  - Sort: `[{"field": "last_message_at", "direction": -1}]`
  - Returns: List of channels with last message, unread count, members
  - Implemented: `queryChannels()` with pagination support

- [x] 6.5.2.2.4 Implement get channel details
  - `GET https://chat.stream-io-api.com/channels/{channel_type}/{channel_id}?api_key={key}`
  - Returns: Full channel object with members, metadata
  - Implemented: `getChannel()` method

- [x] 6.5.2.2.5 Implement delete/leave channel
  - `DELETE https://chat.stream-io-api.com/channels/{channel_type}/{channel_id}?api_key={key}` - Delete
  - `POST https://chat.stream-io-api.com/channels/{channel_type}/{channel_id}/remove_members?api_key={key}` - Leave
  - Implemented: `deleteChannel()` and `leaveChannel()` methods

#### 6.5.2.3 Message Operations (REST API)

- [x] 6.5.2.3.1 Implement send message
  - `POST https://chat.stream-io-api.com/channels/{channel_type}/{channel_id}/message?api_key={key}`
  - Body: `{ "message": { "text": "...", "extra_data": { "audio_url": "...", "reply_to": "..." } } }`
  - Support text, audio snippets, and threaded replies
  - Implemented: `sendMessage()` with extraData support

- [x] 6.5.2.3.2 Implement query messages (get channel messages)
  - `GET https://chat.stream-io-api.com/channels/{channel_type}/{channel_id}/query?api_key={key}`
  - Query params: `messages.limit`, `messages.offset`
  - Returns: Channel state with messages array
  - Implemented: `queryMessages()` with pagination support

- [x] 6.5.2.3.3 Implement update message
  - `POST https://chat.stream-io-api.com/channels/{channel_type}/{channel_id}/message?api_key={key}`
  - Body: `{ "message": { "id": "message_id", "text": "updated text" } }`
  - Only allow if message is < 5 minutes old (enforced by getstream.io)
  - Implemented: `updateMessage()` method

- [x] 6.5.2.3.4 Implement delete message
  - `DELETE https://chat.stream-io-api.com/channels/{channel_type}/{channel_id}/message/{message_id}?api_key={key}`
  - Soft delete (marks as deleted)
  - Implemented: `deleteMessage()` method

- [x] 6.5.2.3.5 Implement message reactions
  - `POST https://chat.stream-io-api.com/channels/{channel_type}/{channel_id}/message/{message_id}/reaction?api_key={key}`
  - Body: `{ "reaction": { "type": "👍" } }`
  - `DELETE` to remove reaction
  - Implemented: `addReaction()` and `removeReaction()` methods

#### 6.5.2.4 Real-time Updates (Polling-based until WebSocket fixed)

> **Note**: True WebSocket implementation blocked by ASIO/C++23 compatibility issues.
> Polling-based workaround provides ~2 second latency for message updates.

- [x] 6.5.2.4.1 Subscribe to channel events (POLLING WORKAROUND)
  - Implemented: `watchChannel()` / `unwatchChannel()` methods
  - Polls every 2 seconds when viewing a channel
  - Detects new messages by comparing `lastSeenMessageId`
  - Triggers `messageReceivedCallback` for new messages from other users
  - `pollWatchedChannel()` handles message detection
  - Event parsing ready for WebSocket: `parseWebSocketEvent()` handles `message.new`, `typing.*`, `user.presence.changed`

- [x] 6.5.2.4.2 Implement typing indicators
  - Send typing events via REST API: `POST /channels/{type}/{id}/event`
  - `sendTypingIndicator()` method implemented
  - Auto-send `typing.stop` after 3 seconds idle (via timer in MessageThreadComponent)
  - MessageThreadComponent tracks `isTyping` and `lastTypingTime`

- [x] 6.5.2.4.3 Implement read receipts (REST API)
  - `POST https://chat.stream-io-api.com/channels/{type}/{id}/read?api_key={key}`
  - Mark channel as read when user views messages
  - Implemented: `markChannelRead()` method
  - Called automatically when loading messages in MessageThreadComponent

#### 6.5.2.5 Audio Snippet Sharing

- [x] 6.5.2.5.1 Upload audio snippet to backend CDN
  - Use existing `POST /api/v1/audio/upload` endpoint (or create dedicated snippet endpoint)
  - Validate duration (max 30 seconds)
  - Returns: `{ "audio_url": "cdn_url", "duration": 15.5 }`
  - Implemented: `uploadAudioSnippet()` method with 30s validation and WAV encoding

- [x] 6.5.2.5.2 Attach audio to message
  - Include `audio_url` and `audio_duration` in message `extra_data` field
  - Send via REST API message endpoint
  - Implemented: `sendMessageWithAudio()` method that uploads snippet then sends message with audio in extra_data

#### 6.5.2.6 Message Search

- [x] 6.5.2.6.1 Implement message search (if getstream.io search API available)
  - Use getstream.io search endpoint or query channels and filter client-side
  - Search within specific channel or across all channels
  - Implemented: `searchMessages()` method using getstream.io `/search` endpoint
  - Supports channel filters, query text, pagination (limit/offset)

#### 6.5.2.7 Presence Integration (App-Wide)

> **Architecture Decision**: Use getstream.io Chat's presence API throughout the entire app, not just for messaging. This replaces the custom WebSocket presence system (Phase 5.3) with getstream.io's built-in presence.
>
> **Benefits**:
> - Automatic online/offline tracking (no custom timeout logic)
> - Real-time presence updates via WebSocket
> - `last_active` timestamps automatically maintained
> - Custom status support (for "in studio" status)
> - Works everywhere: profiles, feeds, user lists, messaging

- [x] 6.5.2.7.1 Query user presence from getstream.io Chat REST API
  - `GET https://chat.stream-io-api.com/users?api_key={key}&filter={filter}&presence=true`
  - Filter: `{"id": {"$in": [user_ids]}}`
  - Returns: Users with `online` (boolean), `last_active` (timestamp), and `status` (custom string) fields
  - Use this for:
    - User profiles: Show online/offline status badge
    - Feed posts: Show if post author is online (green dot on avatar)
    - User lists: Show online indicators in discovery/search
    - "Friends in studio" feature: Query online users with status="in studio"
  - Implemented: `queryPresence()` method with parsing helpers

- [ ] 6.5.2.7.2 Subscribe to presence changes via getstream.io WebSocket
  - Listen for `user.presence.changed` events from getstream.io Chat WebSocket
  - Event format: `{"type": "user.presence.changed", "user": {"id": "...", "online": true, "last_active": "...", "status": "..."}}`
  - Update presence indicators in real-time throughout the app:
    - Update avatar online dots immediately
    - Update profile online status
    - Update feed post author status
    - Update "friends in studio" count

- [ ] 6.5.2.7.3 Update plugin to use getstream.io presence everywhere
  - Replace custom presence queries with getstream.io Chat presence API
  - Update `UserCardComponent` to query/getstream.io presence for online status
  - Update `PostCardComponent` to show author online status (query presence for post author)
  - Update `ProfileComponent` to show online status badge
  - Update feed components to show online indicators on post avatars

- [x] 6.5.2.7.4 Implement "in studio" custom status (REST API)
  - Use getstream.io user `status` field for custom status (e.g., "in studio", "recording", "mixing")
  - Update status when user starts recording: `upsertUser({id: user_id, status: "in studio", extra_data: {daw: "Ableton"}})`
  - Update status when user stops recording: `upsertUser({id: user_id, status: ""})` (empty = no custom status)
  - Implemented: `updateStatus()` method
  - Query users with status for "friends in studio" feature:
    - `GET /users?filter={"status": {"$ne": ""}, "online": true}&presence=true`
  - ⚠️ Integration with recording start/stop still needs to be wired up in plugin

- [ ] 6.5.2.7.5 Migrate "friends in studio" feature
  - Replace `GET /api/v1/ws/friends-in-studio` endpoint
  - Query getstream.io Chat for followed users who are online with custom status
  - Use getstream.io `queryUsers` with filter: `{"id": {"$in": [followed_user_ids]}, "online": true, "status": {"$ne": ""}}`
  - Return count and list of users in studio

- [ ] 6.5.2.7.6 Remove custom presence system (after migration)
  - Remove `PresenceManager` from backend (websocket/presence.go)
  - Remove custom presence WebSocket messages (`MessageTypePresence`, `MessageTypeUserOnline`, etc.)
  - Remove presence endpoints (`/api/v1/ws/presence`, `/api/v1/ws/friends-in-studio`)
  - Keep WebSocket infrastructure for other real-time features (feed updates, notifications)

- [ ] 6.5.2.7.7 Update presence display throughout UI
  - Profile pages: Show online/offline badge next to username
  - Feed posts: Show green dot on post author avatar if online
  - User lists: Show online indicators in search/discovery results
  - Message threads: Show online status in header (already part of messaging UI)
  - "Friends in studio" indicator: Update to use getstream.io presence query

### 6.5.3 Plugin Messaging UI

> **UI Components Needed**:
> - `MessagesListComponent` - List of conversations (channels)
> - `MessageThreadComponent` - Individual conversation view
> - `MessageBubbleComponent` - Individual message rendering
> - `MessageInputComponent` - Text input with send button
> - `AudioSnippetRecorder` - Record short audio clips for messages

#### 6.5.3.1 Messages List View

- [x] 6.5.3.1.1 Create `MessagesListComponent` (MessagesListComponent.h/cpp)
  - Display list of channels/conversations
  - Show: Avatar, name, last message preview, timestamp, unread badge
  - Sort by last message time (most recent first)
  - Click to open conversation
  - Use `StreamChatClient.queryChannels()` to fetch channel list
  - Implemented: Full component with scrolling, auto-refresh, empty/error states

- [x] 6.5.3.1.2 Add "New Message" button
  - Opens user search dialog (currently navigates to Discovery)
  - Select user → Creates/opens channel via `StreamChatClient.createChannel()`
  - Navigate to message thread
  - Implemented: Button in header area, callback wired to Discovery view

- [x] 6.5.3.1.3 Implement channel list updates
  - Poll REST API every 10 seconds (implemented via Timer)
  - ⚠️ WebSocket events for real-time updates still need implementation (6.5.2.4.1)
  - Update unread counts on each poll

- [x] 6.5.3.1.4 Add empty state
  - Show "No messages yet" with "Start a conversation" text
  - Link to user discovery via `onGoToDiscovery` callback
  - Implemented: `drawEmptyState()` method

#### 6.5.3.2 Message Thread View

- [x] 6.5.3.2.1 Create `MessageThreadComponent` (MessageThreadComponent.h/cpp)
  - Header: Back button, channel name (online status pending WebSocket)
  - Message list: Scrollable list of messages (newest at bottom)
  - Input area: Text field + send button (audio button pending)
  - Load messages via `StreamChatClient.queryMessages()`
  - Implemented: Full component with scrolling, auto-refresh, input handling

- [x] 6.5.3.2.2 Implement message rendering (integrated into `MessageThreadComponent`)
  - Draw sender name for received messages
  - Render message text with word wrap (using JUCE TextLayout)
  - Show timestamp ("2h ago" format)
  - Different styling for own messages (right-aligned, primary color) vs received (left-aligned, gray)
  - ⚠️ Read receipts (checkmarks) need WebSocket implementation
  - ⚠️ Audio snippet waveform playback pending

- [x] 6.5.3.2.3 Implement message input (integrated into `MessageThreadComponent`)
  - Single-line text editor (JUCE TextEditor with placeholder)
  - Send button (Enter to send via `textEditorReturnKeyPressed`)
  - ⚠️ Audio record button pending (6.5.3.4)
  - Send via `StreamChatClient.sendMessage()`

- [x] 6.5.3.2.4 Add message actions menu
  - Right-click or long-press on message
  - Options: Copy, Edit (own messages only), Delete, Reply
  - Show popup menu with actions
  - Edit/Delete via REST API calls
  - Implemented: Right-click detection, PopupMenu with actions, edit/delete/reply functionality
  - Edit mode loads message text into input field, reply sets reply_to in extraData

#### 6.5.3.3 Real-time Message Updates (IMPLEMENTED via Polling)

- [x] 6.5.3.3.1 Subscribe to message events (POLLING WORKAROUND)
  - StreamChatClient `watchChannel()` polls every 2 seconds
  - `messageReceivedCallback` triggered for new messages
  - MessageThreadComponent adds new messages to list
  - Auto-scroll to bottom when new message received
  - ⚠️ Notification sound pending (user preference system needed)

- [x] 6.5.3.3.2 Implement typing indicators
  - MessageThreadComponent sends typing via `sendTypingIndicator()`
  - `textEditorTextChanged()` triggers `typing.start`
  - Timer auto-sends `typing.stop` after 3 seconds idle
  - `typingUserName` variable ready for display
  - Visual "User is typing..." display implemented in `drawInputArea()` above input field

- [x] 6.5.3.3.3 Update read receipts
  - `markChannelRead()` called when loading messages
  - Channels marked as read when user views them
  - ⚠️ Visual read receipt checkmarks pending

#### 6.5.3.4 Audio Snippet Recording

- [x] 6.5.3.4.1 Create `AudioSnippetRecorder` component
  - Record button (toggle to record, shows waveform while recording)
  - Max duration: 30 seconds
  - Show timer during recording
  - Cancel button to discard recording
  - Integrated into MessageThreadComponent with audio button in input area

- [ ] 6.5.3.4.2 Implement audio snippet playback in messages
  - Waveform visualization (use existing waveform rendering)
  - Play/pause button
  - Progress indicator
  - Duration display

- [ ] 6.5.3.4.3 Upload audio snippet when sending
  - Upload to backend CDN via `POST /api/v1/audio/upload` (or dedicated endpoint)
  - Attach `audio_url` to message via `extra_data` field
  - Show upload progress indicator

#### 6.5.3.5 Message Threading & Replies

- [x] 6.5.3.5.1 Implement reply UI
  - Click "Reply" on message → Shows quoted message above input
  - Visual reply preview with quoted message text and sender name
  - Cancel reply button (X) to clear reply state
  - Send reply with `reply_to` field set in `extra_data` (getstream.io threading)
  - Reply preview automatically cleared after sending
  - Layout adjusts to show/hide reply preview above input area

- [x] 6.5.3.5.2 Render threaded messages
  - Show parent message preview in reply (sender name + truncated text)
  - Indent reply messages by 20px to show threading
  - Click parent preview to scroll to original message
  - Parent preview shows with accent border and divider line
  - Height calculations account for parent preview area

#### 6.5.3.6 Navigation Integration (COMPLETED)

> **Status**: ✅ RESOLVED - Messages UI is now accessible via header icon

- [x] 6.5.3.6.1 Add messages icon to header
  - Show unread badge (total unread count across all channels) - implemented in HeaderComponent
  - Click to open messages list via `onMessagesClicked` callback
  - Badge updates via `setUnreadMessageCount()` method
  - Implemented: `drawMessagesButton()`, `getMessagesButtonBounds()` in HeaderComponent

- [x] 6.5.3.6.2 Instantiate MessagesListComponent in PluginEditor
  - Component wired up in PluginEditor constructor
  - Added to PluginEditor view switching logic (showView, navigateBack)
  - Initialize with StreamChatClient and NetworkClient references
  - Added Messages and MessageThread to AppView enum

- [x] 6.5.3.6.3 Wire up navigation to messages view
  - Connect header message icon click to showView(AppView::Messages)
  - Messages view accessible from all screens via header icon
  - MessageThread navigation via `showMessageThread()` and `onChannelSelected` callback

- [x] 6.5.3.6.4 Add "Message" button to user profiles
  - Click "Message" on profile → Creates/opens channel via REST API
  - Navigate to message thread view
  - Implemented: Button already exists in ProfileComponent, wired up via `onMessageClicked` callback in PluginEditor
  - Creates direct channel via `createDirectChannel()` and navigates to message thread

### 6.5.4 Group Messaging

- [~] 6.5.4.1 Create group channel UI
  - "Create Group" button in messages list (implemented, shows when space allows)
  - Button currently navigates to Discovery view for user selection
  - ⚠️ Full group creation dialog with user picker and name input pending (requires user search component)
  - Create via `StreamChatClient.createGroupChannel()` ready when dialog is complete

- [x] 6.5.4.2 Display group channels
  - Show group avatar (first letter of name) with blue background
  - Show member count ("X members" below channel name)
  - Show last message from any member (same as DMs)
  - Distinguishes group channels from direct messages visually

- [~] 6.5.4.3 Group management
  - Leave group option (implemented via header menu button for group channels)
  - "Leave Group" menu option in message thread header (three-dot menu)
  - Navigates back to messages list after leaving
  - Rename group (implemented - uses AlertWindow for name input)
  - StreamChatClient methods added: `addMembers()`, `removeMembers()`, `updateChannel()`
  - Header menu expanded with "Add Members", "Remove Members", "Rename Group" options
  - ⚠️ Add/remove members UI pending (requires user picker component for full implementation)

### 6.5.5 Message Moderation

- [x] 6.5.5.1 Implement message reporting
  - Report user via backend endpoint `POST /api/v1/users/:id/report`
  - Popup menu with report reasons: spam, harassment, inappropriate, other
  - Includes message text in report description for context
  - Integrated into message actions menu (right-click on message)

- [x] 6.5.5.2 Add block user in messages
  - Block user via backend endpoint `POST /api/v1/users/:id/block`
  - Option to block from message actions menu
  - Blocked users' messages are removed from view immediately
  - Messages list is reloaded after blocking to reflect changes

### 6.5.6 Testing & Integration

- [~] 6.5.6.1 Write backend tests for token generation
  - ✅ Stream client tests exist in `backend/tests/integration/stream_integration_test.go`
  - ✅ `TestStreamTokenCreation` tests JWT token generation
  - ✅ `TestStreamUserCreation` tests user creation in getstream.io
  - ⚠️ HTTP endpoint test for `GET /api/v1/auth/stream-token` pending (handler exists but needs endpoint test)
  - ⚠️ Test token expiration with 24-hour expiry pending

- [ ] 6.5.6.2 Write plugin unit tests
  - Test `StreamChatClient` REST API calls
  - Test `StreamChatClient` WebSocket connection
  - Test `MessageThreadComponent` rendering
  - Test message input and sending
  - Test audio snippet recording

- [ ] 6.5.6.3 Integration testing
  - Test with two plugin instances
  - Verify real-time message delivery via WebSocket
  - Verify typing indicators
  - Verify read receipts
  - Test group messaging with 3+ users
  - Test token renewal on expiration

---

## Phase 7: Search & Discovery
**Goal**: Help users find great content
**Duration**: 1.5 weeks

### 7.1 Search Infrastructure ✅

> **Testing**: API tests via curl or Postman.
> Test: `curl "http://localhost:8787/api/v1/search/users?q=beat" -H "Authorization: Bearer $TOKEN"`
> Test: `curl "http://localhost:8787/api/v1/search/posts?q=electronic&bpm_min=120&bpm_max=130"`
> Manual: Use search bar in plugin → Verify results filter correctly.

- [x] 7.1.1 Evaluate search options (PostgreSQL full-text vs Elasticsearch) - Chose Elasticsearch for better scalability, advanced filtering, and search features. Added to docker-compose.yml
- [x] 7.1.2 Implement basic search endpoint - GET /api/v1/search/users and GET /api/v1/search/posts with Elasticsearch client wrapper, fallback to PostgreSQL if ES unavailable
- [x] 7.1.3 Search users by username - SearchUsers endpoint uses Elasticsearch with fuzzy matching, boosts username (2.0x), display_name (1.5x), bio (0.5x), sorted by score + follower_count
- [x] 7.1.4 Search posts by title/description - SearchPosts endpoint searches title (2.0x boost) and description (1.0x boost) with fuzzy matching, supports empty query for filter-only searches
- [x] 7.1.5 Add genre filtering - Genre filter implemented in SearchPosts with Elasticsearch terms query (supports multiple genres via comma-separated)
- [x] 7.1.6 Add BPM range filtering - BPM min/max range filter with Elasticsearch range query (bpm_min and bpm_max query params)
- [x] 7.1.7 Add key filtering - Key filter with exact term match (case-sensitive, supports all 24 keys)
- [x] 7.1.8 Implement search result ranking - Function score query combines text relevance with engagement (likes 3.0x, comments 2.0x, plays 1.0x, all log-scaled) and recency (30-day exponential decay, 0.5x weight)
- [x] 7.1.9 Add search analytics (track queries) - SearchQuery model stores queries in database with user_id, query, result_type, result_count, filters (JSON). trackSearchQuery() called from SearchUsers and SearchPosts
- [x] 7.1.10 Implement search suggestions (autocomplete) - GET /api/v1/search/suggestions uses Elasticsearch completion suggester on username.suggest field, returns top 5-10 suggestions

### 7.2 Discovery Features ✅

> **Testing**: API tests for discovery endpoints.
> Test: `curl "http://localhost:8787/api/v1/discover/posts/trending?period=day"` → Returns posts sorted by engagement.
> Test: `curl "http://localhost:8787/api/v1/discover/genre/electronic/posts"` → Returns only electronic posts.
> Manual: Navigate to Discover section in plugin → Verify trending and genre feeds populate.

- [x] 7.2.1 Implement trending posts algorithm
  - [x] 7.2.1.1 Factor in plays, likes, comments. Factor in genre popularity - Engagement score: likes*3 + comments*2 + plays*1. Genre boost: +10% for genres with >100 posts in period
  - [x] 7.2.1.2 Apply time decay (recent posts weighted higher) - Exponential decay based on age (hour/day/week/month periods with different decay scales)
  - [x] 7.2.1.3 Consider velocity (rapid engagement) - Velocity boost up to 3x if >20% of plays happened in last hour
- [x] 7.2.2 Create trending endpoint (hourly/daily/weekly) - GET /api/v1/discover/posts/trending with period param (hour/day/week/month)
- [x] 7.2.3 Implement genre-based feeds (electronic, hip-hop, etc.) - GET /api/v1/discover/genre/:genre/posts filters posts by genre array
- [x] 7.2.4 Add "For You" personalized feed
  - [x] 7.2.4.1 Track user preferences (listened genres, BPMs) - UserPreference model stores genre weights, BPM range, key preferences. Updated from PlayHistory (30-day window, weighted by duration and completion)
  - [x] 7.2.4.2 Recommend similar content - GET /api/v1/discover/for-you scores posts by genre/BPM/key match + engagement
- [x] 7.2.5 Implement hashtag system - Hashtag and PostHashtag models. createOrUpdateHashtag() helper function
- [x] 7.2.6 Add hashtag search and pages - GET /api/v1/discover/hashtags/search, GET /api/v1/discover/hashtags/trending, GET /api/v1/discover/hashtags/:hashtag/posts
- [x] 7.2.7 Create "New & Notable" curated section - GET /api/v1/discover/new-notable returns posts from last 7 days with >10 total engagement, sorted by engagement score
- [x] 7.2.8 Implement "Similar to this" recommendations - GET /api/v1/posts/:id/similar matches by genre (overlap), BPM (±10), key (exact), sorted by engagement

### 7.3 Search UI ✅

> **Testing**: Manual testing in plugin.
> Test: Click search icon → Search screen appears → Type query → Results update as you type.
> Test: Apply filters (genre, BPM range) → Results filtered correctly.
> Test: Recent searches persist after closing/reopening plugin.

- [x] 7.3.1 Add search icon to navigation - HeaderComponent has search button, connected to show SearchComponent view
- [x] 7.3.2 Create search screen with input field - SearchComponent with TextEditor input, 300ms debounce, real-time search
- [x] 7.3.3 Show search results (users + posts tabs) - Tabbed interface with Users/Posts tabs, shows UserCardComponent and PostCardComponent
- [x] 7.3.4 Add filter controls (genre, BPM, key) - Filter buttons with PopupMenu dropdowns for genre (12 options), BPM ranges (8 presets), and key (24 musical keys)
- [x] 7.3.5 Implement recent searches - Persisted to ~/.local/share/Sidechain/recent_searches.txt, loads on startup, max 10 searches
- [x] 7.3.6 Add trending searches section - Shows trending searches in empty state (currently placeholder, UI ready for backend integration)
- [x] 7.3.7 Show "no results" state with suggestions - Draws "No results found" with suggestions like "Try different keyword", "Remove filters", "Check spelling"
- [x] 7.3.8 Implement keyboard navigation - Tab to switch tabs, Up/Down arrows to navigate results with highlighting, Enter to select, Escape to go back

---

## Phase 7.5: Music Stories (Snapchat-style with MIDI Visualization)
**Goal**: Enable producers to share short music clips (stories) with MIDI visualization that expire after 24 hours
**Duration**: 2.5 weeks
**Prerequisites**: Phase 2 (Audio Capture) complete, MIDI capture capability

> **Concept**: Stories are short music clips (15-60 seconds) captured directly from the DAW with MIDI data. Viewers can see a visual representation of what the producer was playing (piano roll, note visualization). Stories auto-expire after 24 hours like Snapchat.

> **Progress Summary** (Updated):
> - ✅ Backend: Complete (database, endpoints, cleanup job)
> - ✅ MIDI Capture: Complete (normalization, validation)
> - ✅ Core UI Components: Complete (recording, feed, viewer, piano roll)
> - ✅ Integration: Complete (upload wired up in PluginEditor)
> - ✅ Preview: Complete (waveform, MIDI visualization, playback controls)
> - ✅ Metadata: Complete (BPM auto-detect, key/genre display)
> - ✅ Expired Stories: Complete (filtering and error messages)
> - ✅ Interactions: Complete (piano roll click/hover/scroll, viewers button, share button)
> - ✅ Visualization Options: Complete (zoom, scroll, velocity/channel toggles)
> - ⚠️ Testing: Not started

> **Testing**:
> - Backend: `cd backend && go test ./internal/handlers/... -run Stories -v`
> - Manual: Record story in plugin → View in feed → Verify MIDI visualization → Wait 24 hours → Verify expiration
> - MIDI: Test with various DAWs → Verify MIDI capture works correctly

### 7.5.1 Story Data Model & Backend

#### 7.5.1.1 Database Schema

- [x] 7.5.1.1.1 Create `stories` table
  ```sql
  CREATE TABLE stories (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    audio_url TEXT NOT NULL,
    audio_duration REAL NOT NULL, -- seconds
    midi_data JSONB, -- MIDI note events with timing
    waveform_data TEXT, -- SVG waveform
    bpm INTEGER,
    key TEXT,
    genre TEXT[],
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMP NOT NULL, -- created_at + 24 hours
    view_count INTEGER DEFAULT 0,
    CONSTRAINT valid_duration CHECK (audio_duration >= 5 AND audio_duration <= 60)
  );
  ```

- [x] 7.5.1.1.2 Create `story_views` table (track who viewed)
  ```sql
  CREATE TABLE story_views (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    story_id UUID NOT NULL REFERENCES stories(id) ON DELETE CASCADE,
    viewer_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    viewed_at TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE(story_id, viewer_id) -- One view per user per story
  );
  ```

- [x] 7.5.1.1.3 Create indexes
  - Index on `stories(user_id, expires_at)` for user's active stories
  - Index on `stories(expires_at)` for cleanup job
  - Index on `story_views(story_id)` for view count queries

#### 7.5.1.2 Story Creation Endpoint

- [x] 7.5.1.2.1 Create `POST /api/v1/stories` endpoint
  - Input: `{ "audio_url": "...", "audio_duration": 30.5, "midi_data": {...}, "bpm": 128, "key": "C", "genre": ["electronic"] }`
  - Returns: `{ "story_id": "...", "expires_at": "2024-12-06T12:00:00Z" }`
  - Validate: Duration 5-60 seconds, user authenticated
  - Set `expires_at` to 24 hours from creation
  - Store MIDI data as JSONB (preserve note events with timing)

- [x] 7.5.1.2.2 Implement MIDI data structure
  ```json
  {
    "events": [
      {"time": 0.0, "type": "note_on", "note": 60, "velocity": 100, "channel": 0},
      {"time": 0.5, "type": "note_off", "note": 60, "channel": 0},
      {"time": 1.0, "type": "note_on", "note": 64, "velocity": 90, "channel": 0}
    ],
    "total_time": 30.5,
    "tempo": 128,
    "time_signature": [4, 4]
  }
  ```

- [x] 7.5.1.2.3 Add audio upload endpoint for stories
  - `POST /api/v1/stories/upload` - Upload story audio (max 60 seconds)
  - Returns: `{ "audio_url": "cdn_url", "duration": 25.3, "waveform_data": "svg..." }`
  - Generate waveform during upload
  - Validate duration before accepting

#### 7.5.1.3 Story Retrieval Endpoints

- [x] 7.5.1.3.1 Create `GET /api/v1/stories` endpoint (feed of active stories)
  - Returns: `[{ "story_id": "...", "user": {...}, "audio_url": "...", "duration": 30, "midi_data": {...}, "view_count": 42, "viewed": false, "expires_at": "..." }]`
  - Filter: Only stories from followed users + own stories
  - Filter: Only stories where `expires_at > NOW()`
  - Sort: Most recent first
  - Include `viewed` boolean (has current user viewed this story?)

- [x] 7.5.1.3.2 Create `GET /api/v1/stories/:story_id` endpoint
  - Returns: Full story object with MIDI data
  - Include view count and viewer list (if story owner)

- [x] 7.5.1.3.3 Create `GET /api/v1/users/:user_id/stories` endpoint
  - Returns: All active stories for a specific user
  - Used for viewing user's story highlights

#### 7.5.1.4 Story Viewing & Analytics

- [x] 7.5.1.4.1 Create `POST /api/v1/stories/:story_id/view` endpoint
  - Mark story as viewed by current user
  - Insert into `story_views` table (or update if exists)
  - Increment `view_count` on story
  - Return: `{ "viewed": true, "view_count": 43 }`

- [x] 7.5.1.4.2 Create `GET /api/v1/stories/:story_id/views` endpoint (for story owner)
  - Returns: List of users who viewed the story
  - `[{ "user": {...}, "viewed_at": "..." }]`
  - Only accessible by story owner

#### 7.5.1.5 Story Expiration & Cleanup

- [x] 7.5.1.5.1 Create background job to delete expired stories
  - Run every hour via cron or scheduled task
  - Delete stories where `expires_at < NOW()`
  - Delete associated audio files from CDN
  - Delete associated view records
  - Log cleanup statistics

- [ ] 7.5.1.5.2 Add story expiration notification (optional)
  - Send notification to user when story is about to expire (1 hour before)
  - "Your story expires in 1 hour"

### 7.5.2 MIDI Capture & Processing

> **MIDI Capture Strategy**:
> - Capture MIDI events from DAW during recording
> - Store note_on/note_off events with precise timing
> - Include velocity, channel, and note number
> - Sync MIDI events with audio timeline

#### 7.5.2.1 Plugin MIDI Capture

- [x] 7.5.2.1.1 Implement MIDI event capture in plugin
  - Use JUCE `MidiInput` or DAW's MIDI callback
  - Capture MIDI events during story recording
  - Store events with relative timestamps (from recording start)
  - Include: note number, velocity, channel, on/off

- [x] 7.5.2.1.2 Create `MIDICapture` class (MIDICapture.h/cpp)
  - `startCapture()` - Begin recording MIDI events
  - `stopCapture()` - Stop and return MIDI data
  - `getEvents()` - Get captured events as JSON-serializable structure
  - Handle MIDI clock messages for tempo sync

- [x] 7.5.2.1.3 Integrate MIDI capture with audio recording
  - Start MIDI capture when story recording starts
  - Stop MIDI capture when story recording stops
  - Sync MIDI events with audio timeline
  - Handle cases where no MIDI is present (audio-only story)

#### 7.5.2.2 MIDI Data Processing

- [x] 7.5.2.2.1 Normalize MIDI timing
  - Convert MIDI timestamps to relative time (0.0 = start of recording)
  - Round to millisecond precision
  - Handle tempo changes (if present)

- [x] 7.5.2.2.2 Validate MIDI data
  - Ensure note_on has matching note_off
  - Remove duplicate events
  - Filter out system messages (keep only note events)
  - Limit to reasonable note range (0-127)

### 7.5.3 Story Creation UI (Plugin)

#### 7.5.3.1 Story Recording Interface

- [x] 7.5.3.1.1 Create `StoryRecordingComponent` (StoryRecordingComponent.h/cpp)
  - Record button (similar to main recording)
  - Duration indicator (max 60 seconds)
  - MIDI activity indicator (shows when MIDI is being captured)
  - Waveform preview during recording
  - Stop button (appears after 5 seconds minimum)

- [x] 7.5.3.1.2 Implement story recording flow
  - Click "Create Story" → Opens recording interface
  - Start recording → Capture audio + MIDI simultaneously
  - Show countdown timer (60 seconds max)
  - Auto-stop at 60 seconds
  - Preview before posting

- [x] 7.5.3.1.3 Add story metadata input
  - BPM (auto-detect or manual) ✓ (auto-detected from DAW, displayed in preview)
  - Key (optional) ✓ (displayed in preview, can be set before upload)
  - Genre tags (optional) ✓ (displayed in preview, can be set before upload)
  - All optional for quick sharing ✓

#### 7.5.3.2 Story Preview & Upload

- [x] 7.5.3.2.1 Create story preview screen
  - Show waveform ✓
  - Show MIDI visualization preview (piano roll) ✓
  - Playback controls ✓ (simulated playback with visualization sync)
  - "Post Story" button ✓
  - "Discard" button ✓

- [x] 7.5.3.2.2 Implement story upload
  - Upload audio to CDN ✓ (NetworkClient::uploadStory exists)
  - Generate waveform ✓ (backend handles this)
  - Send story data (including MIDI) to `POST /api/v1/stories` ✓
  - Show upload progress (TODO: Add progress UI to StoryRecordingComponent)
  - Navigate to feed after successful upload ✓ (Wired up in PluginEditor)

### 7.5.4 Story Viewing UI (Plugin)

#### 7.5.4.1 Story Feed Component

- [x] 7.5.4.1.1 Create `StoriesFeedComponent` (StoriesFeedComponent.h/cpp)
  - Horizontal scrollable list of story circles (user avatars)
  - Show user avatar with "ring" indicator if has unviewed stories
  - Click story circle → Opens story viewer
  - Show "Your Story" circle at start (if user has active story)

- [x] 7.5.4.1.2 Implement story circle rendering
  - Circular avatar image
  - Colored ring if has unviewed stories
  - Story count badge (if user has multiple stories)
  - User name below circle

#### 7.5.4.2 Story Viewer Component

- [x] 7.5.4.2.1 Create `StoryViewerComponent` (StoryViewerComponent.h/cpp)
  - Full-screen story view (Snapchat-style)
  - Audio playback with waveform
  - MIDI visualization (piano roll or note visualization)
  - User info header (avatar, name)
  - Progress bar showing story duration
  - Swipe left/right to navigate between stories
  - Tap to pause/play

- [x] 7.5.4.2.2 Implement MIDI visualization
  - Piano roll view: Notes displayed on vertical piano keys, horizontal timeline
  - Color-code notes by velocity (darker = louder)
  - Show note duration (horizontal bars)
  - Sync visualization with audio playback
  - Animate notes as they play (highlight active notes)

- [x] 7.5.4.2.3 Add MIDI visualization options
  - Toggle between piano roll and note list view (TODO: Add note list view alternative)
  - Zoom in/out on timeline ✓ (Ctrl/Cmd+wheel zooms, wheel scrolls)
  - Show/hide velocity visualization ✓ (setShowVelocity method exists)
  - Show/hide channel colors (if multi-channel) ✓ (setShowChannels method exists)

#### 7.5.4.3 Story Navigation

- [x] 7.5.4.3.1 Implement story progression
  - Auto-advance to next story after current finishes
  - Swipe left = next story, swipe right = previous story
  - Swipe down = close story viewer, return to feed
  - Keyboard: Arrow keys for navigation, Space for play/pause (partially - swipe implemented)

- [x] 7.5.4.3.2 Add story interactions
  - View count display (if story owner) ✓
  - "Viewers" button (if story owner) → Shows list of viewers ✓ (button added, callback wired)
  - Share button → Copy story link (expires in 24h) ✓
  - No likes/comments on stories (by design, ephemeral) ✓

#### 7.5.4.4 Story Expiration Indicators

- [x] 7.5.4.4.1 Show expiration time
  - Display "Expires in X hours" on story
  - Update countdown in real-time
  - Gray out stories that are about to expire (< 1 hour) (partially - expiration text shown)

- [x] 7.5.4.4.2 Handle expired stories
  - Filter out expired stories from feed ✓
  - Show "Story expired" message if user tries to view ✓
  - Remove from user's story list automatically ✓ (filtered on load)

### 7.5.5 MIDI Visualization Engine

> **Visualization Options**:
> - Piano roll (classic DAW view)
> - Note waterfall (falling notes visualization)
> - Circular visualization (notes arranged in circle)
> - Minimalist (just active notes)

#### 7.5.5.1 Piano Roll Renderer

- [x] 7.5.5.1.1 Create `PianoRollComponent` (PianoRollComponent.h/cpp)
  - Vertical piano keys (C0 to C8, or configurable range)
  - Horizontal timeline (synced with audio)
  - Note rectangles (position = note, width = duration, height = key height)
  - Color by velocity or channel

- [x] 7.5.5.1.2 Implement piano roll rendering
  - Draw piano keys (white/black keys)
  - Draw note rectangles from MIDI events
  - Highlight active notes during playback
  - Scroll timeline as audio plays
  - Zoom controls (pinch or mouse wheel) (partially - basic rendering complete)

- [x] 7.5.5.1.3 Add piano roll interactions
  - Click note → Seek audio to that time ✓
  - Hover note → Show note name and velocity ✓
  - Scroll timeline to navigate ✓ (mouse wheel scrolls, Ctrl/Cmd+wheel zooms)

#### 7.5.5.2 Alternative Visualizations

- [ ] 7.5.5.2.1 Create note waterfall visualization
  - Notes fall from top as they play
  - Color by velocity
  - Minimalist, visually appealing

- [ ] 7.5.5.2.2 Create circular visualization
  - Notes arranged in circle (like a clock)
  - Active notes highlighted
  - Artistic, less technical

### 7.5.6 Story Highlights (Optional Enhancement)

- [ ] 7.5.6.1 Allow users to save stories to highlights
  - "Add to Highlights" button on story
  - Creates permanent collection (doesn't expire)
  - Organize highlights by category (e.g., "Jams", "Experiments")

- [ ] 7.5.6.2 Display highlights on profile
  - Show highlight collections on user profile
  - Click highlight → View saved stories
  - Similar to Instagram highlights

### 7.5.7 Testing & Integration

- [ ] 7.5.7.1 Write backend tests for story endpoints
  - Test story creation with MIDI data
  - Test story retrieval and filtering
  - Test story viewing and analytics
  - Test expiration cleanup job

- [ ] 7.5.7.2 Write plugin unit tests
  - Test MIDI capture functionality
  - Test story recording flow
  - Test MIDI visualization rendering

- [ ] 7.5.7.3 Integration testing
  - Test story creation from various DAWs
  - Test MIDI capture with different MIDI sources
  - Test story viewing across multiple users
  - Test expiration after 24 hours
  - Test MIDI visualization accuracy

### 7.5.8 Performance Considerations

- [ ] 7.5.8.1 Optimize MIDI data storage
  - Compress MIDI JSON (remove redundant data)
  - Limit MIDI events (sample if too many events)
  - Cache MIDI visualization rendering

- [ ] 7.5.8.2 Optimize story feed loading
  - Lazy load stories (load on demand)
  - Preload next story in sequence
  - Cache story audio and MIDI data
  - Use CDN for story audio files

---

## Phase 8: Polish & Performance
**Goal**: Production-ready quality
**Duration**: 2 weeks

### 8.1 Performance Optimization

> **Testing**: Profiling and benchmarks.
> Plugin CPU: Load in DAW → Monitor CPU usage in DAW's performance meter → Should be <5% idle.
> Backend: `go test -bench=. ./...` for any benchmark tests.
> API latency: `time curl http://localhost:8787/api/v1/health` → Should be <50ms.

- [ ] 8.1.1 Profile plugin CPU usage during playback
- [ ] 8.1.2 Optimize UI rendering (reduce repaints)
- [ ] 8.1.3 Implement image lazy loading
- [ ] 8.1.4 Add waveform rendering caching
- [ ] 8.1.5 Optimize network requests (batching)
- [ ] 8.1.6 Add response compression (gzip)
- [ ] 8.1.7 Implement CDN for static assets
- [ ] 8.1.8 Add database query optimization
- [ ] 8.1.9 Implement connection pooling tuning
- [ ] 8.1.10 Profile and optimize memory usage

### 8.2 Plugin Stability

> **Testing**: Manual DAW testing matrix.
> **Critical DAWs**: Ableton Live, FL Studio, Logic Pro, Reaper, Studio One.
> For each DAW: Load plugin → Record → Upload → Browse feed → Close/reopen project → Verify state persists.
> Stress test: Open in project with 50+ tracks → Verify no audio dropouts or crashes.

- [ ] 8.2.1 Test in Ableton Live (Mac + Windows)
- [ ] 8.2.2 Test in FL Studio (Windows)
- [ ] 8.2.3 Test in Logic Pro (Mac)
- [ ] 8.2.4 Test in Reaper (cross-platform)
- [ ] 8.2.5 Test in Studio One
- [ ] 8.2.6 Fix any DAW-specific crashes
- [ ] 8.2.7 Test with high CPU project (50+ tracks)
- [ ] 8.2.8 Test network timeout handling
- [ ] 8.2.9 Test plugin state persistence across DAW sessions
- [ ] 8.2.10 Add crash reporting (Sentry or similar)

### 8.3 UI Polish

> **Testing**: Visual inspection and UX testing.
> Test at different window sizes: Resize plugin window → All elements should reflow correctly.
> Test loading states: Disconnect network → Verify loading spinners and error messages display.
> Test empty states: New account with no posts → Helpful prompts should appear.

- [ ] 8.3.1 Finalize color scheme and typography
- [ ] 8.3.2 Add loading states and skeletons
- [ ] 8.3.3 Implement smooth animations (easing)
- [ ] 8.3.4 Add haptic feedback indicators
- [ ] 8.3.5 Improve error messages (user-friendly)
- [ ] 8.3.6 Add empty states with helpful prompts
- [ ] 8.3.7 Implement dark/light theme toggle
- [ ] 8.3.8 Add accessibility improvements (focus indicators)
- [ ] 8.3.9 Test UI at different plugin window sizes
- [ ] 8.3.10 Add plugin resize support

#### 8.3.11 Login Flow Improvements (NEW)

> **Current Issues Identified (Dec 5, 2024)**:
> - `AuthComponent.cpp` has **hardcoded `localhost:8787`** instead of using `NetworkClient` config
> - `ProfileSetupComponent.cpp` has **hardcoded button positions** (not responsive)
> - OAuth polling lacks real-time status feedback during the wait period
> - Login/signup forms don't use the shared `NetworkClient` - duplicated HTTP code
> - No "Remember me" option for credentials
> - No password reset flow in plugin

**AuthComponent Refactoring:**
- [ ] 8.3.11.1 Refactor `AuthComponent` to use `NetworkClient` instead of raw JUCE URL calls
  - Remove hardcoded `http://localhost:8787` (line 601, 733)
  - Pass `NetworkClient` reference to `AuthComponent` constructor
  - Use `networkClient->post()` for login/register API calls
- [ ] 8.3.11.2 Add proper error handling from `NetworkClient::RequestResult`
- [ ] 8.3.11.3 Show connection status indicator in auth screen (use `ConnectionIndicator`)
- [ ] 8.3.11.4 Add "Connecting to server..." state when backend is unreachable

**ProfileSetupComponent Improvements:**
- [ ] 8.3.11.5 Make `ProfileSetupComponent` layout responsive (use `resized()` properly)
  - Replace hardcoded `juce::Rectangle<int>(400, 150, 150, 36)` with calculated positions
  - Center content on different window sizes
- [ ] 8.3.11.6 Add upload progress indicator when uploading profile picture
- [ ] 8.3.11.7 Show S3 URL success feedback after upload completes
- [ ] 8.3.11.8 Add image cropping UI before upload (circular crop preview)

**OAuth Flow Improvements:**
- [ ] 8.3.11.9 Show animated "Waiting for authorization..." during OAuth polling
- [ ] 8.3.11.10 Add countdown timer for OAuth session timeout
- [ ] 8.3.11.11 Add "Cancel" button during OAuth polling to return to welcome screen
- [ ] 8.3.11.12 Show browser launch confirmation ("A browser window has been opened...")

**Authentication UX Enhancements:**
- [ ] 8.3.11.13 Add "Remember me" checkbox to persist credentials securely
- [ ] 8.3.11.14 Implement "Forgot password" link → opens web URL
- [ ] 8.3.11.15 Add password strength indicator during signup
- [ ] 8.3.11.16 Show email verification prompt if email not verified
- [ ] 8.3.11.17 Add biometric/keychain integration for saved credentials (future)

### 8.4 Backend Hardening

> **Testing**: Security testing.
> Rate limiting: Send 100 rapid requests → Verify 429 errors after threshold.
> Input validation: Send malformed JSON → Verify 400 errors with clear messages.
> SQL injection: Test with `'; DROP TABLE users; --` in search queries → Should be escaped.

- [ ] 8.4.1 Add rate limiting (per user, per IP)
- [ ] 8.4.2 Implement request validation (input sanitization)
- [ ] 8.4.3 Add SQL injection protection (verify ORM usage)
- [ ] 8.4.4 Implement XSS protection
- [ ] 8.4.5 Add CSRF protection (not needed for API-only)
- [ ] 8.4.6 Implement API versioning (v1, v2)
- [ ] 8.4.7 Add request logging (structured JSON)
- [ ] 8.4.8 Implement error tracking (Sentry)
- [ ] 8.4.9 Add health check endpoints (for load balancer)
- [ ] 8.4.10 Implement graceful shutdown

---

## Phase 9: Infrastructure & DevOps
**Goal**: Production deployment
**Duration**: 1.5 weeks

### 9.1 Containerization

> **Testing**: Docker build and run tests.
> Build: `docker build -t sidechain-backend ./backend` → Should complete without errors.
> Run: `docker-compose up` → All services start → API responds at localhost:8787.
> Health: `curl http://localhost:8787/health` → Returns 200 OK.

- [ ] 9.1.1 Create Dockerfile for Go backend
- [ ] 9.1.2 Create docker-compose.yml for local dev
- [ ] 9.1.3 Add PostgreSQL container
- [ ] 9.1.4 Add Redis container (for sessions/cache)
- [ ] 9.1.5 Configure environment variable injection
- [ ] 9.1.6 Set up volume mounts for persistence
- [ ] 9.1.7 Create production Docker image (multi-stage build)
- [ ] 9.1.8 Optimize image size (Alpine base)
- [ ] 9.1.9 Add Docker health checks
- [ ] 9.1.10 Document container deployment

### 9.2 Cloud Infrastructure

> **Testing**: Production deployment verification.
> Deploy: Push to main → GitHub Actions deploys → API responds at api.sidechain.live.
> SSL: `curl https://api.sidechain.live/health` → Valid certificate, 200 OK.
> CDN: Upload audio → Verify CDN URL returns audio file with correct headers.

- [ ] 9.2.1 Choose hosting provider (Railway, Fly.io, AWS)
- [ ] 9.2.2 Set up production PostgreSQL (managed)
- [ ] 9.2.3 Configure S3 bucket with CORS
- [ ] 9.2.4 Set up CloudFront CDN for audio
- [ ] 9.2.5 Configure DNS (api.sidechain.live)
- [ ] 9.2.6 Set up SSL certificates (Let's Encrypt)
- [ ] 9.2.7 Configure auto-scaling (if using AWS)
- [ ] 9.2.8 Set up Redis for caching/sessions
- [ ] 9.2.9 Configure backup strategy (daily DB backups)
- [ ] 9.2.10 Set up monitoring (CloudWatch or Datadog)

### 9.3 CI/CD Pipeline

> **Testing**: GitHub Actions verification.
> Push to branch → Verify Actions run → All tests pass → Builds succeed.
> Create release tag `v1.0.0` → Verify release workflow creates artifacts.
> Download artifacts → Verify plugin binaries work on each platform.

- [x] 9.3.1 Configure GitHub Actions for backend tests - build.yml with PostgreSQL service container
- [x] 9.3.2 Configure GitHub Actions for plugin builds - macOS/Linux/Windows matrix builds
- [ ] 9.3.3 Add automated deployment on merge to main
- [ ] 9.3.4 Set up staging environment
- [ ] 9.3.5 Add deployment approval for production
- [ ] 9.3.6 Implement database migration on deploy
- [ ] 9.3.7 Add rollback capability
- [x] 9.3.8 Set up release versioning (semantic versioning) - v*.*.* tag triggers
- [x] 9.3.9 Create GitHub releases for plugin binaries - release.yml with artifact upload
- [ ] 9.3.10 Add deployment notifications (Slack/Discord)

### 9.4 Monitoring & Alerting

> **Testing**: Monitoring verification.
> Uptime: Configure UptimeRobot → Intentionally stop server → Verify alert received.
> Errors: Trigger 500 error → Verify Sentry captures error → Alert sent to Slack.
> Performance: Check APM dashboard → Verify request latencies are being tracked.

- [ ] 9.4.1 Set up uptime monitoring (UptimeRobot or similar)
- [ ] 9.4.2 Configure error alerting (Sentry → Slack)
- [ ] 9.4.3 Add performance monitoring (APM)
- [ ] 9.4.4 Set up log aggregation (Logflare or CloudWatch)
- [ ] 9.4.5 Create monitoring dashboard
- [ ] 9.4.6 Define SLOs (99.9% uptime, <200ms API response)
- [ ] 9.4.7 Set up alerting thresholds
- [ ] 9.4.8 Add cost monitoring (AWS billing alerts)

---

## Phase 10: Launch Preparation
**Goal**: Ready for real users
**Duration**: 2 weeks

### 10.1 Plugin Distribution

> **Testing**: Fresh install testing.
> macOS: Run .pkg installer on clean Mac → Plugin appears in /Library/Audio/Plug-Ins/VST3.
> Windows: Run .exe installer on clean Windows → Plugin appears in C:\Program Files\Common Files\VST3.
> DAW scan: Open DAW → Plugin discovered → Loads without errors.

- [ ] 10.1.1 Create plugin installer (macOS .pkg)
- [ ] 10.1.2 Create plugin installer (Windows .exe)
- [ ] 10.1.3 Code sign macOS plugin (Developer ID)
- [ ] 10.1.4 Code sign Windows plugin (EV certificate)
- [ ] 10.1.5 Notarize macOS plugin (Apple notarization)
- [ ] 10.1.6 Create update mechanism (check for updates)
- [ ] 10.1.7 Set up download server / CDN
- [ ] 10.1.8 Create installation documentation
- [ ] 10.1.9 Test installation on fresh systems
- [ ] 10.1.10 Create uninstaller

### 10.2 Legal & Compliance

> **Testing**: Compliance verification.
> GDPR export: Request data export → Verify JSON file contains all user data.
> Account deletion: Delete account → Verify all user data removed from database and S3.
> Age verification: Register with birthdate < 13 years ago → Should be blocked.

- [ ] 10.2.1 Draft Terms of Service
- [ ] 10.2.2 Draft Privacy Policy
- [ ] 10.2.3 Implement GDPR data export
- [ ] 10.2.4 Implement account deletion (right to erasure)
- [ ] 10.2.5 Add cookie consent (if web component)
- [ ] 10.2.6 Add age verification (13+ requirement)
- [ ] 10.2.7 Set up DMCA takedown process
- [ ] 10.2.8 Create content guidelines
- [ ] 10.2.9 Add copyright reporting flow
- [ ] 10.2.10 Consult with lawyer on music licensing

### 10.3 Documentation

- [ ] 10.3.1 Create user guide (Getting Started)
- [ ] 10.3.2 Document DAW-specific setup
- [ ] 10.3.3 Create FAQ page
- [ ] 10.3.4 Add troubleshooting guide
- [ ] 10.3.5 Create video tutorials
- [ ] 10.3.6 Document API (for potential integrations)
- [ ] 10.3.7 Create changelog
- [ ] 10.3.8 Set up knowledge base (Notion or GitBook)

### 10.4 Marketing Website

- [ ] 10.4.1 Design landing page
- [ ] 10.4.2 Implement landing page (Next.js or static)
- [ ] 10.4.3 Add download buttons (Mac, Windows)
- [ ] 10.4.4 Create feature showcase
- [ ] 10.4.5 Add testimonials section (after beta)
- [ ] 10.4.6 Implement email signup (pre-launch)
- [ ] 10.4.7 Add social links
- [ ] 10.4.8 Set up analytics (Plausible or GA4)
- [ ] 10.4.9 Optimize for SEO
- [ ] 10.4.10 Add press kit

---

## Phase 11: Beta Launch
**Goal**: Get real user feedback
**Duration**: 4 weeks

### 11.1 Beta Program Setup

> **Testing**: Beta infrastructure verification.
> Invite system: Generate invite code → New user registers with code → Account created successfully.
> Feedback: User submits feedback → Appears in feedback collection system.
> Discord/Slack: Join community → Verify bot posts new user announcements.

- [ ] 11.1.1 Create beta signup form
- [ ] 11.1.2 Set up beta user invite system
- [ ] 11.1.3 Create beta user onboarding flow
- [ ] 11.1.4 Set up feedback collection (in-app or survey)
- [ ] 11.1.5 Create beta Discord/Slack community
- [ ] 11.1.6 Define beta testing goals and metrics
- [ ] 11.1.7 Recruit initial beta testers (50-100)
- [ ] 11.1.8 Create beta tester agreement
- [ ] 11.1.9 Set up bug reporting process
- [ ] 11.1.10 Plan beta milestone releases

### 11.2 Beta Testing Rounds

> **Testing**: Structured beta test plan.
> **Round 1**: 10 users test core flow (record → upload → view) → Collect crash reports and UX feedback.
> **Round 2**: 30 users test social features (follow, like, comment) → Measure engagement metrics.
> **Round 3**: 100 users full feature test → Load testing, security audit.

- [ ] 11.2.1 **Round 1**: Core flow testing (record → upload → view)
- [ ] 11.2.2 Collect Round 1 feedback
- [ ] 11.2.3 Fix critical issues from Round 1
- [ ] 11.2.4 **Round 2**: Social features (follow, like, comment)
- [ ] 11.2.5 Collect Round 2 feedback
- [ ] 11.2.6 Fix critical issues from Round 2
- [ ] 11.2.7 **Round 3**: Full feature testing
- [ ] 11.2.8 Performance testing under load
- [ ] 11.2.9 Security audit (basic)
- [ ] 11.2.10 Final polish based on feedback

### 11.3 Metrics & Analytics

> **Testing**: Analytics verification.
> Dashboard: Open analytics dashboard → Verify DAU/WAU/MAU charts populate.
> Events: Create post → Verify "post_created" event logged → Appears in funnel.
> Retention: Check D1/D7/D30 cohort charts → Data looks reasonable.

- [ ] 11.3.1 Track DAU/WAU/MAU
- [ ] 11.3.2 Track posts created per day
- [ ] 11.3.3 Track engagement rate (likes/plays per post)
- [ ] 11.3.4 Track retention (D1, D7, D30)
- [ ] 11.3.5 Track feature usage
- [ ] 11.3.6 Track plugin stability (crash rate)
- [ ] 11.3.7 Track API response times
- [ ] 11.3.8 Create analytics dashboard
- [ ] 11.3.9 Set up funnel tracking (install → signup → first post)
- [ ] 11.3.10 A/B testing framework (future)

---

## Phase 12: Public Launch
**Goal**: Open to everyone
**Duration**: Ongoing

### 12.1 Launch Checklist

> **Testing**: Pre-launch verification.
> Full QA pass: Run through all manual testing checklists (auth, recording, feed, profile).
> Load test: Simulate 1000 concurrent users → API responds within SLO (<200ms).
> Incident response: Team knows escalation path → Runbook documented.

- [ ] 12.1.1 Final QA pass
- [ ] 12.1.2 Database backup before launch
- [ ] 12.1.3 Scale infrastructure for launch traffic
- [ ] 12.1.4 Prepare incident response plan
- [ ] 12.1.5 Brief support team (if any)
- [ ] 12.1.6 Prepare launch announcement
- [ ] 12.1.7 Schedule social media posts
- [ ] 12.1.8 Coordinate with beta users for launch content
- [ ] 12.1.9 Set up customer support (email or chat)
- [ ] 12.1.10 **LAUNCH**

### 12.2 Launch Marketing

- [ ] 12.2.1 Post on Product Hunt
- [ ] 12.2.2 Post on Hacker News
- [ ] 12.2.3 Post on relevant subreddits (r/WeAreTheMusicMakers, etc.)
- [ ] 12.2.4 Reach out to music production YouTubers
- [ ] 12.2.5 Reach out to music blogs
- [ ] 12.2.6 Create launch video/demo
- [ ] 12.2.7 Email pre-launch signup list
- [ ] 12.2.8 Twitter/X launch thread
- [ ] 12.2.9 Instagram teaser posts
- [ ] 12.2.10 Paid ads testing (optional)

### 12.3 Post-Launch

- [ ] 12.3.1 Monitor error rates and performance
- [ ] 12.3.2 Respond to user feedback
- [ ] 12.3.3 Fix critical bugs immediately
- [ ] 12.3.4 Plan v1.1 release with top requests
- [ ] 12.3.5 Continue community engagement
- [ ] 12.3.6 Iterate based on data
- [ ] 12.3.7 Plan monetization (Pro tier)
- [ ] 12.3.8 Consider mobile app (Phase 2 product)
- [ ] 12.3.9 Consider web player (share links work everywhere)
- [ ] 12.3.10 Build team (if traction warrants)

---

## Future Features (Post-Launch Backlog)

### MIDI Integration
- [x] F.1.1 MIDI pattern capture from DAW - **Implemented in Phase 7.5 (Stories)**
- [x] F.1.2 MIDI visualization (piano roll) - **Implemented in Phase 7.5 (Stories)**
- [x] F.1.3 MIDI pattern sharing alongside audio - **Implemented in Phase 7.5 (Stories)**
- [ ] F.1.4 "Drop MIDI to track" feature - Allow importing MIDI from stories into DAW
- [ ] F.1.5 MIDI battle royale competitions - Competitive MIDI challenges

### Collaboration
- [ ] F.2.1 Cross-DAW MIDI sync (challenging)
- [ ] F.2.2 Collaborative playlists
- [ ] F.2.3 Remix chains (remix a remix)
- [ ] F.2.4 Project file exchange
- [ ] F.2.5 Real-time co-production (very ambitious)

### Monetization
- [ ] F.3.1 Pro subscription ($5/month)
- [ ] F.3.2 Extended upload limits
- [ ] F.3.3 Analytics dashboard for Pro
- [ ] F.3.4 Custom profile themes
- [ ] F.3.5 Sample pack sales marketplace
- [ ] F.3.6 Tip jar / support creators

### Audio Quality
- [ ] F.4.1 Lossless audio option (FLAC)
- [ ] F.4.2 Stem separation (AI-powered)
- [ ] F.4.3 Audio fingerprinting for copyright
- [ ] F.4.4 Sample detection (is this copyrighted?)
- [ ] F.4.5 Automatic tempo/key correction

### Mobile
- [ ] F.5.1 iOS app (listen only initially)
- [ ] F.5.2 Android app
- [ ] F.5.3 Mobile notifications
- [ ] F.5.4 Mobile-first upload (from Voice Memos)
- [ ] F.5.5 TikTok-style vertical video for loops

---

## Timeline Summary

| Phase | Duration | Focus |
|-------|----------|-------|
| Phase 1 | 2 weeks | Foundation (Stream.io, OAuth, NetworkClient) |
| Phase 2 | 2 weeks | Audio Capture Pipeline |
| Phase 3 | 2 weeks | Feed Experience |
| Phase 4 | 1.5 weeks | User Profiles |
| Phase 5 | 2 weeks | Real-time Features |
| Phase 6 | 1.5 weeks | Comments & Community |
| Phase 6.5 | 2 weeks | Direct Messaging (getstream.io Chat) |
| Phase 7 | 1.5 weeks | Search & Discovery |
| Phase 7.5 | 2.5 weeks | Music Stories (Snapchat-style with MIDI) |
| Phase 8 | 2 weeks | Polish & Performance |
| Phase 9 | 1.5 weeks | Infrastructure & DevOps |
| Phase 10 | 2 weeks | Launch Preparation |
| Phase 11 | 4 weeks | Beta Launch |
| Phase 12 | Ongoing | Public Launch |

**Total Estimated Time to Launch**: ~7 months (solo developer, includes messaging and stories)

---

## Success Metrics

### MVP Success
- [ ] 100 beta users actively posting
- [ ] Average 5 posts per user
- [ ] 30-day retention > 20%
- [ ] Plugin stability > 99%

### Launch Success
- [ ] 1,000 registered users in first month
- [ ] 100 posts created per day
- [ ] DAU/MAU ratio > 20%
- [ ] NPS score > 30

### Growth Targets (6 months post-launch)
- [ ] 10,000 registered users
- [ ] 500 DAU
- [ ] 1,000 posts per day
- [ ] Community-driven growth (word of mouth)

---

## Risk Mitigation

### Technical Risks
| Risk | Mitigation |
|------|------------|
| DAW compatibility issues | Extensive testing, community bug reports |
| getstream.io rate limits | Cache aggressively, implement backoff |
| Audio processing bottlenecks | Async job queue, auto-scaling |
| Plugin crashes | Crash reporting, sandboxed network calls |

### Business Risks
| Risk | Mitigation |
|------|------------|
| Low adoption | Focus on core value prop, iterate on feedback |
| Copyright issues | Implement DMCA process, audio fingerprinting |
| Competition | Move fast, community-first approach |
| Running out of money | Bootstrap, minimize costs (Railway, etc.) |

---

*This plan is a living document. Update as you make progress.*
