# Sidechain Codebase Improvements - Complete Todo List

**Last Updated:** 2025-12-17
**Status:** Planning Phase
**Total Tasks:** 87 items across 6 categories

---

## SECTION 1: CRITICAL & SECURITY (DO FIRST)

### 1.1 OAuth Security Fix - IMMEDIATE
- [ ] Review `backend/internal/auth/service.go` lines 40-50 for hardcoded URLs
- [ ] Create `backend/internal/config/oauth.go` with environment-based configuration
- [ ] Remove localhost fallback - require `OAUTH_REDIRECT_URL` environment variable
- [ ] Add validation in `config.LoadOAuthConfig()` to fail fast if not configured
- [ ] Update `backend/cmd/server/main.go` to use new OAuthConfig
- [ ] Update `.env.example` with required `OAUTH_REDIRECT_URL` variable
- [ ] Add runtime check: panic if `OAUTH_REDIRECT_URL` not set
- [ ] Test with actual Google/Discord OAuth flow
- [ ] Document in `SETUP_EMAIL_FORWARDING.md` or create `OAUTH_SETUP.md`

### 1.2 Token Refresh Implementation
- [ ] Review `plugin/src/stores/app/Auth.cpp` for token refresh TODO
- [ ] Design token refresh flow: when to refresh, how to retry requests
- [ ] Implement `TokenRefreshService` in backend
- [ ] Add refresh endpoint at `POST /api/v1/auth/refresh`
- [ ] Implement automatic token refresh before expiry in plugin NetworkClient
- [ ] Handle token refresh failure (redirect to login)
- [ ] Add tests for token refresh flow in `backend/internal/auth/service_test.go`

---

## SECTION 2: PLUGIN IMPROVEMENTS (C++/JUCE)

### 2.1 Animation System Overhaul - HIGH PRIORITY

#### 2.1.1 Complete ViewTransitionManager
- [ ] Open `plugin/src/ui/animations/ViewTransitionManager.h` (currently incomplete, TODO 4.21)
- [ ] Define `Animation` interface with:
  - `start()`, `stop()`, `update(float deltaTime)`, `isRunning()`
- [ ] Create `AnimationHandle` type for safe lifecycle management
- [ ] Implement animation timing (linear, easing functions)
- [ ] Add animation callbacks: `onProgress`, `onComplete`, `onCancelled`
- [ ] Test ViewTransitionManager with simple fade animation

#### 2.1.2 Implement AnimationController Singleton
- [ ] Create `plugin/src/ui/animations/AnimationController.h`
- [ ] Add singleton pattern with `getInstance()`
- [ ] Implement `animate()` method that returns `AnimationHandle`
- [ ] Implement `cancel(AnimationHandle)` for safe cleanup
- [ ] Implement `cancelAll(Component*)` for component destruction
- [ ] Add central timer tick (juce::Timer) for all animations
- [ ] Implement animation presets:
  - `fadeIn(int durationMs)`
  - `fadeOut(int durationMs)`
  - `slideIn(Rectangle<int> from, int durationMs)`
  - `slideOut(Rectangle<int> to, int durationMs)`
  - `scaleUp(float fromScale, int durationMs)`
  - `scaleDown(float toScale, int durationMs)`
- [ ] Test with concurrent animations
- [ ] Verify no frame drops during heavy animation load

#### 2.1.3 Refactor PluginEditor Navigation
- [ ] Create `plugin/src/ui/navigation/NavigationStack.h`
- [ ] Implement stack-based navigation with `push<ViewType>()` and `pop()`
- [ ] Integrate AnimationController for view transitions
- [ ] Update `plugin/src/core/PluginEditor.cpp` to use NavigationStack instead of direct view switching
- [ ] Remove local animation code from PluginEditor (currently scattered)
- [ ] Test "back" navigation between all major views
- [ ] Verify animations don't interrupt navigation

#### 2.1.4 Adopt AnimationController in Components
- [ ] Update `plugin/src/ui/feed/PostCard.h` to use AnimationController
- [ ] Update `plugin/src/ui/profile/Profile.h` to use AnimationController
- [ ] Update `plugin/src/ui/stories/StoriesFeed.h` to use AnimationController
- [ ] Update `plugin/src/ui/search/Search.h` to use AnimationController
- [ ] Replace all component-level `Timer` animation code with AnimationController calls
- [ ] Test all view transitions for smooth animations

### 2.2 State Management Refactoring

#### 2.2.1 Split AppStore into Slices
- [ ] Create `plugin/src/stores/slices/` directory
- [ ] Create `plugin/src/stores/slices/FeedSlice.h`:
  - Define `FeedSlice::State` with posts, cursor, isLoading, error
  - Define `FeedSlice::Action` as variant of feed actions
  - Implement `reduce()` function
  - Add selector functions: `selectVisiblePosts()`, `selectIsLoading()`
- [ ] Create `plugin/src/stores/slices/AuthSlice.h`:
  - Define auth state, actions, and selectors
- [ ] Create `plugin/src/stores/slices/PresenceSlice.h`:
  - Define presence state, actions, selectors
- [ ] Create `plugin/src/stores/slices/MessagesSlice.h`:
  - Define messages state, actions, selectors
