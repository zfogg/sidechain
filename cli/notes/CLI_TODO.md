# Sidechain CLI - Itemized Todo List

## PHASE 1: Core Project Setup & Authentication ✅ COMPLETE

### Core Infrastructure
- [x] Create directory structure: cmd/, pkg/, internal/, tests/
- [x] Setup main entry point (cmd/main.go)
- [x] Initialize go.mod with required dependencies
- [x] Create package structure for reusable components

### Configuration Management
- [x] Create config package (viper-based)
- [x] Implement config file loading (~/.sidechain/config.yaml)
- [x] Create credentials storage manager (secure file permissions)
- [x] Add config validation and defaults
- [ ] Implement hot-reload for config changes

### HTTP Client & API Integration
- [x] Create HTTP client wrapper (resty-based)
- [x] Add authentication header middleware
- [x] Implement error handling and response parsing (json-iterator)
- [ ] Add automatic retry logic with exponential backoff
- [x] Implement token refresh logic for JWT expiry
- [x] Add request/response logging

### Command Framework
- [x] Setup Cobra root command
- [x] Create command factory pattern for consistent structure
- [x] Implement command help and usage text
- [x] Add global flags: --verbose, --output, --config-path
- [x] Setup subcommand hierarchy

### Logging & Output
- [x] Setup charmbracelet/log with structured logging
- [x] Create output formatter (text, JSON, table)
- [x] Add color support via fatih/color
- [x] Implement verbosity levels (debug, info, warn, error)
- [x] Create progress bar helpers (schollz/progressbar)

### Authentication Commands
- [x] `auth login` - Email/password login
- [ ] `auth login` - OAuth polling integration
- [x] `auth logout` - Clear credentials
- [x] `auth me` - Display current user info
- [x] `auth refresh` - Manual token refresh
- [x] `auth 2fa enable` - Enable TOTP
- [x] `auth 2fa disable` - Disable TOTP
- [x] `auth 2fa backup-codes` - Regenerate backup codes

### Profile Management
- [x] `profile view [username]` - View user profile
- [x] `profile edit` - Edit current user profile
- [ ] `profile picture upload` - Upload profile picture
- [x] `profile follow <username>` - Follow user
- [x] `profile unfollow <username>` - Unfollow user
- [x] `profile mute <username>` - Mute user
- [x] `profile unmute <username>` - Unmute user
- [x] `profile list-muted` - List muted users
- [x] `profile activity <username>` - View user activity
- [x] `profile followers <username>` - List followers
- [x] `profile following <username>` - List following

---

## PHASE 2: User Features - Audio & Posts 🔄 IN PROGRESS (85%)

### Audio Upload & Post Creation
- [x] `post upload` - Upload audio file and create post
- [ ] Audio file validation (format, size, duration)
- [ ] Extract audio metadata (BPM, key, duration)
- [ ] Upload progress tracking with speed/ETA
- [ ] Async processing status polling
- [x] `post list` - List user's posts
- [x] `post view <post-id>` - View post details
- [x] `post delete <post-id>` - Delete post
- [x] `post archive <post-id>` - Archive post
- [x] `post unarchive <post-id>` - Unarchive post

### Post Engagement
- [x] `post like <post-id>` - Like post
- [x] `post unlike <post-id>` - Unlike post
- [x] `post react <post-id>` - Add emoji reaction
- [x] `post comment <post-id>` - Add comment
- [x] `post comments <post-id>` - List comments
- [x] `post comment-edit <comment-id>` - Edit comment (5-min window)
- [x] `post comment-delete <comment-id>` - Delete comment
- [x] `post comment-like <comment-id>` - Like comment
- [x] `post comment-unlike <comment-id>` - Unlike comment

