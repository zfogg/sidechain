# Sidechain CLI App - Requirements & Implementation Plan

## Project Overview

A comprehensive CLI application for Sidechain that serves both **end-users** and **administrators/moderators**. The CLI enables interaction with the Sidechain social music platform directly from the terminal.

**Key Dependencies:**
- `alexflint/go-arg` - CLI argument parsing
- `resty/go-resty` - HTTP client for API requests
- `coder/websocket` - WebSocket for real-time updates
- `spf13/cobra` - CLI framework (command structure)
- `charmbracelet/bubbletea` - Interactive TUI components
- `spf13/viper` - Configuration management
- `charmbracelet/lipgloss` - Terminal styling
- `charmbracelet/log` - Structured logging
- `fatih/color` - Color output
- `schollz/progressbar` - Progress indicators
- `samber/lo` - Functional utilities
- `sourcegraph/conc` - Concurrency helpers
- `json-iterator/go` - Fast JSON parsing

---

## Phase 1: Core Project Setup & Authentication

### 1.1 Project Structure & Configuration
- [ ] Create main CLI structure (cmd/, pkg/, internal/ directories)
- [ ] Setup Cobra command structure with root command
- [ ] Implement Viper configuration management (.sidechain/config.yaml, .sidechain/credentials)
- [ ] Create config loader with home directory support (~/.sidechain/)
- [ ] Implement credential storage (API tokens, refresh tokens, user profile)
- [ ] Create logger with structured logging and log levels
- [ ] Add dependency injection container for shared services
- [ ] Create HTTP client wrapper with auth headers and error handling
- [ ] Setup global context management for authenticated requests

### 1.2 Authentication Commands
- [ ] `auth login` - Authenticate with email/password or OAuth
  - Interactive prompt for email/password
  - OAuth polling integration (wait for web browser completion)
  - Store JWT token and refresh token
  - Verify server connection
  - Display success with user profile summary
- [ ] `auth logout` - Clear stored credentials
  - Remove tokens from config
  - Confirm logout
- [ ] `auth me` - Display current authenticated user
  - Show username, display name, bio, follower count
  - Show email and verification status
  - Show 2FA status
- [ ] `auth 2fa enable` - Enable two-factor authentication
  - Request TOTP setup
  - Display QR code in terminal
  - Prompt for test code verification
  - Display backup codes
- [ ] `auth 2fa disable` - Disable two-factor authentication
  - Require current 2FA code
  - Confirm disablement
- [ ] `auth refresh` - Refresh authentication token
  - Automatic refresh on expiry (background)
  - Manual refresh command
  - Error handling for expired/revoked tokens
- [ ] `auth 2fa backup-codes` - Regenerate backup codes
  - Display new backup codes
  - Warn about invalidating old codes

### 1.3 Profile Management
- [ ] `profile view [username]` - View user profile
  - Default: current user if no username provided
  - Display: username, display name, bio, location, DAW preference, genres
  - Show: follower count, following count, post count
  - Show: online status if settings allow
  - Show: follow status (following/follow request pending/not following)
- [ ] `profile edit` - Edit current user profile
  - Interactive prompts for: display name, bio, location, DAW preference, genres
  - Upload profile picture (--picture flag)
  - Validate input constraints
  - Show confirmation before saving
- [ ] `profile picture upload` - Upload profile picture
  - Accept file path argument
  - Validate image format (PNG, JPEG)
  - Show upload progress
  - Display new picture URL
- [ ] `profile follow <username>` - Follow/request follow
  - Handle private accounts (follow request)
  - Handle public accounts (immediate follow)
  - Display follow status
- [ ] `profile unfollow <username>` - Unfollow user
  - Confirm action
- [ ] `profile mute <username>` - Mute user
  - Hide their posts without unfollowing
  - Confirm action
- [ ] `profile unmute <username>` - Unmute user
  - Confirm action
- [ ] `profile list-muted` - List muted users
  - Show count
  - Show usernames
- [ ] `profile activity <username>` - View user activity summary
  - Recent posts, follows, likes, comments