- [ ] Create `plugin/src/stores/slices/NotificationsSlice.h`:
  - Define notifications state, actions, selectors

#### 2.2.2 Refactor AppState Structure
- [ ] Update `plugin/src/stores/app/AppState.h` to remove inline state definitions
- [ ] Move state definitions to respective slice headers
- [ ] Remove `AggregatedFeedState` - consolidate into `FeedSlice::State`
- [ ] Update AppStore to compose all slices
- [ ] Create unified `subscribe()` API (remove `subscribeToFeed()`, `subscribeToMessages()`, etc.)

#### 2.2.3 Improve Component Base Class
- [ ] Create `plugin/src/ui/base/StoreComponent.h`:
  - Implement RAII subscription management
  - Add auto-cleanup in destructor
  - Implement `showLoading()`, `hideLoading()` for consistent loading UI
  - Define pure virtual `stateDidChange(const StateSlice&)`
- [ ] Update all components inheriting from `AppStoreComponent<T>` to use new StoreComponent
- [ ] Test subscription cleanup when components are destroyed

#### 2.2.4 Replace AppStore.subscribe() Calls
- [ ] Find all manual `AppStore::getInstance().subscribe()` calls (60+ components)
- [ ] Convert to `StoreComponent<StateSlice>` pattern
- [ ] Remove callback-based subscriptions in favor of `stateDidChange()` override
- [ ] Verify no subscription leaks with address sanitizer

### 2.3 Memory & Cache Improvements

#### 2.3.1 Fix std::any Type Safety Issue
- [ ] Review `plugin/src/stores/AppStore.h` lines 476-484 (std::any usage)
- [ ] Create `plugin/src/util/cache/MemoryCache.h`:
  - Template-based `MemoryCache<Key, Value>`
  - TTL-based expiration
  - LRU eviction when size exceeds 100MB limit
  - Pattern-based invalidation: `invalidatePattern("feed:*")`
  - Safe `get()` returning `std::optional<Value>`
- [ ] Replace all AppStore memory cache usage with typed MemoryCache
- [ ] Add cache statistics: `getSize()`, `getEntryCount()`, `getHitRate()`

#### 2.3.2 Update File Cache Implementation
- [ ] Review `plugin/src/util/cache/FileCache.h` for consistency
- [ ] Add cache size limits (max 500MB for file cache)
- [ ] Implement cache cleanup on low disk space
- [ ] Add periodic cleanup task for expired files

#### 2.3.3 Consolidate Multiple Cache Systems
- [ ] Audit: `AudioCache.h`, `DraftCache.h`, `ImageCache.h` for duplicate logic
- [ ] Consolidate to single cache abstraction where possible
- [ ] Ensure consistent TTL and eviction policies across caches
- [ ] Document cache strategy and limits

### 2.4 Threading & Async Improvements

#### 2.4.1 Implement TaskScheduler with Thread Pool
- [ ] Create `plugin/src/util/async/TaskScheduler.h`:
  - Singleton with `getInstance()`
  - Thread pool with `std::thread::hardware_concurrency()` threads
  - `async(work, callback)` returning `TaskHandle`
  - `cancel(TaskHandle)` for safe cancellation
  - `cancelAll()` for cleanup
  - `getQueueSize()`, `getActiveThreadCount()` for monitoring
- [ ] Implement `TaskHandle` with RAII cleanup
- [ ] Add task timeout mechanism

#### 2.4.2 Replace Async::run() Usage
- [ ] Find all `Async::run()` calls in plugin (currently unlimited thread spawning)
- [ ] Replace with `TaskScheduler::async()`
- [ ] Update network calls to use TaskScheduler:
  - `NetworkClient::fetchFeed()` → use TaskScheduler
  - `NetworkClient::uploadAudio()` → use TaskScheduler
  - `NetworkClient::downloadImage()` → use TaskScheduler
- [ ] Test concurrent requests with thread pool

#### 2.4.3 Integrate CancellationToken
- [ ] Review `plugin/src/util/async/CancellationToken.h` implementation
- [ ] Add cancellation token to TaskScheduler
- [ ] Propagate cancellation tokens through network requests
- [ ] Handle cancellation in download/upload operations
- [ ] Test cancellation of in-flight requests during view transition

#### 2.4.4 Complete Promise<T> Implementation
- [ ] Review `plugin/src/util/async/Promise.h` current state
- [ ] Implement Promise chaining: `then()`, `catch()`, `finally()`
- [ ] Support `resolve()`, `reject()`, `wait()`
- [ ] Use Promise for complex async workflows (auth flow, upload pipeline)
- [ ] Test Promise error propagation

### 2.5 Input Validation & Security

#### 2.5.1 Centralize Validation
- [ ] Review `plugin/src/util/Validate.h` (validation functions scattered)
- [ ] Create `ValidationResult<T>` type for uniform error reporting
- [ ] Consolidate all validation in Validate namespace:
  - `validateUsername()`
  - `validateEmail()`
  - `validatePassword()`
  - `validateAudioFile()`
  - `validateMidiFile()`
- [ ] Add unit tests for each validator

#### 2.5.2 Improve Error Boundary
- [ ] Review `plugin/src/ui/common/ErrorState.h` implementation
- [ ] Create global error boundary component in PluginEditor
- [ ] Implement error recovery strategies:
  - Network errors → retry with exponential backoff
  - Validation errors → show inline message
  - Fatal errors → show error state UI
