# Sidechain Plugin Architecture

**Last Updated**: 2025-12-14
**Version**: 1.0

---

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Key Components](#key-components)
4. [Architecture Documents](#architecture-documents)
5. [Design Principles](#design-principles)
6. [Technology Stack](#technology-stack)

---

## Overview

Sidechain is a social VST audio plugin built with JUCE that enables music producers to share and discover loops directly from their DAW. The plugin combines real-time audio processing with social networking features, providing a unique creative collaboration platform.

### Core Features

- **Audio Capture**: Record and share loops directly from DAW
- **Social Feed**: Browse, play, and interact with community content
- **Recommendations**: AI-powered personalized content discovery
- **Real-Time Updates**: WebSocket-based live feed updates
- **Comments & Reactions**: Engage with other producers
- **User Profiles**: Follow creators, build your network

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         DAW (Ableton, Logic, etc.)                   │
│                                    │                                 │
│                                    ▼                                 │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                    SIDECHAIN VST PLUGIN                       │  │
│  │                      (C++/JUCE Framework)                     │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │                                                               │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────────────────┐ │  │
│  │  │   Audio    │  │     UI     │  │    State Management    │ │  │
│  │  │ Processing │  │ Components │  │        (Stores)        │ │  │
│  │  │            │  │            │  │                        │ │  │
│  │  │ - Capture  │  │ - PostsFeed│  │ - FeedStore           │ │  │
│  │  │ - Playback │  │ - Profile  │  │ - UserDataStore       │ │  │
│  │  │ - Waveform │  │ - Comments │  │ - Reactive Updates    │ │  │
│  │  └────────────┘  └────────────┘  └────────────────────────┘ │  │
│  │                                                               │  │
│  │  ┌──────────────────────────────────────────────────────────┐ │  │
│  │  │              Network Layer (NetworkClient)               │ │  │
│  │  │  - HTTP/REST API Client                                  │ │  │
│  │  │  - WebSocket Client (Real-time updates)                  │ │  │
│  │  │  - Request Queue & Retry Logic                           │ │  │
│  │  │  - Authentication (JWT)                                  │ │  │
│  │  └──────────────────────────────────────────────────────────┘ │  │
│  │                              │                                │  │
│  └──────────────────────────────┼────────────────────────────────┘  │
└─────────────────────────────────┼────────────────────────────────────┘
                                  │ HTTPS/WSS
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          BACKEND SERVER (Go)                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐ │
│  │    API       │  │   WebSocket  │  │   Audio Processing       │ │
│  │  Handlers    │  │    Server    │  │      Pipeline            │ │
│  │              │  │              │  │                          │ │
│  │ - Auth       │  │ - Live Feed  │  │ - FFmpeg Transcoding    │ │
│  │ - Posts      │  │ - Presence   │  │ - Waveform Generation   │ │
│  │ - Social     │  │ - Chat       │  │ - S3 Upload             │ │
│  │ - Recommend  │  │              │  │                          │ │
│  └──────────────┘  └──────────────┘  └──────────────────────────┘ │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                    Integration Layer                         │  │
│  │  ┌────────────┐  ┌──────────────┐  ┌───────────────────┐   │  │
│  │  │ getstream  │  │    Gorse     │  │    PostgreSQL     │   │  │
│  │  │   (Feed)   │  │ (Recommend)  │  │   (Database)      │   │  │
│  │  └────────────┘  └──────────────┘  └───────────────────┘   │  │
│  │  ┌────────────┐  ┌──────────────┐  ┌───────────────────┐   │  │
│  │  │   Redis    │  │     S3       │  │     Stripe        │   │  │
│  │  │  (Cache)   │  │   (CDN)      │  │   (Payments)      │   │  │
│  │  └────────────┘  └──────────────┘  └───────────────────┘   │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Key Components

### Plugin Layer (C++/JUCE)

#### 1. **Audio Processing**
- **AudioCapture**: Records audio from DAW input bus
- **HttpAudioPlayer**: Streams and plays remote audio files
- **WaveformRenderer**: Generates visual waveforms

**Location**: `plugin/src/audio/`

#### 2. **UI Components**
- **PostsFeed**: Main social feed with infinite scroll
- **ProfileView**: User profiles and post grids
- **CommentsPanel**: Nested comment threads
- **RecordingView**: Audio capture interface
- **Header**: Navigation and user menu

**Location**: `plugin/src/ui/`

#### 3. **State Management (Reactive Stores)**
- **FeedStore**: Feed data with real-time updates
- **UserDataStore**: User profile and auth state
- **ReactiveBoundComponent**: Base class for reactive UI

**Pattern**: Observer pattern with automatic UI updates

**Location**: `plugin/src/stores/`

#### 4. **Network Layer**
- **NetworkClient**: HTTP/REST API client
- **RealtimeSync**: WebSocket client for live updates
- **Request Queue**: Retry logic and error handling

**Location**: `plugin/src/network/`

### Backend Layer (Go)

#### 1. **API Handlers**
- **Auth**: Login, signup, JWT token management
- **Posts**: Create, delete, upload audio
- **Social**: Like, comment, follow
- **Recommendations**: Personalized feeds (Gorse integration)
- **Feed**: Timeline, trending, discovery

**Location**: `backend/internal/handlers/`

#### 2. **Audio Processing**
- **FFmpeg Pipeline**: MP3 transcoding, normalization
- **Waveform Generator**: Peak detection, JSON output
- **S3 Uploader**: CDN distribution

**Location**: `backend/internal/audio/`

#### 3. **Integration Clients**
- **getstream.io**: Social feed infrastructure
- **Gorse**: Recommendation engine
- **PostgreSQL**: Primary database
- **Redis**: Caching layer
- **S3**: Audio file storage

**Location**: `backend/internal/`

---

## Architecture Documents

### [Gorse Recommendations](./gorse-recommendations.md)

Comprehensive guide to the AI-powered recommendation system:
- How recommendations work
- Feed types (For You, Popular, Latest, Discovery)
- CTR tracking and optimization
- Configuration and tuning
- Monitoring and debugging

**Read this if**: You're working on recommendation features or want to understand how content discovery works.

### Network Architecture (TODO)

Details on the plugin's networking layer:
- HTTP client implementation
- WebSocket real-time updates
- Request queue and retry logic
- Authentication flow
- Error handling strategies

### State Management (TODO)

Reactive state management using the Store pattern:
- Observer pattern implementation
- ReactiveBoundComponent lifecycle
- State update flow
- Best practices

### Audio Pipeline (TODO)

Audio processing from capture to playback:
- DAW audio bus integration
- Recording and encoding
- Waveform generation
- HTTP audio streaming
- Playback synchronization

---

## Design Principles

### 1. **Separation of Concerns**

Each component has a single, well-defined responsibility:

```cpp
// UI only handles presentation
class PostsFeed : public ReactiveBoundComponent {
    void paint(Graphics& g) override;  // Draw UI
    void handleFeedStateChanged();      // React to data changes
};

// Store handles state and business logic
class FeedStore : public Store<FeedStoreState> {
    void loadFeed(FeedType type);      // Fetch data
    void toggleLike(String postId);    // Business logic
};

// NetworkClient handles API communication
class NetworkClient {
    void getForYouFeed(int limit, FeedCallback callback);
    void likePost(String postId, ResponseCallback callback);
};
```

### 2. **Reactive State Management**

UI components automatically update when data changes:

```cpp
// Subscribe to store
auto unsubscribe = feedStore->subscribe([this](const FeedStoreState& state) {
    // UI updates automatically when state changes
    rebuildPostCards();
    repaint();
});
```

**Benefits**:
- No manual UI updates needed
- Consistent state across components
- Easy to reason about data flow

### 3. **Async-First Networking**

All network operations are asynchronous and non-blocking:

```cpp
// Never blocks the audio thread or UI thread
Async::runVoid([this, callback]() {
    auto response = makeRequest("/api/posts", "GET");

    // Return to UI thread for callback
    MessageManager::callAsync([callback, response]() {
        callback(Outcome<var>::ok(response));
    });
});
```

### 4. **Error Handling & Graceful Degradation**

System continues working even when services fail:

```cpp
// Gorse recommendations unavailable? Fall back to trending
auto posts = gorse->GetForYouFeed(userID);
if (posts.empty()) {
    posts = getTrendingFallback();
}
```

**Principles**:
- Never crash on network errors
- Provide meaningful error messages
- Fall back to cached data when offline
- Log errors for debugging

### 5. **Component Reusability**

UI components are modular and reusable:

```cpp
// PostCard can be used in any feed
class PostCard : public Component {
    std::function<void(const FeedPost&)> onPlayClicked;
    std::function<void(const String&)> onUserClicked;
    std::function<void(const FeedPost&)> onLikeClicked;
};

// Used in PostsFeed, ProfileView, SearchResults, etc.
postCard->onPlayClicked = [this](const FeedPost& post) {
    audioPlayer->play(post.audioUrl);
};
```

### 6. **Performance Optimization**

Critical operations are optimized for low latency:

```cpp
// Parallel network requests
std::vector<std::future<void>> futures;
futures.push_back(std::async([&]() { fetchPopular(); }));
futures.push_back(std::async([&]() { fetchLatest(); }));
futures.push_back(std::async([&]() { fetchForYou(); }));

for (auto& f : futures) f.wait();
```

**Key Optimizations**:
- Database query indexing
- Redis caching (feed data, user data)
- Gorse recommendation caching
- Lazy loading (infinite scroll)
- Image caching in plugin

---

## Technology Stack

### Plugin (Frontend)

| Technology | Purpose | Version |
|------------|---------|---------|
| **JUCE** | Audio plugin framework | 8.0.11 |
| **C++17** | Programming language | C++17 |
| **CMake** | Build system | 3.22+ |
| **Ninja** | Build generator (optional) | Latest |

**Key Libraries**:
- `juce_audio_processors` - VST/AU/AAX plugin support
- `juce_audio_devices` - Audio I/O
- `juce_graphics` - UI rendering
- `juce_data_structures` - JSON, XML parsing

### Backend (Server)

| Technology | Purpose | Version |
|------------|---------|---------|
| **Go** | Programming language | 1.21+ |
| **Gin** | Web framework | Latest |
| **GORM** | ORM for database | Latest |
| **PostgreSQL** | Primary database | 14+ |
| **Redis** | Cache layer | 7+ |

**Key Libraries**:
- `golang-jwt/jwt` - JWT authentication
- `gorilla/websocket` - WebSocket support
- `aws-sdk-go` - S3 file storage
- `ffmpeg-go` - Audio processing

### Infrastructure

| Service | Purpose | Provider |
|---------|---------|----------|
| **getstream.io** | Social feed API | Managed |
| **Gorse** | Recommendation engine | Self-hosted (Docker) |
| **S3** | Audio CDN | AWS |
| **PostgreSQL** | Database | Self-hosted (Docker) |
| **Redis** | Cache | Self-hosted (Docker) |

---

## Development Workflow

### Plugin Development

```bash
# Build plugin (smart - only regenerates when needed)
make plugin

# Build plugin (fast - skip project regeneration)
make plugin-fast

# Clean and rebuild
make plugin-clean && make plugin

# Build debug version
make plugin-debug
```

**Testing**:
1. Build plugin: `make plugin-fast`
2. Open DAW (Ableton Live, Logic Pro, etc.)
3. Load Sidechain as an instrument
4. Test audio capture, playback, feed loading

### Backend Development

```bash
# Start development server with hot reload
make backend-dev

# Run tests
make test-backend

# Manual run
cd backend
go run cmd/server/main.go
```

**Testing**:
```bash
# Unit tests
go test ./internal/...

# Integration tests
go test ./tests/integration/...

# Test specific handler
go test ./internal/handlers -run TestGetForYouFeed -v
```

### Full Stack Development

```bash
# Start entire stack (backend + database + gorse + redis)
make dev

# Or manually:
cd backend
docker-compose up -d
go run cmd/server/main.go
```

Then build and load plugin in DAW.

---

## Common Patterns

### Making API Requests (Plugin)

```cpp
void NetworkClient::getForYouFeed(int limit, int offset, FeedCallback callback) {
    if (!isAuthenticated()) {
        if (callback)
            callback(Outcome<var>::error("Not authenticated"));
        return;
    }

    Async::runVoid([this, limit, offset, callback]() {
        String endpoint = buildApiPath("/recommendations/for-you") +
                          "?limit=" + String(limit) + "&offset=" + String(offset);
        auto response = makeRequest(endpoint, "GET", var(), true);

        if (callback) {
            MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<var>::ok(response));
                else
                    callback(Outcome<var>::error("Invalid response"));
            });
        }
    });
}
```

### Implementing API Handlers (Backend)

```go
func (h *Handlers) GetForYouFeed(c *gin.Context) {
    userID := c.GetString("user_id")
    limit := parseIntDefault(c.Query("limit"), 20)
    offset := parseIntDefault(c.Query("offset"), 0)

    // Get recommendations from Gorse
    scores, err := h.gorse.GetForYouFeed(userID, limit, offset)
    if err != nil {
        c.JSON(500, gin.H{"error": "Failed to get recommendations"})
        return
    }

    // Track impressions for CTR analysis (async)
    go trackImpressions(userID, "for-you", scores)

    c.JSON(200, gin.H{
        "posts":     extractPosts(scores),
        "total":     len(scores),
        "has_more":  len(scores) == limit,
        "source":    "for-you",
    })
}
```

### Reactive UI Updates (Plugin)

```cpp
class PostsFeed : public ReactiveBoundComponent {
public:
    PostsFeed() {
        // Subscribe to store updates
        feedStore = &FeedStore::getInstance();
        storeUnsubscribe = feedStore->subscribe([this](const FeedStoreState& state) {
            handleFeedStateChanged();
        });
    }

    ~PostsFeed() override {
        // Unsubscribe to prevent memory leaks
        if (storeUnsubscribe)
            storeUnsubscribe();
    }

private:
    void handleFeedStateChanged() {
        const auto& state = feedStore->getState();
        const auto& feed = state.getCurrentFeed();

        if (feed.isLoading) {
            showLoadingState();
        } else if (!feed.error.isEmpty()) {
            showErrorState(feed.error);
        } else {
            rebuildPostCards();
            repaint();
        }
    }
};
```

---

## Next Steps

### For New Developers

1. **Read Architecture Docs**: Start with [Gorse Recommendations](./gorse-recommendations.md)
2. **Set Up Development Environment**: Follow `CLAUDE.md` setup guide
3. **Build and Run**: `make dev` (backend) and `make plugin` (plugin)
4. **Explore Codebase**: Start with `plugin/src/ui/feed/PostsFeed.cpp` (UI entry point)
5. **Make a Small Change**: Add a console log, rebuild, test

### For Contributing

1. **Pick an Issue**: Check GitHub issues or talk to team
2. **Create Feature Branch**: `git checkout -b feature/my-feature`
3. **Make Changes**: Follow existing patterns and conventions
4. **Test Thoroughly**: Plugin in DAW + backend unit tests
5. **Submit PR**: Include description, screenshots, testing notes

### For Production Deployment

1. **Build Release**: `make release`
2. **Package Plugin**: `make package` (creates installer)
3. **Deploy Backend**: Docker image to production server
4. **Configure DNS**: Point domain to backend server
5. **Monitor**: Check Gorse dashboard, logs, error tracking

---

## Resources

### Documentation
- [CLAUDE.md](../../CLAUDE.md) - Development commands and workflow
- [Gorse Recommendations](./gorse-recommendations.md) - Recommendation system
- [JUCE Documentation](https://docs.juce.com/) - Plugin framework
- [Gorse Documentation](https://docs.gorse.io/) - Recommendation engine

### Code Locations
- Plugin Source: `plugin/src/`
- Backend Source: `backend/internal/`
- API Routes: `backend/cmd/server/main.go`
- Database Models: `backend/internal/models/`

### External Services
- [getstream.io Dashboard](https://getstream.io/dashboard/)
- [Gorse Dashboard](http://localhost:8088) (local development)
- [AWS S3 Console](https://s3.console.aws.amazon.com/)

---

**Last Updated**: 2025-12-14
**Maintained By**: Sidechain Development Team
**Questions?**: Check GitHub issues or team Slack