- [ ] `profile followers <username>` - List followers
  - Pagination support
  - Show follow status for each
- [ ] `profile following <username>` - List following list
  - Pagination support

---

## Phase 2: User Features - Audio & Posts

### 2.1 Audio Upload & Post Creation
- [ ] `post upload` - Upload audio and create post
  - Accept audio file path argument
  - Display file validation (format, size, duration)
  - Extract metadata (BPM, key, DAW if available)
  - Optional: set genre, description (tags)
  - Optional: remix metadata (--remix-of flag with post ID)
  - Show upload progress with speed and ETA
  - Show processing status (pending/processing/complete)
  - Poll for processing completion with status updates
  - Display final post URL and sharing options
- [ ] `post list` - List user's own posts
  - Show: title, duration, date, play count, like count
  - Pagination (default 20 per page)
  - Sort options: newest, oldest, trending
  - Show post status (public/archived/draft)
- [ ] `post view <post-id>` - View post details
  - Show full metadata: creator, duration, BPM, key, genre, DAW
  - Show engagement: likes, plays, downloads, comments, reposts
  - Show remix chain if applicable
  - Show comment count
  - Show save/repost/like status (if user is authenticated)
  - Option to play/download (--play, --download flags)

### 2.2 Audio Playback & Download
- [ ] `audio play <post-id>` - Stream audio post
  - Play in system audio player (open in default player)
  - Track play event
  - Display progress bar (if TUI mode)
- [ ] `audio download <post-id>` - Download audio file
  - Accept optional output filename (--output flag)
  - Show download progress
  - Track download event
  - Save to file or pipe to stdout (--stdout flag)
  - Default download location: ~/Music or configurable

### 2.3 Post Management
- [ ] `post delete <post-id>` - Delete own post
  - Confirm deletion
  - Show if post has reposts/quotes (warn user)
  - Soft delete on server
- [ ] `post archive <post-id>` - Archive post
  - Hide from feed without deletion
  - Can unarchive later
- [ ] `post unarchive <post-id>` - Unarchive post
- [ ] `post save <post-id>` - Save post (bookmark)
  - Add to personal collection
- [ ] `post unsave <post-id>` - Remove from saved
- [ ] `post saved-list` - List all saved posts
  - Show: creator, title, date saved
  - Pagination support
- [ ] `post pin <post-id>` - Pin post to profile (max 3)
  - Confirm action
  - Show current pinned posts
- [ ] `post unpin <post-id>` - Unpin post
- [ ] `post repost <post-id>` - Repost with optional quote
  - Optional: add quote comment (--quote flag)
  - Confirm action
  - Show in own feed immediately
- [ ] `post unrepost <post-id>` - Undo repost
- [ ] `post set-comment-audience <post-id>` - Control who can comment
  - Options: everyone (default), followers-only, disabled
  - Confirm change

### 2.4 Post Engagement
- [ ] `post like <post-id>` - Like a post
  - Confirm action
  - Show updated like count
- [ ] `post unlike <post-id>` - Unlike a post
  - Confirm action
- [ ] `post react <post-id>` - Add emoji reaction
  - Show emoji picker (if TUI mode available)
  - Accept emoji argument (e.g., "🔥", "❤️")
  - Show reaction count
- [ ] `post comment <post-id>` - Add comment
  - Interactive text editor for comment
  - Show comment size limit (2000 chars)
  - Confirm before posting
  - Display created comment with ID
- [ ] `post comments <post-id>` - List comments on post
  - Show: author, date, comment text, like count
  - Show threading (replies to comments)
  - Pagination support
  - Show edit window info (5-minute window)
- [ ] `comment edit <comment-id>` - Edit own comment
  - Show edit window remaining time
  - Prevent if outside 5-minute window
  - Text editor for editing
  - Confirm changes
- [ ] `comment delete <comment-id>` - Delete own comment
  - Replace with "[Comment deleted]" on server
  - Confirm action