### Post Management
- [x] `post save <post-id>` - Save/bookmark post
- [x] `post unsave <post-id>` - Remove saved post
- [x] `post saved-list` - List saved posts
- [x] `post pin <post-id>` - Pin post (max 3)
- [x] `post unpin <post-id>` - Unpin post
- [x] `post repost <post-id>` - Repost with optional quote
- [x] `post unrepost <post-id>` - Undo repost
- [ ] `post set-comment-audience <post-id>` - Control comment permissions

### Audio Playback & Download
- [ ] `audio play <post-id>` - Stream/play audio
- [ ] `audio download <post-id>` - Download audio file
- [ ] Download progress tracking
- [ ] Configurable download location
- [ ] Auto-open with default player

### Stories (24-hour Ephemeral)
- [ ] `story upload` - Create 15-60 second story
- [ ] Duration validation
- [ ] Story processing and status tracking
- [ ] `story list` - List own stories
- [ ] `story view <story-id>` - View story details
- [ ] `story delete <story-id>` - Delete story
- [ ] `story remix <story-id>` - Create post from story

### MIDI & Project Files
- [ ] `midi upload` - Upload MIDI pattern
- [ ] `midi list` - List MIDI patterns
- [ ] `midi view <midi-id>` - View MIDI details
- [ ] `midi download <midi-id>` - Download MIDI file
- [ ] `midi delete <midi-id>` - Delete MIDI pattern
- [ ] `project-file upload` - Upload DAW project file
- [ ] `project-file download <file-id>` - Download project file

---

## PHASE 3: Social & Discovery ✅ COMPLETE (98%)

### Feed Management
- [x] `feed timeline` - View timeline (followed users)
- [x] `feed global` - View global feed
- [x] `feed trending` - View trending posts
- [x] `feed for-you` - View personalized recommendations
- [x] `feed search` - Search posts with filters
- [x] Pagination support for all feeds
- [x] Rich formatting and metadata display

### Sound Discovery
- [x] `feed sound search` - Search sounds/samples
- [x] `feed sound trending` - View trending sounds
- [x] `feed sound info` - View sound details
- [x] `feed sound posts` - List posts using sound

### User Discovery
- [x] `feed discover trending-users` - View trending producers
- [x] `feed discover featured` - View featured producers
- [x] `feed discover suggested` - View suggested users
- [x] `feed discover by-genre <genre>` - Browse by genre
- [ ] `discover recommendations` - Similar users to specified user

### Follow Management
- [x] `profile follow <username>` - Follow user (in profile service)
- [x] `profile unfollow <username>` - Unfollow user (in profile service)
- [x] `profile followers [username]` - List user's followers
- [x] `profile following [username]` - List following
- [x] `follow-request list` - List pending follow requests
- [x] `follow-request accept <username>` - Accept follow request
- [x] `follow-request reject <username>` - Reject follow request
- [x] `follow-request cancel <username>` - Cancel sent follow request

### Notifications
- [x] `notifications list` - Display notifications
- [ ] `notifications watch` - Stream real-time notifications (WebSocket)
- [x] `notifications count` - Show unread count
- [x] `notifications mark-read` - Mark as read
- [x] `notifications preferences` - View/manage notification settings

---

## PHASE 4: Moderation & Admin 🔄 IN PROGRESS (80%)

### Content Reporting (All Users)
- [x] `report post <post-id>` - Report post with reason selection and optional description
- [x] `report comment <comment-id>` - Report comment with reason selection and optional description
- [x] `report user <username>` - Report user with reason selection and optional description
- [x] Report reason selection and description (interactive prompts)

### Admin: Report Management
- [x] `admin reports list` - List all reports
- [x] Filtering: status, type, reason
- [ ] Sorting: date, status
- [x] Pagination
- [x] `admin reports view <report-id>` - View report details
- [ ] `admin reports assign <report-id>` - Assign to moderator
- [x] `admin reports resolve <report-id>` - Mark resolved with action (dismiss, remove_content, suspend_user)
- [x] `admin reports dismiss <report-id>` - Dismiss report

