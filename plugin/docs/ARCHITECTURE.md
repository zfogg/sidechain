# Sidechain VST Plugin Architecture Guide

**Last Updated**: December 2024
**Target Audience**: Developers, architects, new team members
**Time to Understand**: ~1-2 hours (with code examples)

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Core Infrastructure](#core-infrastructure)
3. [Reactive Architecture (Phase 1-2)](#reactive-architecture-phase-1-2)
4. [Advanced Features (Phase 3)](#advanced-features-phase-3)
5. [Security & Hardening (Phase 4)](#security--hardening-phase-4)
6. [Threading Model](#threading-model)
7. [Data Flow Examples](#data-flow-examples)
8. [Adding New Features](#adding-new-features)

---

## System Overview

Sidechain is a social VST plugin with three main layers:

```
┌─────────────────────────────────────────────────────┐
│ UI Layer (JUCE Components)                          │
│ ├─ Reactive Components (ObservableProperty binding) │
│ ├─ Animations (Easing, Transitions, Timelines)      │
│ └─ Views (FeedView, ChatView, ProfileView)          │
└────────────────┬────────────────────────────────────┘
                 │ subscribe()
                 ↓
┌─────────────────────────────────────────────────────┐
│ State Management Layer (Reactive Stores)            │
│ ├─ FeedStore (posts, loading, errors)               │
│ ├─ ChatStore (messages, typing, presence)           │
│ ├─ UserStore (profile, following, followers)        │
│ └─ Cache Layer (memory + disk tiers)                │
└────────────────┬────────────────────────────────────┘
                 │ dispatch()
                 ↓
┌─────────────────────────────────────────────────────┐
│ Services Layer (Business Logic)                     │
│ ├─ NetworkService (HTTP, WebSocket)                 │
│ ├─ AudioService (capture, analysis, upload)         │
│ ├─ SyncManager (conflict resolution)                │
│ └─ OfflineSyncManager (offline queueing)            │
└────────────────┬────────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────────┐
│ Security & Monitoring (Phase 4)                     │
│ ├─ SecureTokenStore (platform-specific storage)     │
│ ├─ InputValidation (sanitization, XSS prevention)   │
│ ├─ RateLimiter (API abuse prevention)               │
│ └─ ErrorTracking (aggregation, deduplication)       │
└─────────────────────────────────────────────────────┘
```

---

## Core Infrastructure

### 1. Reactive Properties (Phase 1)

**Location**: `plugin/src/util/reactive/ObservableProperty.h`

Reactive properties automatically notify UI components when data changes:

```cpp
// Define a reactive property
ObservableProperty<int> likeCount(0);

// Subscribe to changes
likeCount.observe([](int newValue) {
    Logger::info("Like count changed to: " + String(newValue));
});

// Update value - automatically notifies all observers
likeCount.set(42);
```

**Key Features**:
- Thread-safe (mutex + atomic)
- Supports map/filter transformations
- Move semantics for non-copyable types
- Automatic observer cleanup on destruction

### 2. Reactive Collections (Phase 1)

**Location**: `plugin/src/util/reactive/ObservableArray.h`

Collections with item-level change notifications:

```cpp
ObservableArray<FeedPost> posts;

// Subscribe to any change
posts.observe([](const auto& allPosts) {
    updateUI(allPosts);
});

// Subscribe to specific item events
posts.observeItemAdded([](int index, const auto& item) {
    animateNewPost(item);
});

// Batch updates for efficiency
posts.batchUpdate([&](auto& localArray) {
    for (int i = 0; i < 10; ++i)
        localArray.add(createPost(i));
});
```

### 3. Structured Logging (Phase 1)

**Location**: `plugin/src/util/logging/Logger.h`

Structured logging with categories, context, and async sinks:

```cpp
// Log with context
Logger::info("User login", {
    {"userId", "123"},
    {"timestamp", "2024-12-14T10:30:00Z"},
    {"source", "AuthComponent"}
});

// Auto-capture severity
Logger::warning("Network latency detected", {
    {"latency_ms", "1250"},
    {"endpoint", "api.sidechain.io"}
});

Logger::error("Connection failed", {
    {"code", "504"},
    {"retryCount", "3"}
});

// Logs automatically sent to sinks (console, file, network)
```

### 4. Async & Promises (Phase 1)

**Location**: `plugin/src/util/async/Async.h`, `Promise.h`, `CancellationToken.h`

Async operations with cancellation and progress:

```cpp
auto task = Async::run<FeedData>(
    [](Async::Task& task) {
        // Long-running work
        auto data = fetchFeedFromServer();
        task.setProgress(0.5f);

        // More work...
        processData(data);
        task.setProgress(1.0f);

        return data;
    },
    [](const FeedData& result) {
        // Success callback
        updateUI(result);
    },
    [](const String& error) {
        // Error callback
        showError(error);
    },
    cancellationToken
);

// Cancel if needed
task->cancel();
```

---

## Reactive Architecture (Phase 1-2)

### 1. Store Pattern

**Base**: `plugin/src/stores/Store.h`

Stores manage application state with observer notifications:

```cpp
// FeedStore inherits from Store<FeedState>
class FeedStore : public Store<FeedState> {

    struct FeedState {
        Array<FeedPost> posts;
        bool isLoading = false;
        String error;
        int64 lastUpdated = 0;
    };

public:
    void loadFeed(FeedType type) {
        // Update state -> notifies observers
        setState([](auto& state) {
            state.isLoading = true;
        });

        // Fetch data asynchronously
        networkClient->getFeed(type, [this](const auto& posts) {
            setState([posts](auto& state) {
                state.posts = posts;
                state.isLoading = false;
                state.lastUpdated = Time::getCurrentTime().toMilliseconds();
            });
        });
    }

    void toggleLike(const String& postId) {
        // Optimistic update
        setState([postId](auto& state) {
            for (auto& post : state.posts) {
                if (post.id == postId) {
                    post.isLiked = !post.isLiked;
                    post.likeCount += post.isLiked ? 1 : -1;
                }
            }
        });

        // Send to server (with rollback on error)
        networkClient->toggleLike(postId, [this, postId](bool success) {
            if (!success) {
                // Rollback
                setState([postId](auto& state) {
                    for (auto& post : state.posts) {
                        if (post.id == postId) {
                            post.isLiked = !post.isLiked;
                            post.likeCount += post.isLiked ? 1 : -1;
                        }
                    }
                });
            }
        });
    }
};
```

### 2. Reactive Component Binding

**Base**: `plugin/src/ui/bindings/ReactiveBoundComponent.h`

Components automatically update when store state changes:

```cpp
class PostCardComponent : public ReactiveBoundComponent<FeedPost> {

    Label titleLabel, likeCountLabel;
    ImageComponent avatarImage;

public:
    void setup(const FeedStore& store, const String& postId) {
        // Bind properties
        bindProperty("post.title",
            [&store, postId]() { return getPostTitle(store, postId); },
            [this](const String& title) { titleLabel.setText(title); });

        bindProperty("post.likeCount",
            [&store, postId]() { return getLikeCount(store, postId); },
            [this](int count) {
                likeCountLabel.setText(String(count));
                repaint();
            });

        // Changes automatically trigger repaint
    }

    void mouseDown(const MouseEvent& e) override {
        if (likeButton.contains(e.position)) {
            // Dispatch to store
            store->toggleLike(postId);  // Triggers binding updates
        }
    }
};
```

---

## Advanced Features (Phase 3)

### 1. Animation Framework

**Locations**:
- `plugin/src/ui/animations/Easing.h`
- `plugin/src/ui/animations/TransitionAnimation.h`
- `plugin/src/ui/animations/AnimationTimeline.h`
- `plugin/src/ui/animations/ViewTransitionManager.h`

#### Easing Functions

30+ easing curves (Linear, EaseIn/Out, Cubic, Elastic, Bounce, Back, etc.):

```cpp
// Use easing in animations
float eased = Easing::easeOutCubic(0.5f);  // [0, 1] → [0, 1] with easing

// Or look up by name
auto easingFn = Easing::create("easeOutElastic");
```

#### Transition Animations

Animate any value type:

```cpp
// Animate float opacity
TransitionAnimation<float>::create(0.0f, 1.0f, 300)  // 0 → 1 over 300ms
    ->withEasing(Easing::easeOutCubic)
    ->onProgress([this](float opacity) {
        component->setAlpha(opacity);
    })
    ->onCompletion([](){
        Logger::info("Animation complete");
    })
    ->start();

// Animate color
TransitionAnimation<Colour>::create(Colours::red, Colours::blue, 500)
    ->withEasing(Easing::easeInOutQuad)
    ->onProgress([this](Colour c) {
        repaint();  // Triggered 60 times/second
    })
    ->start();
```

#### Animation Timeline

Chain animations (sequential or parallel):

```cpp
// Sequential: one after another
AnimationTimeline::sequential()
    ->add(slideLeftAnimation)
    ->add(fadeInAnimation)
    ->withStagger(50)  // 50ms between starts
    ->onCompletion([]{ showNextView(); })
    ->start();

// Parallel: all at once
AnimationTimeline::parallel()
    ->add(scaleAnimation)
    ->add(rotateAnimation)
    ->start();
```

#### View Transitions

Built-in view transitions with animations:

```cpp
ViewTransitionManager::slideLeft(currentView, nextView, 300)
    ->onCompletion([]{
        Logger::info("Navigation complete");
    });

// Available transitions:
// - slideLeft, slideRight, slideUp, slideDown
// - fadeTransition
// - scaleIn, scaleOut, scaleFade
```

### 2. Multi-Tier Caching

**Location**: `plugin/src/util/cache/CacheLayer.h`

Three-tier cache (memory → disk → network):

```cpp
MultiTierCache<String, FeedData> cache(
    100 * 1024 * 1024,  // 100MB memory limit
    File::getTempDirectory(),  // Disk location
    3600  // 1-hour TTL
);

// Put with priority
cache.put("feed_timeline", feedData, 3600, 100);  // priority=100

// Get (checks memory → disk → network)
if (auto data = cache.get("feed_timeline")) {
    showFeed(*data);
} else {
    networkClient->getFeed(...);  // Network fallback
}

// Check stats
auto stats = cache.getStats();
Logger::info("Cache hit rate: " + String(stats.hitRate * 100) + "%");
```

### 3. Performance Monitoring

**Location**: `plugin/src/util/profiling/PerformanceMonitor.h`

Automatic performance tracking with low overhead:

```cpp
// Automatic measurement (RAII)
{
    ScopedTimer timer("feed::load");
    // ... work ...
}  // ~logs: feed::load: 250ms (avg), p95=300ms

// Manual recording
PerformanceMonitor::getInstance()->record("network::api_call",
    elapsed_ms, 1000.0  // 1000ms threshold);

// Get metrics
auto metrics = PerformanceMonitor::getInstance()->getMetrics("feed::load");
Logger::info("Feed load p95: " + String(metrics.percentile95) + "ms");
```

### 4. Real-Time Collaboration (Operational Transform)

**Location**: `plugin/src/util/crdt/OperationalTransform.h`

Conflict-free operation transform for concurrent editing:

```cpp
// Define operations
auto insert = std::make_shared<OperationalTransform::Insert>();
insert->position = 0;
insert->content = "hello";
insert->clientId = 1;

auto delete_op = std::make_shared<OperationalTransform::Delete>();
delete_op->position = 0;
delete_op->length = 5;
delete_op->clientId = 2;

// Transform against each other for conflict resolution
auto [transformed1, transformed2] =
    OperationalTransform::transform(insert, delete_op);

// Apply to text
std::string text = "hello world";
OperationalTransform::apply(text, insert);  // "hellohello world"
```

---

## Security & Hardening (Phase 4)

### 1. Secure Token Storage

**Location**: `plugin/src/security/SecureTokenStore.h`

Platform-specific secure credential storage:

```cpp
auto store = SecureTokenStore::getInstance();

// Save token securely
if (store->saveToken("jwt_token", authToken)) {
    Logger::info("Token saved securely");
}

// Load token (never in plain memory)
{
    TokenGuard guard("jwt_token");  // RAII - auto-clears on scope exit
    if (guard.isValid()) {
        auto token = guard.get();
        makeAuthenticatedRequest(token);
    }  // Memory zeroed here
}

// Backends:
// - macOS: Keychain (SecKeychainAddGenericPassword)
// - Windows: DPAPI (CryptProtectData)
// - Linux: Secret Service (file-based fallback)
```

### 2. Input Validation & Sanitization

**Location**: `plugin/src/security/InputValidation.h`

Comprehensive input validation framework:

```cpp
// Create validator
auto validator = InputValidator::create()
    ->addRule("email", InputValidator::email())
    ->addRule("username", InputValidator::alphanumeric()
        .minLength(3).maxLength(20))
    ->addRule("age", InputValidator::integer().min(0).max(150))
    ->addRule("bio", InputValidator::string().maxLength(500));

// Validate input
auto result = validator->validate({
    {"email", "user@example.com"},
    {"username", "john_doe"},
    {"age", "25"},
    {"bio", "<script>alert('xss')</script>"}
});

if (result.isValid()) {
    // All validated and sanitized
    auto email = *result.getValue("email");
    auto bio = *result.getValue("bio");  // Script tags entity-encoded
} else {
    for (const auto& error : result.getErrors()) {
        Logger::error("Validation error: " + error);
    }
}

// Validate file uploads
auto fileResult = InputValidator::validateFileUpload(
    file,
    5 * 1024 * 1024,  // 5MB max
    {"mp3", "wav", "flac"}  // Allowed extensions
);
```

### 3. Rate Limiting

**Location**: `plugin/src/security/RateLimiter.h`

Dual-algorithm rate limiting (Token Bucket / Sliding Window):

```cpp
auto limiter = RateLimiter::create()
    ->setRate(100)          // 100 requests
    ->setWindow(60)         // per 60 seconds
    ->setBurstSize(20)      // allow 20-token burst
    ->setAlgorithm(RateLimiter::Algorithm::TokenBucket);

// Check before processing request
if (auto status = limiter->tryConsume("user_123")) {
    if (status.allowed) {
        processRequest();
    } else {
        rejectRequest(429, "Too Many Requests");
        // Tell client when to retry
        response.setHeader("Retry-After", String(status.retryAfterSeconds));
    }
}

// Get current status without consuming
auto status = limiter->getStatus("user_123");
Logger::info("Remaining: " + String(status.remaining) +
             " / " + String(status.limit));
```

### 4. Error Tracking & Aggregation

**Location**: `plugin/src/util/error/ErrorTracking.h`

Automatic error collection, deduplication, and export:

```cpp
auto tracker = ErrorTracker::getInstance();

// Record errors
tracker->recordError(ErrorSource::Network,
    "Connection timeout",
    ErrorSeverity::Warning);

// Record exceptions
try {
    riskyOperation();
} catch (const std::exception& e) {
    tracker->recordException(ErrorSource::Internal, e, {
        {"operation", "riskyOperation"},
        {"userId", "123"}
    });
}

// Get statistics
auto stats = tracker->getStatistics();
Logger::info("Critical errors: " + String(stats.criticalCount));
Logger::info("Top errors: ");
for (const auto& error : stats.topErrors) {
    Logger::info("  " + error.first + ": " + String(error.second) + "x");
}

// Export for analytics
auto json = tracker->exportAsJson();
sendToAnalyticsBackend(json);

// Set callbacks
tracker->setOnCriticalError([](const ErrorInfo& error) {
    // Alert team immediately
    notifyTeam("CRITICAL: " + error.message);
});
```

---

## Threading Model

### Audio Thread (Lock-Free)

**Constraints**: Must never block, allocate, or make system calls

```cpp
// Audio callback - 0 allocations, 0 locks
void processBlock(juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi) {
    // Load atomic state (no blocks)
    if (isRecording.load(std::memory_order_acquire)) {
        // Copy to lock-free buffer
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            ringBuffer.push(buffer.getReadPointer(ch),
                          buffer.getNumSamples());
        }
    }
}
```

### UI Thread (60fps)

**Constraints**: Can block briefly (~16ms per frame), but minimize locks

```cpp
// UI update - respects 16ms frame budget
void paint(Graphics& g) {
    // Get reactive property (atomic read)
    auto likeCount = likeCountProperty.get();

    // Render (should be <1ms)
    g.drawText(String(likeCount), bounds);
}

// Listen to store changes
store->subscribeToPostLikes([this](int count) {
    // Queued on UI thread automatically
    likeCountProperty.set(count);
    repaint();
});
```

### Background Threads (Can block)

**Constraints**: Network and file I/O allowed, but coordinate with UI thread

```cpp
// Network on background thread
Async::run<FeedData>(
    []() {
        // Heavy work, network calls OK
        auto data = networkClient->getFeed();
        return data;
    },
    [this](const auto& result) {
        // Callback on UI thread - can update stores
        feedStore->setData(result);
    }
);
```

---

## Data Flow Examples

### Example 1: Load & Display Feed

```
User clicks "Load Feed"
    ↓
UI dispatches loadFeed() to FeedStore
    ↓
FeedStore sets isLoading = true → UI shows spinner
    ↓
NetworkService makes HTTP request on background thread
    ↓
Response received → FeedStore updates posts array
    ↓
FeedStore notifies observers → UI updates
    ↓
Each post component binds to post properties
    ↓
Post properties auto-update component on any change
```

### Example 2: Like Post (Optimistic Update)

```
User clicks like button
    ↓
UI immediately increments local count (optimistic)
    ↓
FeedStore marks post as liked in state
    ↓
Observers notified → Component repaint
    ↓
NetworkService sends like to API
    ↓
If error: Rollback state to previous value
    ↓
If success: State already correct
```

### Example 3: Real-Time Collaboration

```
Local user edits text
    ↓
Local operation created (Insert/Delete/Modify)
    ↓
Sent to server immediately
    ↓
Remote user makes concurrent edit
    ↓
Server receives both operations
    ↓
OperationalTransform::transform() resolves conflict
    ↓
Both operations converge to same final state
    ↓
Results sent back to both clients
    ↓
Clients apply operations in same order → Consistency
```

---

## Adding New Features

### Step 1: Define State

```cpp
// New: Comment typing indicators
struct CommentState {
    Array<TypingIndicator> typingUsers;  // Who's typing
    Array<Comment> comments;
    bool isLoading = false;
};

class CommentStore : public Store<CommentState> {
    // ...
};
```

### Step 2: Create UI Component

```cpp
class CommentThreadComponent : public ReactiveBoundComponent {
public:
    CommentThreadComponent(CommentStore& store)
        : store_(store) {

        // Bind to store updates
        bindProperty("comments", [this]() {
            return store_.getComments();
        }, [this](const auto& comments) {
            updateCommentList(comments);
        });

        // Bind typing indicators
        bindProperty("typing", [this]() {
            return store_.getTypingUsers();
        }, [this](const auto& typing) {
            updateTypingIndicators(typing);
        });
    }
};
```

### Step 3: Add Service Logic

```cpp
class CommentService {
    CommentStore& store_;
    NetworkClient& network_;

public:
    void sendComment(const String& text) {
        // Validate input
        auto validation = InputValidator::create()
            ->addRule("text", InputValidator::string().maxLength(500))
            ->validate({{"text", text}});

        if (!validation.isValid()) {
            Logger::error("Invalid comment");
            return;
        }

        // Optimistic update
        store_.addCommentOptimistic(text);

        // Send to server
        network_.postComment(text, [this](bool success) {
            if (!success) {
                store_.rollbackLastComment();
            }
        });
    }
};
```

### Step 4: Add Tests

```cpp
TEST_CASE("Comment typing indicators") {
    auto mockNetwork = std::make_shared<MockNetworkClient>();
    CommentStore store(mockNetwork);
    CommentService service(store, mockNetwork);

    Array<TypingIndicator> received;
    store.subscribeToTyping([&](const auto& typing) {
        received = typing;
    });

    // Simulate remote user typing
    mockNetwork->simulateRemoteTyping("user_123");

    REQUIRE(received.size() == 1);
    REQUIRE(received[0].userId == "user_123");
}
```

---

## Performance Guidelines

| Operation | Target | Budget | Notes |
|-----------|--------|--------|-------|
| Feed load | < 500ms | 300ms | With cache: < 100ms |
| Post render | < 50ms | 30ms | Per 10 items |
| Like toggle | < 100ms | 50ms | Optimistic + network |
| Audio capture | < 1% CPU | Per thread | Lock-free |
| UI animation | 60fps | 16ms | 1000 pixels/sec |
| Network request | < 2s | 1s | With retries |

---

## Recommended Reading Order

1. **New to Plugin Development?**
   - Start with `README.md` in root
   - Review JUCE plugin basics

2. **Understanding State Management?**
   - Read Phase 1 section
   - Study Store pattern implementations

3. **Want to Add Features?**
   - Follow "Adding New Features" section
   - Review existing stores (FeedStore, ChatStore)

4. **Performance Optimization?**
   - Read "Performance Monitoring" section
   - Profile with ScopedTimer
   - Check benchmarks: `plugin/tests/performance/BenchmarkTests.cpp`

5. **Security Hardening?**
   - Review Phase 4 section
   - Always use InputValidator for user data
   - Always use SecureTokenStore for tokens
   - Always use RateLimiter for API endpoints

---

## FAQ

**Q: How do I update UI when store changes?**
A: Bind properties in your component constructor:
```cpp
bindProperty("postTitle", [&]() { return store->getPostTitle(); },
             [this](const auto& t) { titleLabel.setText(t); });
```

**Q: Can I use synchronous network calls?**
A: No. Always use `Async::run()` to prevent blocking audio/UI threads.

**Q: How do I cancel an async operation?**
A: Pass a `CancellationToken` to `Async::run()` and call `cancel()`.

**Q: Should I validate user input?**
A: Always. Use `InputValidator::create()->addRule()` for all user data.

**Q: How do I prevent API abuse?**
A: Use `RateLimiter::create()->tryConsume()` before processing requests.

**Q: What's the best animation approach?**
A: Use `TransitionAnimation<T>` for single values, `AnimationTimeline` for sequences.

---

**Questions?** Check the code comments in each component or open an issue.

**Ready to contribute?** See `CONTRIBUTING.md` for guidelines.