- [ ] `comment like <comment-id>` - Like comment
- [ ] `comment unlike <comment-id>` - Unlike comment

### 2.5 Stories (Ephemeral Content - 24h)
- [ ] `story upload` - Create story (15-60 seconds)
  - Accept audio file path
  - Validate duration (15-60 seconds)
  - Optional: add MIDI pattern (--midi flag)
  - Show upload and processing
  - Display story expiry time (24 hours from now)
- [ ] `story list` - List own stories
  - Show: date created, view count, time until expiry
  - Sorted by newest first
- [ ] `story view <story-id>` - View story details
  - Show creator, view count, viewers list (if owner)
  - Show expiry time
- [ ] `story delete <story-id>` - Delete story
  - Confirm action
- [ ] `story remix <story-id>` - Create post from story
  - Story becomes part of remix chain
  - Create new post with remix metadata

### 2.6 MIDI Patterns & Files
- [ ] `midi upload` - Upload standalone MIDI pattern
  - Accept MIDI file path
  - Set name, description
  - Set key, BPM, genre
  - Optional: make public (default private)
  - Display MIDI details
- [ ] `midi list` - List own MIDI patterns
  - Show: name, filename, key, BPM, public/private
  - Pagination support
- [ ] `midi view <midi-id>` - View MIDI pattern
  - Show: full metadata, download count
  - Show usage stats
- [ ] `midi download <midi-id>` - Download MIDI file
  - Save to file with optional --output flag
  - Show download progress
- [ ] `midi delete <midi-id>` - Delete MIDI pattern
  - Confirm action
- [ ] `project-file upload` - Upload DAW project file
  - Accept .als, .flp, .aup3, etc.
  - Associate with post (--post-id flag)
  - Show upload progress
- [ ] `project-file download <file-id>` - Download project file
  - Accept output path (--output flag)
  - Show download progress

---

## Phase 3: User Features - Social & Discovery

### 3.1 Feed Management
- [ ] `feed timeline` - View timeline (posts from followed users)
  - Pagination (default 20 posts per page)
  - Options: sort by newest/oldest
  - Rich formatting with post metadata
  - TUI mode: interactive browsing
- [ ] `feed global` - View global feed (all public posts)
  - Pagination
  - Sorting options
- [ ] `feed trending` - View trending posts
  - Show: trending score, creator, stats
  - Pagination
  - Sorted by trending rank
- [ ] `feed for-you` - View personalized recommendations
  - Gorse-powered recommendations
  - Pagination
  - Track: views, clicks, dislikes for recommendation improvement
- [ ] `feed search` - Search posts and sounds
  - Accept search query argument
  - Filter by: genre, BPM, key, DAW, date range
  - Pagination
  - Show: results count

### 3.2 Sound Discovery
- [ ] `sound search` - Search for sounds/samples
  - Accept search query
  - Show: sound name, creator, usage count, trending rank
  - Pagination
- [ ] `sound trending` - View trending sounds
  - Show: rank, name, usage count, creator
  - Pagination
- [ ] `sound info <sound-id>` - View sound details
  - Show: metadata, creator, usage count, posts using it
- [ ] `sound posts <sound-id>` - List posts using specific sound
  - Show: creator, date, stats
  - Pagination

### 3.3 User Discovery & Recommendations
- [ ] `discover trending-users` - View trending producers
  - Show: username, follower count, recent post count
  - Pagination
- [ ] `discover featured` - View featured producers
  - Show: username, bio, follower count
  - Pagination
- [ ] `discover suggested` - View suggested users based on taste
  - Show: username, reason (similar to X, popular in Y genre)
  - Pagination
- [ ] `discover by-genre <genre>` - Browse users by music genre
  - Show available genres (cache locally)
  - Show: username, genre focus, follower count
  - Pagination
- [ ] `discover recommendations <user-id>` - View recommended users similar to another user
  - Show: username, similarity score
  - Pagination

### 3.4 Follow Management
- [ ] `follow list` - List people you follow
  - Show: username, follower count, is following back
  - Pagination
  - Sort options: alphabetical, recently followed