### Admin: Post Moderation
- [x] `admin posts delete <post-id>` - Delete post (admin only)
- [x] `admin posts hide <post-id>` - Hide post temporarily (admin only)
- [x] `admin posts unhide <post-id>` - Restore hidden post (admin only)
- [x] `admin posts list` - List flagged posts with pagination
- [x] `admin posts view <post-id>` - View post details with metadata

### Admin: Comment Moderation
- [x] `admin comments delete <comment-id>` - Delete comment (admin only)
- [x] `admin comments hide <comment-id>` - Hide comment (admin only)
- [x] `admin comments list` - List flagged comments with pagination

### Admin: User Management
- [ ] `admin users list` - List all users
- [ ] Filtering: verified, 2FA, join date
- [ ] Sorting: join date, username, post count
- [ ] `admin users view <username>` - View user details
- [ ] `admin users suspend <username>` - Suspend account
- [ ] `admin users unsuspend <username>` - Lift suspension
- [ ] `admin users warn <username>` - Issue warning
- [ ] `admin users email-verify <username>` - Verify email manually
- [ ] `admin users reset-password <username>` - Force password reset

### Admin: MIDI Challenges
- [ ] `admin challenges create` - Create challenge/battle
- [ ] `admin challenges list` - List challenges
- [ ] `admin challenges edit <challenge-id>` - Edit challenge
- [ ] `admin challenges delete <challenge-id>` - Delete challenge
- [ ] `admin challenges view-entries <challenge-id>` - View entries

### Admin: Dashboard & Stats
- [ ] `admin dashboard` - Overview of moderation activity
- [ ] Pending reports count, recent violations
- [ ] `admin stats` - Platform statistics
- [ ] User counts, post counts, engagement stats
- [ ] `admin logs stream` - Stream server logs
- [ ] `admin health` - Check server/service health
- [ ] `admin config get` - View configuration
- [ ] `admin config set <key> <value>` - Update configuration

---

## PHASE 5: Advanced Features

### Remix & Project Management
- [ ] `remix list <post-id>` - List posts that remixed
- [ ] `remix chain <post-id>` - View remix lineage
- [ ] `playlist create` - Create playlist
- [ ] `playlist edit <playlist-id>` - Edit playlist
- [ ] `playlist delete <playlist-id>` - Delete playlist
- [ ] `playlist add <playlist-id> <post-id>` - Add post
- [ ] `playlist remove <playlist-id> <post-id>` - Remove post
- [ ] `playlist list` - List your playlists
- [ ] `playlist view <playlist-id>` - View playlist details

### Interactive TUI Mode
- [ ] `tui` - Launch interactive terminal UI
- [ ] Dashboard layout (feed, notifications, status bar)
- [ ] Navigation: feed, profile, discover, search
- [ ] Keybindings: j/k scroll, l like, c comment, r repost, q quit
- [ ] Real-time updates from WebSocket
- [ ] Fuzzy find for users/posts
- [ ] Use bubbletea for interactive components

### Batch Operations
- [ ] `batch upload-folder` - Upload directory of audio files
- [ ] Directory scanning and progress
- [ ] `batch export-archive` - Export posts as archive
- [ ] JSON/CSV format support
- [ ] `batch delete` - Delete multiple posts

### Export & Data Tools
- [ ] `export profile` - Export profile data as JSON
- [ ] `export posts` - Export posts with optional audio
- [ ] `export statistics` - Generate personal statistics
- [ ] `import from-backup` - Restore from backup JSON
- [ ] ZIP archive creation for large exports

### Advanced Search
- [ ] `search advanced` - Advanced search with filters
- [ ] Query syntax: by:creator, genre:house, bpm:128+
- [ ] `search recent` - Show previous searches

### Caching & Offline
- [ ] `cache clear` - Clear local cache
- [ ] `cache status` - Show cache info
- [ ] `sync` - Sync local data with server