- [ ] Test error recovery flows

#### 2.5.3 Rate Limiter Integration
- [ ] Review `plugin/src/security/RateLimiter.h` implementation
- [ ] Integrate RateLimiter into all network requests
- [ ] Add per-endpoint rate limiting:
  - Feed requests: 1 per 500ms
  - Upload: 1 concurrent
  - Search: 1 per 300ms
- [ ] Show rate limit status in UI

### 2.6 WebSocket & Real-Time Features

#### 2.6.1 Fix StreamChatClient Issues
- [ ] Review `plugin/src/network/StreamChatClient.cpp` TODO about queryChannels
- [ ] Debug: Why are 0 members returned despite 6 channels existing?
- [ ] Check authentication token format for getstream.io
- [ ] Verify channel member count population
- [ ] Add logging for channel queries

#### 2.6.2 Consolidate WebSocket Clients
- [ ] Audit: StreamChatClient.h (direct getstream.io) vs WebSocketClient.h (backend)
- [ ] Clarify which client is used for which features:
  - Messages: StreamChatClient or WebSocketClient?
  - Presence: StreamChatClient or WebSocketClient?
  - Typing indicators: which?
  - Real-time feed updates: which?
- [ ] Remove duplicate WebSocket connections
- [ ] Test real-time message delivery

#### 2.6.3 Implement Reconnection Strategy
- [ ] Add exponential backoff for WebSocket reconnection
- [ ] Implement connection state tracking (connected, reconnecting, disconnected)
- [ ] Queue messages during disconnection
- [ ] Sync state on reconnection
- [ ] Show connection status in UI

#### 2.6.4 Persist Presence Across Sessions
- [ ] Review `plugin/src/stores/app/Presence.cpp` status persistence TODO
- [ ] Implement local storage of last presence status
- [ ] Restore presence on plugin reload
- [ ] Sync with backend on startup
- [ ] Test presence recovery

---

## SECTION 3: BACKEND IMPROVEMENTS (GO)

### 3.1 Handler Architecture Refactoring - HIGH PRIORITY

#### 3.1.1 Design Repository Layer
- [ ] Create `backend/internal/repository/` directory
- [ ] Create `backend/internal/repository/feed_repository.go`:
  - Define `FeedRepository` interface
  - Methods: `GetTimeline()`, `GetGlobalFeed()`, `CreatePost()`, `GetPost()`
  - Implement with GORM
- [ ] Create `backend/internal/repository/story_repository.go`
- [ ] Create `backend/internal/repository/message_repository.go`
- [ ] Create `backend/internal/repository/notification_repository.go`
- [ ] Create `backend/internal/repository/user_repository.go`
- [ ] Create `backend/internal/repository/audio_repository.go`
- [ ] Create `backend/internal/repository/midi_repository.go`
- [ ] Create `backend/internal/repository/playlist_repository.go`
- [ ] Create `backend/internal/repository/reaction_repository.go`
- [ ] Create `backend/internal/repository/comment_repository.go`

#### 3.1.2 Design Service Layer
- [ ] Create `backend/internal/service/` directory
- [ ] Create `backend/internal/service/feed_service.go`:
  - Define `FeedService` interface
  - Methods: `GetTimeline()`, `GetGlobalFeed()`, `CreatePost()`
  - Inject: FeedRepository, cache, search client
  - Business logic (aggregation, recommendations)
- [ ] Create `backend/internal/service/story_service.go`
- [ ] Create `backend/internal/service/message_service.go`
- [ ] Create `backend/internal/service/notification_service.go`
- [ ] Create `backend/internal/service/user_service.go`
- [ ] Create `backend/internal/service/audio_service.go`
- [ ] Create `backend/internal/service/midi_service.go`
- [ ] Create `backend/internal/service/playlist_service.go`
- [ ] Create `backend/internal/service/recommendation_service.go`

#### 3.1.3 Refactor Handlers to Use Services
- [ ] Update `backend/internal/handlers/feed_handler.go`:
  - Inject FeedService (not FeedRepository)
  - Remove business logic from handler
  - Keep only HTTP layer (request/response)
- [ ] Update all other handlers similarly
- [ ] Remove fat dependencies from handlers.go struct
- [ ] Test handlers with mock services

#### 3.1.4 Split Handlers Struct
- [ ] Create `backend/internal/handlers/feed_handlers.go`
- [ ] Create `backend/internal/handlers/story_handlers.go`
- [ ] Create `backend/internal/handlers/message_handlers.go`
- [ ] Create `backend/internal/handlers/user_handlers.go`
- [ ] Create `backend/internal/handlers/notification_handlers.go`
- [ ] Create `backend/internal/handlers/audio_handlers.go`
- [ ] Create `backend/internal/handlers/playlist_handlers.go`
- [ ] Create `backend/internal/handlers/auth_handlers.go` (if not exists)
- [ ] Move corresponding handler methods from handlers.go to domain-specific files
- [ ] Keep handlers.go minimal (maybe just router setup)

### 3.2 Request/Response DTO Layer

