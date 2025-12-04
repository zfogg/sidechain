# Sidechain Development Plan

> **Vision**: Instagram Reels / TikTok / YouTube Shorts — but for musicians, with audio.
> Share loops directly from your DAW, discover what other producers are making.

**Last Updated**: December 2024
**Developer**: Solo project
**Target**: Professional launch with real users

---

## Current State Assessment

### What's Working
- VST3 plugin loads in DAWs (tested in AudioPluginHost)
- Email/password registration and login
- JWT authentication with token persistence
- Basic UI rendering (dark theme, forms)
- PostgreSQL database with migrations
- S3 storage configuration
- CMake build system with AudioPluginHost
- **Full audio capture pipeline** (lock-free recording, circular buffer, level metering)
- **Waveform visualization** (real-time during recording, preview after)
- **Recording UI** (start/stop, level meters, time display, preview playback)
- **GitHub Actions CI/CD** (macOS/Linux/Windows builds, release automation)
- **Codecov integration** (component-based coverage tracking)

### What's Stubbed/Incomplete
- Feed UI data binding (post cards, playback)
- Upload UI flow (BPM/key input, progress indicator)

### Recently Completed
- **Backend audio processing pipeline** (FFmpeg normalization, MP3 encoding, waveform SVG, S3 upload)
- **Background job queue** (goroutine worker pool with channels)
- Plugin NetworkClient HTTP implementation (GET, POST, multipart uploads)
- HTTP status code extraction and user-friendly error messages
- Stream.io Feeds V2 API integration (activities, reactions, follows) - switched from V3 beta to stable V2
- Integration tests for Stream.io client (all 8 tests passing)
- OAuth with Google/Discord (token exchange, user info, account linking)
- OAuth token storage for future refresh
- **AudioCapture class** (CircularAudioBuffer, lock-free recording, level metering)
- **RecordingComponent** (full recording UI with waveform, preview, level meters)
- **WAV export** with temp file management
- **GitHub Actions release workflow** (v0.0.1 released)

### What's Missing
- Audio playback in feed
- Real-time WebSocket features
- Search and discovery
- Production deployment

---

## Phase 1: Foundation Completion
**Goal**: Get real data flowing through the system
**Duration**: 2 weeks

### 1.1 Build System Finalization

- [x] 1.1.1 Migrate from Projucer to CMake
- [x] 1.1.2 Add JUCE 8.0.8 as dependency (deps/JUCE)
- [x] 1.1.3 Configure CMakeLists.txt for VST3/AU/Standalone builds
- [x] 1.1.4 Add AudioPluginHost to CMake configuration
- [x] 1.1.5 Set up `cmake --install` for platform-specific VST3 paths
- [x] 1.1.6 Create GitHub Actions CI for macOS/Linux/Windows
- [x] 1.1.7 Test CI builds on all three platforms
- [x] 1.1.8 Add build badges to README (Codecov badge and sunburst graph)

### 1.2 Stream.io Integration (Critical Path)

- [x] 1.2.1 Create getstream.io account and obtain API credentials
- [x] 1.2.2 Configure getstream.io Feeds V2 (V3 is beta, not dashboard-integrated; using stream-go2/v8)
- [x] 1.2.3 Define feed groups: `user` (personal), `timeline` (following), `global` (all) in getstream dashboard
- [x] 1.2.4 Implement `CreateLoopActivity()` with real Stream.io API call (stream/client.go:95-168)
- [x] 1.2.5 Implement `GetUserTimeline()` with real Stream.io query (stream/client.go:170-198)
- [x] 1.2.6 Implement `GetGlobalFeed()` with pagination (stream/client.go:200-226)
- [x] 1.2.7 Implement `FollowUser()` / `UnfollowUser()` operations (stream/client.go:228-264)
- [x] 1.2.8 Implement `AddReaction()` for likes and emoji reactions (stream/client.go:266-316)
- [x] 1.2.9 Add Stream.io user creation on registration (auth/oauth.go:231-236)
- [x] 1.2.10 Write integration tests for Stream.io client
- [x] 1.2.11 Remove mock data from stream/client.go - replaced with real API calls