- [ ] `followers list [username]` - List followers
  - Default: current user if no username
  - Show: username, follower count, follow back status
  - Pagination
- [ ] `follow-request list` - List pending follow requests
  - Show: username, request date
  - Count displayed
- [ ] `follow-request accept <username>` - Accept follow request
  - Confirm action
- [ ] `follow-request reject <username>` - Reject follow request
  - Confirm action
- [ ] `follow-request cancel` - Cancel pending follow request sent by you
  - Confirm action

### 3.5 Notifications
- [ ] `notifications list` - Display notifications
  - Show: type (like/comment/follow/mention), author, post/context, date
  - Pagination (default 20)
  - Show unread count
  - Mark as read/seen
- [ ] `notifications watch` - Watch for real-time notifications
  - WebSocket connection
  - Stream notifications as they arrive
  - Display with color coding by type
  - Option to dismiss individual notifications
- [ ] `notifications count` - Show unread notification count
- [ ] `notifications mark-read` - Mark all as read
  - Or mark specific notification as read
- [ ] `notifications preferences` - View/manage notification settings
  - Show: which notification types are enabled
  - Enable/disable individual types (likes, comments, follows, etc.)

---

## Phase 4: Moderation & Admin Features

### 4.1 Content Reporting
- [ ] `report post <post-id>` - Report post for violation
  - Show reason options: spam, harassment, copyright, inappropriate, violence, other
  - Accept reason argument
  - Optional: additional description (--reason flag)
  - Confirm submission
  - Display report receipt/ID
- [ ] `report comment <comment-id>` - Report comment
  - Show reason options
  - Accept reason argument
  - Optional: description
  - Confirm submission
- [ ] `report user <username>` - Report user
  - Show reason options
  - Accept reason argument
  - Optional: description
  - Confirm submission

### 4.2 Admin: Report Management (Admin-only)
- [ ] `admin reports list` - List all reports
  - Filter by: status (pending/reviewing/resolved/dismissed), type (post/comment/user), reason
  - Show: reporter, target, reason, submission date, status
  - Pagination
  - Sort by: newest, oldest, status
- [ ] `admin reports view <report-id>` - View specific report
  - Show: full details, evidence (post/comment content)
  - Show: reporter info, target info
  - Show: current status and any moderator notes
  - Show: actions available
- [ ] `admin reports assign` - Assign report to moderator
  - Show available moderators
  - Assign to self or other moderator
  - Update status to "reviewing"
- [ ] `admin reports resolve <report-id>` - Mark report as resolved
  - Accept action taken (--action flag)
  - Options: deleted, no-action, warned-user, suspended-user
  - Add moderator notes (--notes flag)
  - Confirm resolution
- [ ] `admin reports dismiss <report-id>` - Dismiss report as unfounded
  - Add moderator notes (--notes flag)
  - Confirm dismissal

### 4.3 Admin: Post Management
- [ ] `admin posts delete <post-id>` - Delete post
  - Show post details (creator, content)
  - Confirm deletion (permanent)
  - Record reason (--reason flag)
  - Notify creator (--notify flag, default yes)
- [ ] `admin posts hide <post-id>` - Hide post from feeds temporarily
  - Set duration (--duration flag, default 24h)
  - Add reason (--reason flag)
  - Notify creator or not
- [ ] `admin posts unhide <post-id>` - Restore hidden post
  - Confirm action
- [ ] `admin posts list` - List recently flagged/hidden posts
  - Filter by: status (visible/hidden/deleted), reason
  - Show: creator, report reason, current status
  - Pagination
- [ ] `admin posts view <post-id>` - View post with full context
  - Show: content, creator, reports made
  - Show: any hidden/deleted status
  - Show: actions taken

### 4.4 Admin: User Management
- [ ] `admin users list` - List users
  - Filter by: verified status, 2FA enabled, account age, join date range
  - Show: username, email, join date, post count, follower count
  - Pagination
  - Sort: join date, username, post count