#### 3.2.1 Create DTO Package
- [ ] Create `backend/internal/dto/` directory
- [ ] Create `backend/internal/dto/user.go`:
  - `UserResponse` (public fields only: ID, Username, AvatarURL)
  - `UserDetailResponse` (includes email for authenticated user)
  - `CreateUserRequest` with validation tags
  - `UpdateUserRequest` with partial updates
  - Add `ToResponse()` conversion method on models.User
- [ ] Create `backend/internal/dto/post.go`:
  - `PostResponse` (basic post info)
  - `PostDetailResponse` (with comments, reactions)
  - `CreatePostRequest` with validation
  - `UpdatePostRequest`
- [ ] Create `backend/internal/dto/story.go`
- [ ] Create `backend/internal/dto/comment.go`
- [ ] Create `backend/internal/dto/reaction.go`
- [ ] Create `backend/internal/dto/message.go`
- [ ] Create `backend/internal/dto/notification.go`
- [ ] Create `backend/internal/dto/audio.go`
- [ ] Create `backend/internal/dto/playlist.go`

#### 3.2.2 Update Handlers to Use DTOs
- [ ] Update feed handlers to return `PostResponse` instead of `models.Post`
- [ ] Update user handlers to return `UserResponse` instead of `models.User`
- [ ] Update all handlers to accept request DTOs
- [ ] Add conversion methods: `post.ToResponse()`, `user.ToResponse()`
- [ ] Test API responses don't expose sensitive fields

#### 3.2.3 Document DTO Strategy
- [ ] Create `notes/DTO_STRATEGY.md`
- [ ] Document which fields are public vs internal
- [ ] Document field encryption/redaction rules
- [ ] Document API versioning strategy for DTOs

### 3.3 Validation Middleware

#### 3.3.1 Create Validation Package
- [ ] Create `backend/internal/validation/` directory
- [ ] Create `backend/internal/validation/request_validator.go`:
  - Helper function to validate and parse requests
  - Custom error formatting for validation failures
  - Return structured `ValidationError`
- [ ] Create validation rules for each DTO:
  - Username: alphanumeric, 3-32 chars
  - Email: valid RFC 5322
  - Password: min 8 chars, complexity rules
  - Post title: 1-200 chars
  - Audio file: size < 50MB, format MP3/WAV/AAC

#### 3.3.2 Create Validation Middleware
- [ ] Create `backend/internal/middleware/validate_request.go`:
  - Middleware that extracts and validates request body
  - Return 400 Bad Request with validation errors if invalid
  - Pass validated data to handler
- [ ] Add middleware to router for all routes
- [ ] Test validation error responses

#### 3.3.3 Add Custom Validation Functions
- [ ] Implement `ValidateUsername()` → ValidationError
- [ ] Implement `ValidateEmail()` → ValidationError
- [ ] Implement `ValidatePassword()` → ValidationError
- [ ] Implement `ValidateAudioFile()` → ValidationError
- [ ] Implement `ValidatePostTitle()` → ValidationError
- [ ] Add regex patterns for URL, phone, etc.
- [ ] Test all validators with edge cases

### 3.4 Database Query Optimization

#### 3.4.1 Fix N+1 Problems in Feed
- [ ] Review `backend/internal/handlers/feed.go` query patterns
- [ ] Update `GetGlobalFeed()` to eager load:
  - Author (via Preload("Author"))
  - Comments with comment authors (via Preload("Comments.Author"))
  - Reactions (via Preload("Reactions"))
  - Audio metadata (via Preload("Audio"))
- [ ] Benchmark before/after with 1000 posts
- [ ] Document eager loading strategy

#### 3.4.2 Implement Query Builder Abstractions
- [ ] Create query builders to prevent raw SQL in handlers
- [ ] Example: `QueryBuilder.ForUser(userID).WithComments().Build()`
- [ ] Create builders for common queries:
  - Get feed with pagination
  - Get user posts
  - Get trending posts
  - Get user followers/following

#### 3.4.3 Add Database Indexes
- [ ] Review current indexes (if any) in migrations
- [ ] Add index on `posts.user_id` (filter by user)
- [ ] Add index on `posts.created_at` (sort by time)
- [ ] Add index on `comments.post_id` (get comments)
- [ ] Add index on `users.username` (search)
- [ ] Add compound index on `(user_id, created_at)` for user feed
- [ ] Test index usage with EXPLAIN ANALYZE

#### 3.4.4 Implement Connection Pooling
- [ ] Review current GORM connection pool settings
- [ ] Set `MaxIdleConns` to 10
- [ ] Set `MaxOpenConns` to 100
- [ ] Set `ConnMaxLifetime` to 1 hour
- [ ] Monitor connection pool metrics

### 3.5 Structured Logging

#### 3.5.1 Add Zap Logging
- [ ] Add `go.uber.org/zap` to go.mod
- [ ] Create `backend/internal/logger/` directory
- [ ] Create `backend/internal/logger/logger.go`:
  - Initialize zap logger in main()
  - Export global logger instance
  - Configure for JSON output (production) / human-readable (dev)
- [ ] Replace all `fmt.Printf()`, `log.Printf()` with zap calls
- [ ] Add structured fields: request_id, user_id, endpoint
- [ ] Test log output format