### 1.3 OAuth Completion

- [x] 1.3.1 Register Google OAuth application (console.developers.google.com) (client secret added to backend/.env)
- [x] 1.3.2 Register Discord OAuth application (discord.com/developers)
- [x] 1.3.3 Implement `exchangeGoogleCode()` token exchange (oauth.go:187 - uses oauth2.Config.Exchange)
- [x] 1.3.4 Implement `getGoogleUserInfo()` profile fetch (oauth.go:186-216)
- [x] 1.3.5 Implement `exchangeDiscordCode()` token exchange (oauth.go:220 - uses oauth2.Config.Exchange)
- [x] 1.3.6 Implement `getDiscordUserInfo()` profile fetch (oauth.go:218-254)
- [x] 1.3.7 Test account unification (OAuth + native same email) (service_test.go:TestAccountUnification, TestReverseAccountUnification)
- [x] 1.3.8 Add OAuth error handling and user feedback (handlers/auth.go:226-268 - getOAuthErrorMessage, parseOAuthError)
- [x] 1.3.9 Store OAuth tokens for future refresh (oauth.go:104-124 - updateOAuthTokens)

### 1.4 Plugin NetworkClient Implementation

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

---

## Phase 2: Audio Capture Pipeline
**Goal**: Capture audio from DAW and upload to backend
**Duration**: 2 weeks

### 2.1 DAW Audio Integration ✅

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

### 2.2 Audio Encoding (Plugin Side)

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

### 2.3 Upload Flow

- [ ] 2.3.1 Design upload UI (title, BPM, key, genre selector)
- [ ] 2.3.2 Implement BPM detection (tap tempo or auto-detect)
- [ ] 2.3.3 Implement key detection (basic pitch analysis)
- [ ] 2.3.4 Add genre dropdown (electronic, hip-hop, rock, etc.)
- [ ] 2.3.5 Create upload progress indicator (0-100%)
- [ ] 2.3.6 Implement chunked upload for large files
- [ ] 2.3.7 Add upload cancellation support
- [ ] 2.3.8 Handle upload failures with retry option
- [ ] 2.3.9 Show success confirmation with post preview
- [ ] 2.3.10 Clear recording buffer after successful upload

### 2.4 Backend Audio Processing ✅

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

---

## Phase 3: Feed Experience
**Goal**: Browse and interact with posts in the plugin
**Duration**: 2 weeks

### 3.1 Feed Data Layer

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

### 3.2 Post Card Component

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

### 3.3 Audio Playback Engine

- [x] 3.3.1 Research JUCE audio playback in plugin context - AudioFormatReaderSource + ResamplingAudioSource mixed into processBlock
- [x] 3.3.2 Create AudioPlayer class with transport controls - AudioPlayer.h/cpp with play/pause/stop/seek
- [x] 3.3.3 Implement audio streaming from URL (no full download) - Downloads to memory, caches for replay
- [x] 3.3.4 Add playback progress indicator on waveform - PostCardComponent::setPlaybackProgress() with callback integration
- [x] 3.3.5 Implement seek by tapping waveform - onWaveformClicked callback connected to AudioPlayer::seekToNormalizedPosition()
- [x] 3.3.6 Add volume control (separate from DAW) - AudioPlayer::setVolume() with atomic float
- [ ] 3.3.7 Implement auto-play next post option
- [ ] 3.3.8 Handle audio focus (pause when DAW plays)
- [ ] 3.3.9 Add keyboard shortcuts (space = play/pause)
- [x] 3.3.10 Cache recently played audio (memory limit) - LRU cache with 50MB limit, eviction strategy
- [ ] 3.3.11 Pre-buffer next post for seamless playback - preloadAudio() method available

### 3.4 Social Interactions

- [x] 3.4.1 Implement like/unlike toggle (optimistic UI) - PostCardComponent handles UI state, callback wired up
- [ ] 3.4.2 Add like animation (heart burst)
- [ ] 3.4.3 Implement emoji reactions panel
- [ ] 3.4.4 Show reaction counts and types
- [ ] 3.4.5 Implement follow/unfollow from post card
- [ ] 3.4.6 Add "following" indicator on posts from followed users
- [ ] 3.4.7 Implement play count tracking
- [ ] 3.4.8 Track listen duration (for algorithm)
- [ ] 3.4.9 Implement "Add to DAW" button (download to project folder)
- [x] 3.4.10 Add post sharing (generate shareable link) - Copies https://sidechain.live/post/{id} to clipboard