- [ ] `admin users view <username>` - View user details
  - Show: email, join date, verification status
  - Show: 2FA status, last active
  - Show: post count, follower count, following count
  - Show: reports made against user
  - Show: actions taken
- [ ] `admin users suspend <username>` - Suspend user account
  - Duration (--duration flag)
  - Reason (--reason flag)
  - Notify user (--notify flag, default yes)
  - Confirm action
- [ ] `admin users unsuspend <username>` - Lift suspension
  - Confirm action
- [ ] `admin users warn <username>` - Issue warning to user
  - Reason (--reason flag)
  - Notify user (--notify flag, default yes)
- [ ] `admin users email-verify <username>` - Manually verify user email
  - Confirm action
- [ ] `admin users reset-password <username>` - Force password reset
  - Generate reset token
  - Send reset email (--send-email flag)
  - Display reset link if --show-link flag

### 4.5 Admin: Comment Moderation
- [ ] `admin comments delete <comment-id>` - Delete comment
  - Show comment content
  - Confirm deletion
  - Notify comment author (--notify flag)
- [ ] `admin comments hide <comment-id>` - Hide comment temporarily
  - Set duration (--duration flag)
  - Reason (--reason flag)
- [ ] `admin comments list` - List reported/flagged comments
  - Filter by: status, reason
  - Show: author, comment preview, report reason
  - Pagination

### 4.6 Admin: Moderation Dashboard
- [ ] `admin dashboard` - View moderation overview
  - Show: pending reports count, recent violations
  - Show: suspension count, active suspensions
  - Show: reported users/posts/comments (trending violations)
  - Show: moderator activity stats
- [ ] `admin stats` - View platform statistics
  - Show: total users, total posts, active users (24h)
  - Show: verification rate, 2FA adoption
  - Show: average post engagement
  - Show: content reports volume (by reason)
  - Show: suspension/warning stats

### 4.7 Admin: MIDI Challenges
- [ ] `admin challenges create` - Create MIDI challenge/battle
  - Set: name, description, theme
  - Set: start time, end time, max entries
  - Set: voting enabled (yes/no)
  - Display created challenge details
- [ ] `admin challenges list` - List all challenges
  - Show: name, status (active/upcoming/ended), entry count, voter count
  - Pagination
  - Filter by: status, date range
- [ ] `admin challenges edit <challenge-id>` - Edit challenge
  - Change: name, description, theme, dates, settings
  - Confirm changes
- [ ] `admin challenges delete <challenge-id>` - Delete challenge
  - Confirm action
- [ ] `admin challenges view-entries <challenge-id>` - View challenge entries
  - Show: creator, submission date, vote count
  - Pagination
  - Sort: votes, newest, oldest

### 4.8 Admin: System Configuration
- [ ] `admin config get` - View system configuration
  - Show: API settings, rate limits, feature flags
  - Show: email settings, OAuth keys status
- [ ] `admin config set <key> <value>` - Update configuration
  - Validate changes
  - Show before/after
  - Confirm change
- [ ] `admin logs stream` - Stream server logs
  - Filter by: level (error/warn/info), service
  - Real-time log streaming via WebSocket
- [ ] `admin health` - Check server health
  - Show: API status, database status, external services status
  - Show: response times, connection pool stats

---

## Phase 5: Advanced Features & Optimization

### 5.1 MIDI & Project Management
- [ ] `remix list <post-id>` - List posts that remixed a post
  - Show: creator, date, stats
  - Pagination
- [ ] `remix chain <post-id>` - View full remix chain/lineage
  - Show: original → remix1 → remix2, etc.
  - Display in tree format
- [ ] `playlist create` - Create playlist
  - Set: name, description, public/private, collaborative
  - Display created playlist ID
- [ ] `playlist edit <playlist-id>` - Edit playlist
  - Change: name, description, settings
  - Confirm changes
- [ ] `playlist delete <playlist-id>` - Delete playlist
  - Confirm action
- [ ] `playlist add <playlist-id> <post-id>` - Add post to playlist
  - Confirm action
  - Show position in playlist