#### 3.5.2 Add Request Logging Middleware
- [ ] Create `backend/internal/middleware/request_logger.go`
- [ ] Log request: method, path, status, duration
- [ ] Log response size
- [ ] Include request_id for tracing
- [ ] Add middleware to all routes

#### 3.5.3 Add Error Logging
- [ ] Log all errors with context (not just print)
- [ ] Include stack trace for panics
- [ ] Log database errors with query info (if safe)
- [ ] Create error tracking integration (if needed)

### 3.6 Error Handling & Recovery

#### 3.6.1 Centralized Error Handler
- [ ] Create `backend/internal/middleware/error_handler.go`
- [ ] Define error type constants:
  - `ErrValidationFailed`, `ErrNotFound`, `ErrUnauthorized`, `ErrConflict`
- [ ] Map error types to HTTP status codes
- [ ] Implement error response formatter
- [ ] Add middleware to all routes

#### 3.6.2 Panic Recovery
- [ ] Create `backend/internal/middleware/panic_recovery.go`
- [ ] Recover from panics and return 500 Internal Server Error
- [ ] Log panic with stack trace
- [ ] Don't expose panic details to clients
- [ ] Add to top of middleware stack

#### 3.6.3 Database Error Handling
- [ ] Implement helper to convert GORM errors:
  - `gorm.ErrRecordNotFound` → 404 Not Found
  - Constraint violations → 409 Conflict
  - Other errors → 500 Internal Server Error
- [ ] Test error messages are safe (no SQL leakage)

### 3.7 WebSocket & Real-Time Improvements

#### 3.7.1 Message Persistence
- [ ] Review `backend/internal/websocket/handler.go` design
- [ ] Add message queue to persist WebSocket messages
- [ ] Store messages in database if delivery fails
- [ ] Replay stored messages on reconnection
- [ ] Implement message TTL (keep for 7 days)

#### 3.7.2 Reconnection Strategy
- [ ] Implement connection state tracking
- [ ] Implement exponential backoff for reconnection
- [ ] Sync presence status on reconnection
- [ ] Catch up on missed notifications
- [ ] Test reconnection after network outage

#### 3.7.3 Presence Sync
- [ ] Verify Phase 8 implementation: "direct presence updates to getstream.io"
- [ ] Test presence status is actually synced
- [ ] Add presence status in user response
- [ ] Track last_seen_at timestamp

### 3.8 Existing Todos Review

#### 3.8.1 Prioritize common_todos.go
- [ ] Open `backend/internal/handlers/common_todos.go` (159 items)
- [ ] Categorize into:
  - Must-have (top 10 items) → create GitHub issues
  - Nice-to-have (next 20 items) → backlog
  - Future work (rest) → archive
- [ ] Move categorized items to GitHub issues
- [ ] Replace common_todos.go with link to GitHub milestone
- [ ] Review and update existing issues

---

## SECTION 4: DEPENDENCIES & TOOLS

### 4.1 Plugin Dependencies (C++)

#### 4.1.1 Add New Dependencies
- [ ] Add `fmt` library for string formatting:
  - `find_package(fmt CONFIG REQUIRED)` in CMakeLists.txt
  - Replace custom string concatenation with `fmt::format()`
- [ ] Add `spdlog` for async logging:
  - `find_package(spdlog CONFIG REQUIRED)` in CMakeLists.txt
  - Create logger initialization in PluginProcessor
  - Replace printf/cout with spdlog calls
- [ ] Add `nlohmann/json` for JSON parsing:
  - `find_package(nlohmann_json CONFIG REQUIRED)`
  - Replace custom JSON parsing with library
- [ ] Add `range-v3` for functional programming:
  - `find_package(range-v3 CONFIG REQUIRED)`
  - Use in algorithms (filter, map, transform)
- [ ] Update `asio` from 1.14.1 to 1.31+ (standalone):
  - Update in CMakeLists.txt
  - Test WebSocket connections still work

#### 4.1.2 Pin Dependency Versions
- [ ] Specify exact versions in CMakeLists.txt for reproducible builds
- [ ] Document version compatibility matrix
- [ ] Add notes about platform-specific versions (macOS, Linux, Windows)

### 4.2 Backend Dependencies (Go)

#### 4.2.1 Remove Duplicate AWS SDK
- [ ] Review go.mod for `aws-sdk-go` (v1) and `aws-sdk-go-v2`
- [ ] Remove all `github.com/aws/aws-sdk-go` (v1) imports
- [ ] Replace with `aws-sdk-go-v2` equivalents
- [ ] Update S3 client initialization to use v2
- [ ] Test S3 uploads still work
- [ ] Run `go mod tidy` to cleanup

#### 4.2.2 Add Structured Logging
- [ ] Add `go.uber.org/zap` v1.27+ to go.mod:
  - `go get github.com/uber-go/zap`
  - `go get go.uber.org/zap`
- [ ] Create logger initialization in `backend/internal/logger/logger.go`
- [ ] Update `backend/cmd/server/main.go` to initialize logger

#### 4.2.3 Add CLI Framework
- [ ] Add `github.com/spf13/cobra` v1.8+ for CLI commands:
  - `go get github.com/spf13/cobra/cmd/cobra@latest`
  - Create `backend/cmd/admin/` for admin tools
  - Implement: user management, data cleanup, index rebuild