---

## Phase 4: User Profiles
**Goal**: Complete user identity and discovery
**Duration**: 1.5 weeks

### 4.1 Profile Data Model

> **Architecture Decision**: Follower/following counts and likes are stored in Stream.io (source of truth).
> PostgreSQL stores only user profile metadata (bio, location, links). This avoids data sync issues.

- [x] 4.1.1 Extend User model: bio, location, genres, social_links (PostgreSQL)
- [x] 4.1.2 Add profile_picture_url field (S3 URL in PostgreSQL)
- [x] 4.1.3 Implement GetFollowStats() in Stream client (follower_count, following_count from Stream.io)
- [x] 4.1.4 Implement GetEnrichedActivities() with reaction counts (likes from Stream.io)
- [x] 4.1.5 Create migration for profile fields (bio, location, genres, social_links, profile_picture_url)
- [x] 4.1.6 Implement GET /api/users/:id/profile endpoint (merges PostgreSQL + Stream.io data)
- [x] 4.1.7 Implement PUT /api/users/profile endpoint (updates PostgreSQL profile fields)
- [ ] 4.1.8 Add username change with uniqueness check
- [ ] 4.1.9 Implement profile picture upload and crop
- [ ] 4.1.10 Add profile verification system (future: badges)

### 4.2 Profile UI (Plugin)

- [ ] 4.2.1 Design profile view layout
- [ ] 4.2.2 Display profile header (avatar, name, bio)
- [ ] 4.2.3 Show follower/following counts (tappable)
- [ ] 4.2.4 Display user's posts in grid/list
- [ ] 4.2.5 Add follow/unfollow button
- [ ] 4.2.6 Implement edit profile modal (own profile)
- [ ] 4.2.7 Add social links display (Instagram, SoundCloud, etc.)
- [ ] 4.2.8 Show genre tags
- [ ] 4.2.9 Display "member since" date
- [ ] 4.2.10 Add profile sharing (copy link)

### 4.3 Follow System

> **Architecture Decision**: Stream.io is the source of truth for follows.
> No PostgreSQL Follow table needed - Stream.io's feed follow system handles everything.

- [x] 4.3.1 ~~Create Follow model~~ → Using Stream.io feed follows (no PostgreSQL table)
- [x] 4.3.2 Add follow/unfollow endpoints (stream/client.go:FollowUser/UnfollowUser - already done!)
- [x] 4.3.3 Implement GET /api/users/:id/followers endpoint (paginated, via Stream.io GetFollowers)
- [x] 4.3.4 Implement GET /api/users/:id/following endpoint (paginated, via Stream.io GetFollowing)
- [x] 4.3.5 ~~Update Stream.io feed subscriptions on follow~~ → Automatic (Stream.io handles this)
- [ ] 4.3.6 Add "suggested users to follow" endpoint
- [x] 4.3.7 Implement mutual follow detection ("follows you") via CheckIsFollowing()
- [ ] 4.3.8 Add follow notifications (Phase 5)
- [ ] 4.3.9 Implement bulk follow import (future: from SoundCloud)

### 4.4 User Discovery

- [ ] 4.4.1 Implement user search endpoint (by username)
- [ ] 4.4.2 Add search UI in plugin (search bar)
- [ ] 4.4.3 Show recent searches
- [ ] 4.4.4 Implement trending users algorithm
- [ ] 4.4.5 Add "featured producers" section
- [ ] 4.4.6 Implement genre-based user discovery
- [ ] 4.4.7 Add "producers you might like" recommendations
- [ ] 4.4.8 Show users with similar BPM/key preferences

---

## Phase 5: Real-time Features
**Goal**: Live updates and presence
**Duration**: 2 weeks

### 5.1 WebSocket Infrastructure