- [ ] `playlist remove <playlist-id> <post-id>` - Remove post
  - Confirm action
- [ ] `playlist list` - List your playlists
  - Show: name, count, last modified, public/private
  - Pagination
- [ ] `playlist view <playlist-id>` - View playlist details
  - Show: posts in order, owner, collaborators
  - Pagination

### 5.2 Interactive TUI Mode
- [ ] `tui` - Launch interactive terminal UI
  - Dashboard showing:
    - Timeline feed (scrollable, up/down)
    - Notification panel (right sidebar)
    - Status bar (current user, connection status)
  - Navigation: feed, profile, discover, search
  - Keybindings: j/k (scroll), l (like), c (comment), r (repost), q (quit)
  - Real-time updates from WebSocket
  - Fuzzy find for users/posts

### 5.3 Batch Operations
- [ ] `batch upload-folder` - Upload all audio files in directory
  - Recursive directory scan
  - Skip existing files
  - Show: progress bar, upload speed, ETA
  - Collect tags for all (--tags flag)
  - Display results (successes/failures)
- [ ] `batch export-archive` - Export all posts/metadata as archive
  - Format: JSON or CSV
  - Include: audio files, metadata, comments
  - Show: export progress
  - Create ZIP archive
- [ ] `batch delete` - Delete multiple posts
  - Accept list of post IDs
  - Confirm bulk deletion
  - Show progress

### 5.4 Export & Data Tools
- [ ] `export profile` - Export user profile data
  - Format: JSON
  - Include: profile info, all posts metadata, followers/following
  - Save to file (--output flag)
  - Download: all audio files (optional, --include-audio flag)
- [ ] `export posts` - Export posts as JSON/CSV
  - Optional: include audio files
  - Optional: date range (--from, --to flags)
  - Create ZIP if including audio
- [ ] `export statistics` - Generate personal statistics
  - Format: JSON/CSV
  - Show: total posts, average engagement, top posts
  - Show: genre distribution, DAW breakdown
  - Show: growth over time
- [ ] `import from-backup` - Restore from backup JSON
  - Accept backup file path
  - Validate integrity
  - Confirm import (preview changes)
  - Show: import progress

### 5.5 Search & Filtering
- [ ] `search advanced` - Advanced search with filters
  - Query syntax: by:creator genre:house bpm:128+
  - Filter by: creator, genre, BPM range, key, DAW, date range
  - Sort options
  - Display results with rich formatting
- [ ] `search recent` - Recent searches (locally cached)
  - Show: previous searches, frequency
  - Quick re-run search

### 5.6 Caching & Offline Support
- [ ] `cache clear` - Clear local cache
  - Cache includes: feed posts, user profiles, search results
  - Show: space freed
- [ ] `cache status` - Show cache status
  - Show: cache size, age of cached items
  - Show: what's cached
- [ ] `sync` - Sync local data with server
  - Upload: drafts, pending posts
  - Download: feed, new messages
  - Handle conflicts

### 5.7 Configuration & Settings
- [ ] `config show` - Display current configuration
  - Show: API endpoint, download location, notification settings
  - Show: theme, log level
- [ ] `config edit` - Edit configuration interactively
  - Text editor for config file
  - Validate changes before save
- [ ] `config reset` - Reset to defaults
  - Confirm action
- [ ] `theme list` - List available themes
  - Show: theme names
- [ ] `theme set <theme-name>` - Set color theme
  - Preview theme
  - Confirm change
- [ ] `log-level set <level>` - Set logging level
  - Options: debug, info, warn, error
  - Apply immediately

### 5.8 Help & Documentation
- [ ] `help <command>` - Get help for specific command
  - Show: command description, flags, examples
  - Show: related commands
- [ ] `docs` - Open documentation (in browser)
  - Links to: API docs, feature guides, troubleshooting
- [ ] `version` - Show CLI version
  - Show: build info, dependencies

---

## Phase 6: Backend Integration & Polish