- [ ] Add `github.com/spf13/viper` for config file support

#### 4.2.4 Add Testing Utilities
- [ ] Add `github.com/stretchr/testify/assert` for test assertions
- [ ] Add `github.com/stretchr/testify/mock` for mocking
- [ ] Add `github.com/stretchr/testify/suite` for test suites

#### 4.2.5 Update Go Version
- [ ] Review `backend/go.mod` - currently uses 1.24.2 (outdated)
- [ ] Update to `go 1.23` (latest stable)
- [ ] Test compilation with new version
- [ ] Update CI/CD to use Go 1.23

#### 4.2.6 Replace go-migrate with Atlas
- [ ] Add `ariga.io/atlas` for better schema versioning
- [ ] Create schema.hcl for schema definitions
- [ ] Migrate existing migrations to Atlas format
- [ ] Test schema diff and apply operations

### 4.3 Development Tools

#### 4.3.1 Code Linting & Formatting
- [ ] Install `golangci-lint` locally:
  - `go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest`
  - Create `.golangci.yml` config
- [ ] Install `cppcheck` for C++ static analysis:
  - `apt install cppcheck` (or `brew install cppcheck`)
- [ ] Install `clang-format` for C++ formatting:
  - Create `.clang-format` config in project root
  - Setup pre-commit hook to auto-format

#### 4.3.2 Hot Reload Development
- [ ] Install `air` for Go hot reload:
  - `go install github.com/cosmtrek/air@latest`
  - Create `.air.toml` config
  - Update Makefile: `make backend-dev` to use air
- [ ] Keep existing cmake watch for plugin

#### 4.3.3 Memory & Performance Tools
- [ ] Install `valgrind` for memory debugging (Linux):
  - `apt install valgrind`
- [ ] Install `cppcheck` for static analysis:
  - Already mentioned above
- [ ] Integrate into CI pipeline

### 4.4 Add to Makefile

#### 4.4.1 Linting Targets
- [ ] Add `make lint-go` → runs golangci-lint
- [ ] Add `make lint-cpp` → runs cppcheck
- [ ] Add `make lint` → runs all linters
- [ ] Add `make format-go` → auto-formats Go code
- [ ] Add `make format-cpp` → auto-formats C++ code

#### 4.4.2 Testing & Coverage Targets
- [ ] Add `make test-coverage` → runs tests with coverage
- [ ] Add `make coverage-report` → generates HTML coverage report
- [ ] Add `make test-watch` → runs tests on file changes
- [ ] Update test output to show coverage percentage

#### 4.4.3 Development Targets
- [ ] Add `make dev` → starts both plugin and backend with hot reload
- [ ] Add `make plugin-watch` → watches plugin files
- [ ] Add `make backend-watch` → watches backend files with air

---

## SECTION 5: TESTING & CI/CD

### 5.1 Backend Test Coverage

#### 5.1.1 Critical Tests - Feed Handlers
- [ ] Create `backend/internal/handlers/feed_test.go` (currently missing):
  - Test `GetTimeline()` returns correct posts for user
  - Test `GetGlobalFeed()` returns top posts
  - Test `CreatePost()` validates input
  - Test pagination with cursor
  - Test error cases (auth, validation, database)
- [ ] Mock dependencies: FeedRepository, cache, search
- [ ] Test concurrent requests
- [ ] Add benchmarks for query performance

#### 5.1.2 Auth Handler Tests
- [ ] Update `backend/internal/handlers/auth.go` tests:
  - Test Google OAuth flow
  - Test Discord OAuth flow
  - Test token refresh
  - Test logout (revoke token)
  - Test invalid credentials
  - Test token expiration

#### 5.1.3 WebSocket Handler Tests
- [ ] Create `backend/internal/websocket/handler_test.go`:
  - Test message delivery
  - Test presence updates
  - Test reconnection
  - Test message persistence

#### 5.1.4 Story Handler Tests
- [ ] Create `backend/internal/handlers/stories_handlers_test.go` (if incomplete):
  - Expand existing tests
  - Test story creation with audio
  - Test story viewing
  - Test highlights
  - Test deletion/archiving

#### 5.1.5 Repository Tests
- [ ] Create tests for each repository:
  - `backend/internal/repository/feed_repository_test.go`
  - `backend/internal/repository/user_repository_test.go`
  - etc.
- [ ] Use test database (SQLite for tests)
- [ ] Test all query methods

#### 5.1.6 Service Tests
- [ ] Create tests for each service:
  - `backend/internal/service/feed_service_test.go`
  - `backend/internal/service/user_service_test.go`
  - etc.
- [ ] Mock repositories
- [ ] Test business logic

#### 5.1.7 Middleware Tests
- [ ] Create `backend/internal/middleware/` test files
- [ ] Test validation middleware
- [ ] Test error handler
- [ ] Test auth middleware
- [ ] Test logging middleware

#### 5.1.8 Integration Tests
- [ ] Create `backend/tests/integration/` directory
- [ ] Write end-to-end tests:
  - User registration → login → create post → view feed
  - Message flow: send message → receive → delivery confirmation
  - Real-time presence: go online → appears in followers' UI
- [ ] Use test database + test server

### 5.2 Plugin Tests