- [ ] 5.1.1 Choose WebSocket library (gorilla/websocket)
- [ ] 5.1.2 Create WebSocket server handler
- [ ] 5.1.3 Implement connection authentication (JWT in query param)
- [ ] 5.1.4 Create connection manager (track active connections)
- [ ] 5.1.5 Implement heartbeat/ping-pong for connection health
- [ ] 5.1.6 Handle reconnection on disconnect
- [ ] 5.1.7 Implement message routing (by type)
- [ ] 5.1.8 Add rate limiting per connection
- [ ] 5.1.9 Implement graceful shutdown (drain connections)
- [ ] 5.1.10 Add WebSocket metrics (connections, messages/sec)

### 5.2 Plugin WebSocket Client

- [ ] 5.2.1 Implement WebSocket client in C++ (JUCE or beast)
- [ ] 5.2.2 Connect on plugin load (after auth)
- [ ] 5.2.3 Implement reconnection with exponential backoff
- [ ] 5.2.4 Parse incoming JSON messages
- [ ] 5.2.5 Route messages to appropriate handlers
- [ ] 5.2.6 Send heartbeat messages
- [ ] 5.2.7 Handle connection state UI (connected/disconnected)
- [ ] 5.2.8 Queue messages when disconnected, send on reconnect

### 5.3 Presence System

- [ ] 5.3.1 Track user online/offline status
- [ ] 5.3.2 Track "in studio" status (plugin open in DAW)
- [ ] 5.3.3 Broadcast presence to followers
- [ ] 5.3.4 Show online indicator on avatars (green dot)
- [ ] 5.3.5 Add "X friends in studio" indicator
- [ ] 5.3.6 Implement presence persistence (Redis or in-memory)
- [ ] 5.3.7 Handle presence timeout (5 minutes no heartbeat = offline)
- [ ] 5.3.8 Add DAW detection (show which DAW user is using)

### 5.4 Live Notifications

- [ ] 5.4.1 Create notification types enum (like, follow, comment, etc.)
- [ ] 5.4.2 Implement notification creation on events
- [ ] 5.4.3 Store notifications in database
- [ ] 5.4.4 Push notifications via WebSocket
- [ ] 5.4.5 Create notification bell UI with unread count
- [ ] 5.4.6 Implement notification list view
- [ ] 5.4.7 Add notification grouping (X users liked your post)
- [ ] 5.4.8 Implement mark as read
- [ ] 5.4.9 Add notification preferences (mute types)
- [ ] 5.4.10 Implement notification sound option

### 5.5 Real-time Feed Updates

- [ ] 5.5.1 Push new posts to followers via WebSocket
- [ ] 5.5.2 Show "new posts" toast in feed
- [ ] 5.5.3 Update like counts in real-time
- [ ] 5.5.4 Update follower counts in real-time
- [ ] 5.5.5 Implement optimistic updates (instant UI, confirm later)
- [ ] 5.5.6 Handle conflicts (stale data resolution)

---

## Phase 6: Comments & Community
**Goal**: Enable conversation around posts
**Duration**: 1.5 weeks

### 6.1 Comment System Backend

- [ ] 6.1.1 Create Comment model (id, post_id, user_id, content, created_at)
- [ ] 6.1.2 Add parent_id for threaded replies
- [ ] 6.1.3 Create POST /posts/{id}/comments endpoint
- [ ] 6.1.4 Create GET /posts/{id}/comments endpoint (paginated)
- [ ] 6.1.5 Implement comment editing (within 5 minutes)
- [ ] 6.1.6 Implement comment deletion
- [ ] 6.1.7 Add comment likes
- [ ] 6.1.8 Implement @mentions with notification
- [ ] 6.1.9 Add comment count to post response
- [ ] 6.1.10 Index comments for search

### 6.2 Comment UI

- [ ] 6.2.1 Design comment section (expandable from post)
- [ ] 6.2.2 Implement comment list rendering
- [ ] 6.2.3 Add comment input field
- [ ] 6.2.4 Show user avatar and username per comment
- [ ] 6.2.5 Display relative timestamps ("2h ago")
- [ ] 6.2.6 Implement reply threading (1 level deep)
- [ ] 6.2.7 Add comment actions menu (edit, delete, report)
- [ ] 6.2.8 Implement @mention autocomplete
- [ ] 6.2.9 Add emoji picker for comments
- [ ] 6.2.10 Show "typing" indicator (future)