### 6.1 Error Handling & Recovery
- [ ] Comprehensive error messages
  - User-friendly error descriptions
  - Suggested fixes or next steps
  - Error codes for debugging
- [ ] Retry logic
  - Automatic retry for transient failures (network timeouts)
  - Exponential backoff
  - Max retry count
  - User can force retry (--retry flag)
- [ ] Session management
  - Automatic token refresh
  - Handle 401 errors gracefully
  - Logout + prompt to login again
  - Persist refresh tokens for seamless resumption

### 6.2 Performance & Optimization
- [ ] Request batching
  - Batch multiple API calls to reduce latency
  - Automatic batching for list operations
- [ ] Pagination optimization
  - Smart prefetching (load next page in background)
  - Cursor-based pagination
  - Cache consecutive pages locally
- [ ] Image/media handling
  - Lazy load images (only when needed)
  - Cache images locally
  - Progress tracking for downloads

### 6.3 Testing & Quality Assurance
- [ ] Unit tests
  - Config management
  - Authentication
  - HTTP client
  - Command parsing
- [ ] Integration tests
  - API integration tests (with test server)
  - WebSocket functionality
  - File upload/download
- [ ] E2E tests
  - Full workflow: auth → upload → feed → comment → delete

### 6.4 Documentation
- [ ] README with:
  - Installation instructions
  - Quick start guide
  - Feature overview
  - Troubleshooting
- [ ] Command reference documentation
  - Each command with examples
  - Flag documentation
  - Common use cases
- [ ] Admin guide
  - Moderation workflows
  - Report management best practices
  - User management procedures
- [ ] Developer guide
  - Architecture overview
  - Adding new commands
  - Testing instructions

### 6.5 Release & Distribution
- [ ] Build scripts for multiple platforms
  - Linux (amd64, arm64)
  - macOS (x86_64, arm64)
  - Windows (amd64)
- [ ] Automated testing in CI/CD
  - Run tests on PR
  - Build on push to main
- [ ] Version management
  - Semantic versioning
  - Changelog
  - Release notes
- [ ] Distribution
  - GitHub releases with binaries
  - Package managers (Homebrew, AUR, Chocolatey)

---

## Implementation Notes

### Authentication Flow
1. User runs `sidechain-cli auth login`
2. Prompt for email/password (or "oauth" option)
3. If password login: send to server, receive JWT + refresh token
4. If OAuth: open browser polling endpoint, wait for callback
5. Store tokens in ~/.sidechain/credentials with restricted permissions
6. Subsequent requests include Authorization header with JWT
7. CLI detects expired JWT, automatically refreshes using refresh token
8. If refresh fails, prompt user to login again

### API Request Pattern
1. All requests go through HTTP client wrapper
2. Wrapper adds: Auth header, User-Agent, content type
3. Handle 401 → refresh token → retry logic
4. Wrap errors in user-friendly format
5. Log requests (optional, controlled by log level)
6. Support: retry, timeout, rate limit handling

### Data Persistence
1. Config: ~/.sidechain/config.yaml (settings, preferences)
2. Credentials: ~/.sidechain/credentials (encrypted or with strict file perms)
3. Cache: ~/.sidechain/cache/ (feed, profiles, searches)
4. Logs: ~/.sidechain/logs/ (for debugging)

### Real-time Updates
1. WebSocket connection for notifications
2. Optional: streaming feed (--watch flag)
3. Reconnect logic with exponential backoff
4. Display updates in real-time without polling

### Admin Authentication
1. Same login flow as regular users
2. Backend returns admin role in JWT claims
3. CLI checks role before showing admin commands
4. All admin actions logged for audit trail

---

## Success Criteria

✅ User can authenticate and access their profile
✅ User can upload audio and create posts
✅ User can browse feeds and discover content
✅ User can engage (like, comment, follow)
✅ Admins can view and manage reports
✅ Admins can suspend/warn users
✅ Admins can delete inappropriate content
✅ All interactions are logged and tracked
✅ CLI is responsive and user-friendly
✅ Help/documentation is comprehensive