#### 5.2.1 Unit Tests for Utilities
- [ ] Create `plugin/tests/unit/` directory
- [ ] Create `plugin/tests/unit/Validate_test.cpp`:
  - Test each validator (username, email, password, etc.)
  - Test valid and invalid inputs
- [ ] Create `plugin/tests/unit/MemoryCache_test.cpp`:
  - Test cache get/set
  - Test TTL expiration
  - Test LRU eviction
  - Test pattern invalidation
- [ ] Create `plugin/tests/unit/AnimationController_test.cpp`:
  - Test animation creation and cancellation
  - Test concurrent animations
  - Test handle cleanup

#### 5.2.2 Integration Tests for Plugin
- [ ] Expand `plugin/tests/integration/`:
  - Test WebSocket message delivery
  - Test feed loading
  - Test post creation
  - Test real-time updates
- [ ] Use mock backend (or local test server)
- [ ] Test audio file handling

#### 5.2.3 UI Component Tests
- [ ] Create `plugin/tests/ui/` for JUCE component tests:
  - Test component rendering
  - Test user interactions
  - Test state updates trigger repaint
  - Test accessibility

### 5.3 GitHub Actions CI/CD

#### 5.3.1 Create CI Pipeline
- [ ] Create `.github/workflows/ci.yml`:
  - Trigger on push/PR to main
  - Run on: ubuntu-latest, macos-latest, windows-latest
  - Setup Go 1.23, C++ compiler (GCC/Clang), JUCE
  - Run `make lint-go`, `make lint-cpp`
  - Run `make test-backend` with coverage
  - Run `make plugin-debug`
  - Upload coverage to codecov.io

#### 5.3.2 Create Deployment Pipeline
- [ ] Create `.github/workflows/deploy.yml`:
  - Trigger on tags (v*.*.*)
  - Build backend with `make backend-release`
  - Build plugin with `make plugin-release`
  - Create GitHub release with binaries
  - Deploy backend to production (if configured)
  - Publish plugin to VST stores

#### 5.3.3 Create Code Quality Check
- [ ] Create `.github/workflows/quality.yml`:
  - Run codecov.io integration
  - Check coverage meets threshold (target 80%)
  - Run SAST (static analysis) tools
  - Check for security vulnerabilities

#### 5.3.4 Setup Code Coverage
- [ ] Add codecov.io integration to repo
- [ ] Configure coverage threshold: min 80% required for merge
- [ ] Create coverage badge in README
- [ ] Setup coverage reports in PR comments

---

## SECTION 6: CODE CLEANUP & REFACTORING

### 6.1 Plugin Cleanup

#### 6.1.1 Remove Technical Debt
- [ ] Remove TODO 4.21 (animation) - replace with actual implementation
- [ ] Remove TODO 4.20 (navigation) - replace with NavigationStack
- [ ] Remove TODO 6.5.3.4.2 (Phase incomplete) - mark as backlog
- [ ] Search for all TODOs and categorize:
  - Active work (current sprint)
  - Backlog (future)
  - Obsolete (delete)

#### 6.1.2 Code Organization
- [ ] Move standalone functions in .cpp files to class methods
- [ ] Remove unused includes from headers
- [ ] Move implementation details to .cpp files (not headers)
- [ ] Use forward declarations to reduce coupling

#### 6.1.3 Const Correctness
- [ ] Add `const` to methods that don't modify state
- [ ] Add `const` to references that aren't modified
- [ ] Mark static helpers as `static` to limit scope

#### 6.1.4 Header Cleanup
- [ ] Remove circular includes (use forward declarations)
- [ ] Add include guards to all headers
- [ ] Verify header includes are minimal
- [ ] Use `#pragma once` for consistency

### 6.2 Backend Cleanup

#### 6.2.1 Remove Unused Code
- [ ] Audit handlers/handlers.go for unused methods after refactoring
- [ ] Remove deprecated endpoints (if any)
- [ ] Remove unused packages from go.mod
- [ ] Search for dead code (functions never called)

#### 6.2.2 Fix StringArray Parsing
- [ ] Update `backend/internal/models/user.go` StringArray implementation
- [ ] Handle escaped commas in array parsing
- [ ] Use proper PostgreSQL array type (pq.Array)
- [ ] Test with edge cases: empty, single, multiple, special chars

#### 6.2.3 Consolidate Config
- [ ] Review scattered configuration (hardcoded values)
- [ ] Move all config to `.env` or config package
- [ ] Use `github.com/spf13/viper` for config management
- [ ] Document all required config variables

#### 6.2.4 Database Schema Cleanup
- [ ] Review schema for unused tables/columns
- [ ] Create migrations to remove obsolete fields
- [ ] Add NOT NULL constraints where appropriate
- [ ] Verify foreign key relationships

### 6.3 Documentation

#### 6.3.1 Update Architecture Docs
- [ ] Update `CLAUDE.md` with new patterns:
  - AnimationController usage
  - Repository/Service pattern
  - DTO layer
  - Validation middleware
- [ ] Add architecture diagrams (optional but helpful)
- [ ] Document threading model explicitly

#### 6.3.2 API Documentation
- [ ] Create `notes/API_DOCUMENTATION.md`
- [ ] Document all endpoints with:
  - Request/response examples
  - Required auth
  - Error codes
  - Rate limits