### 6.3 Content Moderation

- [ ] 6.3.1 Implement post reporting endpoint
- [ ] 6.3.2 Implement comment reporting endpoint
- [ ] 6.3.3 Implement user reporting endpoint
- [ ] 6.3.4 Create Report model (reporter, target, reason, status)
- [ ] 6.3.5 Add report reasons enum (spam, harassment, copyright, etc.)
- [ ] 6.3.6 Implement user blocking
- [ ] 6.3.7 Hide content from blocked users
- [ ] 6.3.8 Add profanity filter (basic word list)
- [ ] 6.3.9 Create moderation admin endpoint (future: admin panel)
- [ ] 6.3.10 Implement content takedown flow

---

## Phase 7: Search & Discovery
**Goal**: Help users find great content
**Duration**: 1.5 weeks

### 7.1 Search Infrastructure

- [ ] 7.1.1 Evaluate search options (PostgreSQL full-text vs Elasticsearch)
- [ ] 7.1.2 Implement basic search endpoint
- [ ] 7.1.3 Search users by username
- [ ] 7.1.4 Search posts by title/description
- [ ] 7.1.5 Add genre filtering
- [ ] 7.1.6 Add BPM range filtering
- [ ] 7.1.7 Add key filtering
- [ ] 7.1.8 Implement search result ranking
- [ ] 7.1.9 Add search analytics (track queries)
- [ ] 7.1.10 Implement search suggestions (autocomplete)

### 7.2 Discovery Features

- [ ] 7.2.1 Implement trending posts algorithm
  - [ ] 7.2.1.1 Factor in plays, likes, comments
  - [ ] 7.2.1.2 Apply time decay (recent posts weighted higher)
  - [ ] 7.2.1.3 Consider velocity (rapid engagement)
- [ ] 7.2.2 Create trending endpoint (hourly/daily/weekly)
- [ ] 7.2.3 Implement genre-based feeds (electronic, hip-hop, etc.)
- [ ] 7.2.4 Add "For You" personalized feed
  - [ ] 7.2.4.1 Track user preferences (listened genres, BPMs)
  - [ ] 7.2.4.2 Recommend similar content
- [ ] 7.2.5 Implement hashtag system
- [ ] 7.2.6 Add hashtag search and pages
- [ ] 7.2.7 Create "New & Notable" curated section
- [ ] 7.2.8 Implement "Similar to this" recommendations

### 7.3 Search UI

- [ ] 7.3.1 Add search icon to navigation
- [ ] 7.3.2 Create search screen with input field
- [ ] 7.3.3 Show search results (users + posts tabs)
- [ ] 7.3.4 Add filter controls (genre, BPM, key)
- [ ] 7.3.5 Implement recent searches
- [ ] 7.3.6 Add trending searches section
- [ ] 7.3.7 Show "no results" state with suggestions
- [ ] 7.3.8 Implement keyboard navigation

---

## Phase 8: Polish & Performance
**Goal**: Production-ready quality
**Duration**: 2 weeks

### 8.1 Performance Optimization

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

### 8.4 Backend Hardening

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
- [ ] F.1.1 MIDI pattern capture from DAW
- [ ] F.1.2 MIDI visualization (piano roll)
- [ ] F.1.3 MIDI pattern sharing alongside audio
- [ ] F.1.4 "Drop MIDI to track" feature
- [ ] F.1.5 MIDI battle royale competitions

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
| Phase 7 | 1.5 weeks | Search & Discovery |
| Phase 8 | 2 weeks | Polish & Performance |
| Phase 9 | 1.5 weeks | Infrastructure & DevOps |
| Phase 10 | 2 weeks | Launch Preparation |
| Phase 11 | 4 weeks | Beta Launch |
| Phase 12 | Ongoing | Public Launch |

**Total Estimated Time to Launch**: ~6 months (solo developer)

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
| Stream.io rate limits | Cache aggressively, implement backoff |
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