### Configuration
- [ ] `config show` - Display current config
- [ ] `config edit` - Edit config interactively
- [ ] `config reset` - Reset to defaults
- [ ] `theme list` - List available themes
- [ ] `theme set <theme-name>` - Set theme
- [ ] `log-level set <level>` - Set log level

### Documentation
- [ ] `help <command>` - Get command help
- [ ] `docs` - Open documentation
- [ ] `version` - Show CLI version

---

## PHASE 6: Backend Integration & Polish

### Error Handling
- [ ] User-friendly error messages
- [ ] Suggested fixes for common errors
- [ ] Error code documentation
- [ ] Session recovery (auto-login refresh)
- [ ] Graceful degradation for optional features

### Performance
- [ ] Request batching for multiple API calls
- [ ] Smart pagination with prefetching
- [ ] Local caching of profiles, posts, feeds
- [ ] Image lazy loading and caching
- [ ] Connection pooling

### Testing
- [ ] Unit tests: config, auth, HTTP client, commands
- [ ] Integration tests: API integration, WebSocket
- [ ] E2E tests: full workflows
- [ ] Mock API server for testing
- [ ] Test fixtures and data generators

### Documentation
- [ ] README with install/quick start/troubleshooting
- [ ] Command reference documentation
- [ ] Admin guide with workflows
- [ ] Developer guide for contributors
- [ ] Architecture documentation

### Release & Distribution
- [ ] Multi-platform build scripts
- [ ] CI/CD pipeline setup
- [ ] Automated testing on PR
- [ ] Version management and changelog
- [ ] GitHub releases with binaries
- [ ] Package manager support (Homebrew, AUR, Chocolatey)

---

## IMPLEMENTATION PRIORITIES

### Critical (Must Have)
1. Authentication (login, logout, token refresh)
2. Profile viewing and basic editing
3. Post upload and creation
4. Feed viewing (timeline, global)
5. Basic engagement (like, comment, follow)
6. Admin report management
7. Admin user management

### High Priority (Should Have)
1. Audio download and playback
2. Advanced discovery and search
3. Stories
4. Notifications
5. Admin dashboard and stats
6. Batch operations

### Medium Priority (Nice to Have)
1. MIDI/Project files
2. Playlists and remix tracking
3. Interactive TUI mode
4. Data export/import
5. Advanced configuration

### Low Priority (Future)
1. MIDI challenges creation
2. Advanced caching strategies
3. Offline mode
4. Package manager distribution

---

## Dependencies to Add (Go Modules)

```bash
go get github.com/alexflint/go-arg
go get github.com/go-resty/resty/v2
go get github.com/coder/websocket
go get github.com/spf13/cobra
go get github.com/charmbracelet/bubbletea
go get github.com/spf13/viper
go get github.com/charmbracelet/lipgloss
go get github.com/charmbracelet/log
go get github.com/fatih/color
go get github.com/schollz/progressbar/v3
go get github.com/samber/lo
go get github.com/sourcegraph/conc
go get github.com/json-iterator/go
```

---

## Estimated Task Breakdown

### PHASE 1 (Core Setup): ~15-20 tasks
### PHASE 2 (Audio & Posts): ~30-35 tasks
### PHASE 3 (Social & Discovery): ~25-30 tasks
### PHASE 4 (Admin & Moderation): ~35-40 tasks
### PHASE 5 (Advanced): ~20-25 tasks
### PHASE 6 (Polish): ~15-20 tasks

**Total: ~145-170 tasks** across 6 phases

---

## Key Metrics for Success

- ✅ All core user workflows functional
- ✅ All admin/moderation workflows functional
- ✅ API integration complete and tested
- ✅ Error handling comprehensive
- ✅ Documentation complete
- ✅ Performance acceptable (< 1s for most operations)
- ✅ No sensitive data in logs or config files
- ✅ Cross-platform builds working