- [ ] Use OpenAPI/Swagger format (if tooling available)

#### 6.3.3 Development Guide
- [ ] Create `notes/DEVELOPMENT_WORKFLOW.md`:
  - Setup instructions (database, env vars)
  - Running tests
  - Running linters
  - Creating commits
  - PR process
- [ ] Create troubleshooting guide

#### 6.3.4 Deployment Guide
- [ ] Create `notes/DEPLOYMENT.md`:
  - Environment variables
  - Database migrations
  - Secrets management
  - Scaling considerations
- [ ] Document infrastructure (Docker, K8s, etc.)

---

## SECTION 7: IMPLEMENTATION PHASES

### Phase 1: Critical Security & Foundation (Week 1)
**Goal: Fix immediate issues and establish development patterns**

- [ ] 1.1.1-1.1.9: Fix OAuth hardcoding (1 day)
- [ ] 4.1.2-4.2.5: Update dependencies (2 days)
- [ ] 4.3.1-4.3.3: Setup development tools (1 day)
- [ ] 4.4.1-4.4.3: Update Makefile with new targets (1 day)
- [ ] 5.3.1: Create GitHub Actions CI pipeline (1 day)

**Total Effort:** ~6 days

**Deliverables:**
- OAuth fixed and tested
- CI/CD pipeline running
- Development tools configured
- Makefile targets available

### Phase 2: Plugin Architecture (Weeks 2-3)
**Goal: Fix animations and refactor state management**

- [ ] 2.1.1-2.1.4: Complete animation system (4 days)
  - Finish ViewTransitionManager
  - Implement AnimationController
  - Update PluginEditor navigation
  - Adopt in components
- [ ] 2.2.1-2.2.4: Refactor AppStore to slices (3 days)
- [ ] 2.3.1-2.3.3: Fix memory cache type safety (2 days)
- [ ] 2.4.1-2.4.4: Implement TaskScheduler (2 days)

**Total Effort:** ~11 days

**Deliverables:**
- Smooth animations in all view transitions
- Type-safe state management
- Thread pool for async operations
- No more animation stuttering

### Phase 3: Backend Architecture (Weeks 3-4)
**Goal: Refactor handlers into services and repositories**

- [ ] 3.1.1-3.1.4: Split handlers into Repository/Service (5 days)
  - Start with FeedService (most critical)
  - Then User, Story, Message, etc.
- [ ] 3.2.1-3.2.3: Implement DTO layer (3 days)
- [ ] 3.3.1-3.3.3: Add validation middleware (2 days)
- [ ] 3.5.1-3.5.3: Add structured logging (2 days)

**Total Effort:** ~12 days

**Deliverables:**
- Cleaner handler structure
- API contracts separated from domain models
- Centralized validation
- Structured logging for debugging

### Phase 4: Database & Performance (Week 4)
**Goal: Optimize queries and add proper testing**

- [ ] 3.4.1-3.4.4: Fix N+1 queries and add indexes (2 days)
- [ ] 3.7.1-3.7.3: Improve real-time features (2 days)
- [ ] 5.1.1-5.1.8: Add comprehensive backend tests (5 days)

**Total Effort:** ~9 days

**Deliverables:**
- Feed loads 10x faster
- 100+ tests covering critical paths
- Real-time features working reliably

### Phase 5: Polish & Testing (Week 5)
**Goal: Complete testing and documentation**

- [ ] 2.5-2.6: Complete plugin improvements (3 days)
- [ ] 5.2.1-5.2.3: Add plugin unit/integration tests (3 days)
- [ ] 6.1-6.3: Documentation and cleanup (2 days)

**Total Effort:** ~8 days

**Deliverables:**
- Plugin tests with 70%+ coverage
- Complete documentation
- Zero critical TODOs
- Ready for production

---

## SUMMARY STATISTICS

| Category | Count | Est. Effort (hours) |
|----------|-------|-------------------|
| Critical/Security | 7 | 6 |
| Plugin Improvements | 28 | 88 |
| Backend Improvements | 32 | 96 |
| Dependencies & Tools | 18 | 24 |
| Testing & CI/CD | 24 | 56 |
| Code Cleanup | 16 | 32 |
| **TOTAL** | **87** | **302 hours (~7-8 weeks)** |

---

## TRACKING PROGRESS

Use this template to track completion:

```
## Week of [DATE]

### Completed
- [ ] Item from todo list
- [ ] Item from todo list

### In Progress
- [ ] Item from todo list

### Blocked
- [ ] Item requiring decision

### Next Week
- [ ] Planned items
```

---

## NOTES

- **Prioritization**: Start with Phase 1 (security) then Phase 2 (animations) before Phase 3
- **Team Coordination**: If working with others, coordinate on Phase 3 (backend) to avoid conflicts
- **Testing Strategy**: Write tests as you implement new features, don't batch testing at end
- **Documentation**: Update docs alongside code, not as separate task
- **Git Commits**: Make atomic commits for each completed item with clear messages
- **Review**: Get code review before merging major refactoring PRs (especially handlers)

---

**Created:** 2025-12-17
**Last Updated:** 2025-12-17
**Status:** Ready for Implementation
