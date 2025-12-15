# Gorse Recommendation System Architecture

**Last Updated**: 2025-12-14
**Status**: Production Ready
**Version**: 1.0

---

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Data Flow](#data-flow)
4. [Components](#components)
5. [API Endpoints](#api-endpoints)
6. [Feed Types](#feed-types)
7. [CTR Tracking](#ctr-tracking)
8. [Configuration](#configuration)
9. [Monitoring](#monitoring)
10. [Debugging](#debugging)
11. [Performance](#performance)

---

## Overview

Sidechain uses [Gorse](https://gorse.io), an open-source recommender system, to provide personalized content discovery. The system tracks user behavior (plays, likes, downloads) and generates personalized recommendations in real-time.

### Key Features

- **Personalized Recommendations**: "For You" feed tailored to each user
- **Multiple Feed Types**: Popular, Latest, Discovery (blended)
- **Real-Time Feedback**: User interactions sync to Gorse immediately
- **CTR Tracking**: Measure recommendation quality per feed type
- **Negative Feedback**: "Not Interested", Skip, Hide signals
- **Category Filtering**: Filter by genre, BPM range
- **Diversity Tuning**: Mix of familiar + new content to prevent filter bubbles

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         VST PLUGIN (C++/JUCE)                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐ │
│  │  PostsFeed   │      │ NetworkClient│      │  FeedStore   │ │
│  │              │─────▶│              │◀─────│              │ │
│  │ - Discovery  │      │ - getDiscov..│      │ - Discovery  │ │
│  │ - Trending   │      │ - getPopular │      │ - Popular    │ │
│  │ - For You    │      │ - getLatest  │      │ - Latest     │ │
│  │ - Following  │      │ - trackClick │      │ - ForYou     │ │
│  └──────────────┘      └──────────────┘      └──────────────┘ │
│         │                      │                      │         │
│         └──────────────────────┼──────────────────────┘         │
│                                │                                │
└────────────────────────────────┼────────────────────────────────┘
                                 │ HTTP/REST
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                      BACKEND SERVER (Go)                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           Recommendation Handlers                        │  │
│  │  /recommendations/for-you                                │  │
│  │  /recommendations/popular                                │  │
│  │  /recommendations/latest                                 │  │
│  │  /recommendations/discovery-feed                         │  │
│  │  /recommendations/similar-posts/:id                      │  │
│  │  /recommendations/click                                  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                         │                                       │
│                         ▼                                       │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           GorseClient (gorse_rest.go)                    │  │
│  │  - SyncFeedback() - Real-time event tracking            │  │
│  │  - SyncItem() - Post metadata sync                      │  │
│  │  - SyncUser() - User preference sync                    │  │
│  │  - GetForYouFeed() - Personalized recommendations       │  │
│  │  - GetPopular() - Trending content                      │  │
│  │  - GetLatest() - Recent content                         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                         │                                       │
└─────────────────────────┼───────────────────────────────────────┘
                          │ Gorse REST API
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                     GORSE (Docker Container)                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐ │
│  │   Master     │  │   Server     │  │   Recommendation     │ │
│  │   Node       │  │   API        │  │   Engine             │ │
│  │              │  │              │  │                      │ │
│  │ - Training   │  │ - REST API   │  │ - Collaborative      │ │
│  │ - Indexing   │  │ - Caching    │  │   Filtering          │ │
│  │ - Jobs       │  │ - Serving    │  │ - CTR Prediction     │ │
│  └──────────────┘  └──────────────┘  │ - Item Neighbors     │ │
│         │                  │          └──────────────────────┘ │
│         │                  │                                    │
│         └──────────────────┼────────────────────────────────┐  │
│                            ▼                                │  │
│         ┌──────────────────────────────────────────┐        │  │
│         │          PostgreSQL (Data Store)         │        │  │
│         │  - Users, Items, Feedback                │        │  │
│         │  - Model parameters                      │        │  │
│         └──────────────────────────────────────────┘        │  │
│                            ▲                                │  │
│         ┌──────────────────┴────────────────────────────────┘  │
│         │          Redis (Cache Store)                         │
│         │  - Recommendation cache                              │
│         │  - User/item metadata cache                          │
│         └──────────────────────────────────────────────────────┘
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                   SIDECHAIN DATABASE (PostgreSQL)               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────┐  ┌────────────────────────────────┐  │
│  │  audio_posts         │  │  recommendation_impressions    │  │
│  │  - id, user_id       │  │  - id, user_id, post_id        │  │
│  │  - genre, bpm        │  │  - source, position, score     │  │
│  │  - audio_url         │  │  - clicked, clicked_at         │  │
│  └──────────────────────┘  └────────────────────────────────┘  │
│                                                                 │
│  ┌──────────────────────┐  ┌────────────────────────────────┐  │
│  │  users               │  │  recommendation_clicks         │  │
│  │  - id, username      │  │  - id, user_id, post_id        │  │
│  │  - favorite_genres   │  │  - play_duration, completed    │  │
│  └──────────────────────┘  └────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Data Flow

### 1. Real-Time Feedback Sync

When a user interacts with content, the backend immediately syncs feedback to Gorse:

```
User Action (Plugin) → Backend Handler → GorseClient.SyncFeedback() → Gorse API
```

**Example**: User plays a post
```go
// backend/internal/handlers/feed.go
func (h *Handlers) TrackPlay(c *gin.Context) {
    // ... validate request ...

    // Async sync to Gorse (doesn't block response)
    if playHistory.Completed {
        go h.gorse.SyncFeedback(userID, postID, "like")
    } else {
        go h.gorse.SyncFeedback(userID, postID, "view")
    }

    c.JSON(200, response)
}
```

### 2. Recommendation Generation

When a user opens a feed, the plugin requests recommendations:

```
Plugin (PostsFeed) → FeedStore → NetworkClient → Backend API →
    GorseClient → Gorse API → Recommendation Engine → Response
```

**Example**: Loading "For You" feed
```cpp
// plugin/src/ui/feed/PostsFeed.cpp
void PostsFeed::switchFeedType(FeedType::ForYou) {
    feedStore->switchFeedType(FeedType::ForYou);
    // FeedStore calls NetworkClient.getForYouFeed()
}
```

```go
// backend/internal/recommendations/gorse_rest.go
func (gc *GorseClient) GetForYouFeed(userID string, limit, offset int) ([]PostScore, error) {
    url := fmt.Sprintf("%s/api/recommend/%s?n=%d&offset=%d",
        gc.baseURL, userID, limit, offset)

    // Gorse returns scored recommendations
    resp := gc.makeRequest("GET", url)

    // Convert to PostScore with reasons
    return parseRecommendations(resp)
}
```

### 3. CTR Tracking Loop

Tracks which recommendations users click to measure quality:

```
Show Recommendations → Track Impressions (async) → User Clicks →
    Track Click → Link to Impression → Calculate CTR → Tune System
```

**Example**: Impression tracking
```go
// backend/internal/handlers/recommendations.go
func trackImpressions(userID, source string, scores []PostScore) {
    impressions := make([]models.RecommendationImpression, len(scores))
    for i, score := range scores {
        impressions[i] = models.RecommendationImpression{
            UserID:   userID,
            PostID:   score.Post.ID,
            Source:   source, // "for-you", "popular", "discovery"
            Position: i,
            Score:    &score.Score,
            Reason:   &score.Reason,
        }
    }

    // Async batch insert
    go database.DB.CreateInBatches(&impressions, 100)
}
```

### 4. Background Sync Jobs

Ensures data consistency even if real-time sync fails:

```
Every Hour → Batch Sync Job → Sync All Users/Items/Feedback → Gorse
```

**Example**: Startup sync
```go
// backend/cmd/server/main.go
go func() {
    log.Println("🔄 Running initial Gorse sync...")
    gorse.BatchSyncUsers()
    gorse.BatchSyncItems()
    gorse.BatchSyncFeedback()
    log.Println("✅ Initial sync complete")
}()
```

---

## Components

### Backend Components

#### 1. **GorseClient** (`backend/internal/recommendations/gorse_rest.go`)

The main interface to Gorse. Handles all API communication.

**Key Methods**:
- `SyncFeedback(userID, itemID, feedbackType string)` - Track user interactions
- `SyncItem(post *models.AudioPost)` - Update post metadata
- `SyncUser(user *models.User)` - Update user preferences
- `GetForYouFeed(userID string, limit, offset int)` - Get personalized recommendations
- `GetPopular(limit, offset int)` - Get trending content
- `GetLatest(limit, offset int)` - Get recent content
- `GetSimilarPosts(postID string, limit int)` - Get similar items

**Configuration**:
```go
type GorseClient struct {
    baseURL    string // http://gorse:8087
    apiKey     string // from GORSE_API_KEY env
    httpClient *http.Client
}
```

#### 2. **Recommendation Handlers** (`backend/internal/handlers/recommendations.go`)

HTTP handlers that serve recommendation endpoints to the plugin.

**Key Endpoints**:
- `GET /api/v1/recommendations/for-you` - Personalized feed
- `GET /api/v1/recommendations/popular` - Popular posts
- `GET /api/v1/recommendations/latest` - Latest posts
- `GET /api/v1/recommendations/discovery-feed` - Blended discovery
- `GET /api/v1/recommendations/similar-posts/:id` - Similar items
- `POST /api/v1/recommendations/click` - Track clicks for CTR

**Discovery Feed Algorithm**:
```go
// Fetch in parallel
var popular, latest, forYou []PostScore
wg.Add(3)
go fetchPopular(&popular)
go fetchLatest(&latest)
go fetchForYou(&forYou)
wg.Wait()

// Blend: 30% popular, 20% latest, 50% personalized
blended := interleave(
    popular[:popularCount],
    latest[:latestCount],
    forYou[:forYouCount],
)
```

#### 3. **CTR Analytics** (`backend/internal/recommendations/ctr.go`)

Calculates click-through rates to measure recommendation quality.

**CTR Calculation**:
```go
type CTRMetric struct {
    Source      string  // "for-you", "popular", "discovery"
    Impressions int64   // How many times shown
    Clicks      int64   // How many times clicked
    CTR         float64 // (clicks / impressions) * 100
    Date        time.Time
}

func CalculateCTR(db *gorm.DB, since time.Time) ([]CTRMetric, error) {
    // Query impressions and clicks from database
    // Calculate CTR per source
    // Return metrics for analysis
}
```

### Plugin Components

#### 1. **NetworkClient** (`plugin/src/network/api/FeedClient.cpp`)

C++ client for calling backend recommendation APIs.

**Key Methods**:
```cpp
void getForYouFeed(int limit, int offset, FeedCallback callback);
void getPopularFeed(int limit, int offset, FeedCallback callback);
void getLatestFeed(int limit, int offset, FeedCallback callback);
void getDiscoveryFeed(int limit, int offset, FeedCallback callback);
void getSimilarPosts(const String& postId, int limit, FeedCallback callback);
void trackRecommendationClick(const String& postId, const String& source,
                               int position, double playDuration,
                               bool completed, ResponseCallback callback);
```

**Implementation Pattern**:
```cpp
void NetworkClient::getDiscoveryFeed(int limit, int offset, FeedCallback callback) {
    Async::runVoid([this, limit, offset, callback]() {
        String endpoint = buildApiPath("/recommendations/discovery-feed") +
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

#### 2. **FeedStore** (`plugin/src/stores/FeedStore.h/cpp`)

Reactive state management for feed data.

**Feed Types**:
```cpp
enum class FeedType {
    Timeline,    // Following feed (users you follow)
    Global,      // All public posts
    Trending,    // Engagement-based trending
    ForYou,      // Gorse personalized recommendations
    Popular,     // Gorse popular/trending
    Latest,      // Gorse latest posts
    Discovery,   // Gorse blended discovery (30% popular, 20% latest, 50% personalized)
    // ... aggregated feeds
};
```

**Feed Loading**:
```cpp
void FeedStore::performFetch(FeedType feedType, int limit, int offset) {
    auto callback = [this, feedType, limit, offset](Outcome<var> result) {
        if (result.isOk())
            handleFetchSuccess(feedType, result.getValue(), limit, offset);
        else
            handleFetchError(feedType, result.getError());
    };

    switch (feedType) {
        case FeedType::ForYou:
            networkClient->getForYouFeed(limit, offset, callback);
            break;
        case FeedType::Popular:
            networkClient->getPopularFeed(limit, offset, callback);
            break;
        case FeedType::Discovery:
            networkClient->getDiscoveryFeed(limit, offset, callback);
            break;
        // ...
    }
}
```

#### 3. **PostsFeed UI** (`plugin/src/ui/feed/PostsFeed.cpp`)

Main feed UI component. Displays recommendations and tracks clicks.

**CTR Tracking**:
```cpp
audioPlayer->onPlaybackStarted = [this](const String& postId) {
    // Track play event
    networkClient->trackPlay(postId, ...);

    // Track recommendation click for CTR
    if (feedStore != nullptr) {
        auto currentFeedType = feedStore->getCurrentFeedType();
        String source = mapFeedTypeToSource(currentFeedType); // "for-you", "popular", etc.
        int position = findPostPosition(postId);

        if (source != "unknown" && position >= 0) {
            networkClient->trackRecommendationClick(postId, source, position, 0.0, false);
        }
    }
};
```

---

## API Endpoints

### Recommendation Endpoints

#### GET /api/v1/recommendations/for-you

Personalized recommendations based on user's listening history.

**Query Parameters**:
- `limit` (optional, default=20) - Number of recommendations
- `offset` (optional, default=0) - Pagination offset
- `genre` (optional) - Filter by genre (e.g., "electronic", "hip-hop")
- `min_bpm` (optional) - Minimum BPM filter
- `max_bpm` (optional) - Maximum BPM filter

**Response**:
```json
{
  "posts": [
    {
      "id": "post_123",
      "user_id": "user_456",
      "audio_url": "https://...",
      "genre": "electronic",
      "bpm": 128,
      "recommendation_score": 0.95,
      "recommendation_reason": "Similar to posts you liked in the past"
    }
  ],
  "total": 100,
  "has_more": true,
  "source": "for-you",
  "filters": {
    "genre": "electronic"
  }
}
```

#### GET /api/v1/recommendations/popular

Popular/trending posts based on engagement.

**Query Parameters**:
- `limit` (optional, default=20)
- `offset` (optional, default=0)

**Response**: Same structure as above with `source: "popular"`

#### GET /api/v1/recommendations/latest

Recently added posts.

**Query Parameters**:
- `limit` (optional, default=20)
- `offset` (optional, default=0)

**Response**: Same structure as above with `source: "latest"`

#### GET /api/v1/recommendations/discovery-feed

Blended discovery feed (30% popular, 20% latest, 50% personalized).

**Query Parameters**:
- `limit` (optional, default=20)
- `offset` (optional, default=0)

**Response**:
```json
{
  "posts": [...],
  "total": 100,
  "has_more": true,
  "source": "discovery",
  "blend_info": {
    "popular_count": 6,
    "latest_count": 4,
    "personalized_count": 10,
    "popular_percentage": 30,
    "latest_percentage": 20,
    "personalized_percentage": 50
  }
}
```

#### GET /api/v1/recommendations/similar-posts/:id

Similar posts to a given post.

**Path Parameters**:
- `id` - Post ID

**Query Parameters**:
- `limit` (optional, default=20)
- `genre` (optional) - Filter similar posts by genre

**Response**: Same structure as for-you

#### POST /api/v1/recommendations/click

Track a recommendation click for CTR analysis.

**Request Body**:
```json
{
  "post_id": "post_123",
  "source": "for-you",
  "position": 5,
  "play_duration": 0.0,
  "completed": false
}
```

**Response**:
```json
{
  "success": true,
  "click_id": "click_789",
  "impression_id": "impression_456"
}
```

### Feedback Endpoints

#### POST /api/v1/recommendations/dislike/:post_id

Mark a post as "Not Interested".

**Response**: `{ "success": true }`

#### POST /api/v1/recommendations/skip/:post_id

Track that user skipped past a post.

**Response**: `{ "success": true }`

#### POST /api/v1/recommendations/hide/:post_id

Hide a post from recommendations (strongest negative signal).

**Response**: `{ "success": true }`

### Metrics Endpoints

#### GET /api/v1/recommendations/metrics/ctr

Get CTR metrics for all recommendation sources.

**Query Parameters**:
- `period` (optional, default="24h") - Time period ("24h", "7d", "30d")

**Response**:
```json
{
  "period": "24h",
  "metrics": [
    {
      "source": "for-you",
      "impressions": 1000,
      "clicks": 150,
      "ctr": 15.0
    },
    {
      "source": "popular",
      "impressions": 500,
      "clicks": 50,
      "ctr": 10.0
    },
    {
      "source": "discovery",
      "impressions": 800,
      "clicks": 120,
      "ctr": 15.0
    }
  ]
}
```

---

## Feed Types

### Comparison

| Feed Type | Source | Algorithm | Use Case |
|-----------|--------|-----------|----------|
| **Timeline** | getstream.io | Following graph | See posts from followed users |
| **Trending** | Backend | Engagement score | What's hot right now |
| **For You** | Gorse | Collaborative filtering + CTR | Personalized recommendations |
| **Popular** | Gorse | Engagement-based | Trending content (Gorse version) |
| **Latest** | Gorse | Timestamp-based | Recently added content |
| **Discovery** | Gorse | Blended (30/20/50) | Balanced discovery experience |
| **Global** | Backend | All public posts | Everything on the platform |

### When to Use Each Feed

**For You**:
- Best for returning users with interaction history
- Personalized based on likes, plays, downloads
- Updates in real-time as user behavior changes

**Discovery**:
- Best default feed for all users
- Balances familiar content with new discoveries
- Prevents filter bubbles with 30% popular + 20% latest mix

**Popular**:
- Good for new users (cold start)
- Shows what's trending platform-wide
- Updated hourly as engagement changes

**Latest**:
- Good for power users who want fresh content
- See newest posts first
- No personalization

**Trending**:
- Legacy engagement-based feed
- Eventually can be replaced by Popular

---

## CTR Tracking

### Overview

CTR (Click-Through Rate) measures recommendation quality:

```
CTR = (Clicks / Impressions) × 100
```

**Example**: If 1000 recommendations shown and 150 clicked, CTR = 15%

### Implementation

#### 1. Track Impressions

When recommendations are shown to user:

```go
// backend/internal/handlers/recommendations.go
func trackImpressions(userID, source string, scores []PostScore) {
    for i, score := range scores {
        impression := models.RecommendationImpression{
            UserID:   userID,
            PostID:   score.Post.ID,
            Source:   source,      // "for-you", "popular", "discovery"
            Position: i,            // 0, 1, 2, ...
            Score:    score.Score,  // Gorse confidence score
            Reason:   score.Reason, // Why recommended
        }
        // Batch insert (async, non-blocking)
    }
}
```

#### 2. Track Clicks

When user plays a recommended post:

```cpp
// plugin/src/ui/feed/PostsFeed.cpp
audioPlayer->onPlaybackStarted = [this](const String& postId) {
    // Map FeedType to source string
    String source = currentFeedType == FeedType::ForYou ? "for-you" :
                    currentFeedType == FeedType::Popular ? "popular" :
                    currentFeedType == FeedType::Discovery ? "discovery" : "unknown";

    // Find position in feed
    int position = findPostPosition(postId);

    // Track click
    networkClient->trackRecommendationClick(postId, source, position, 0.0, false);
};
```

Backend handler:

```go
// backend/internal/handlers/recommendations.go
func TrackRecommendationClick(c *gin.Context) {
    click := models.RecommendationClick{
        UserID:       userID,
        PostID:       req.PostID,
        Source:       req.Source,
        Position:     req.Position,
        PlayDuration: req.PlayDuration,
        Completed:    req.Completed,
    }

    // Find matching impression
    impression := findImpression(userID, req.PostID, req.Source)
    if impression != nil {
        click.RecommendationImpressionID = &impression.ID
        impression.Clicked = true
        impression.ClickedAt = now
    }

    // Send completed plays to Gorse as "like" feedback
    if req.Completed {
        go h.gorse.SyncFeedback(userID, req.PostID, "like")
    }
}
```

#### 3. Calculate CTR

Daily background job:

```go
// backend/cmd/server/main.go
go func() {
    ticker := time.NewTicker(24 * time.Hour)
    for {
        select {
        case <-ticker.C:
            recommendations.LogCTRMetrics(database.DB)
        }
    }
}()
```

```go
// backend/internal/recommendations/ctr.go
func LogCTRMetrics(db *gorm.DB) {
    since := time.Now().Add(-24 * time.Hour)
    metrics, _ := CalculateCTR(db, since)

    for _, m := range metrics {
        log.Printf("📊 CTR [%s]: %.2f%% (%d clicks / %d impressions)",
            m.Source, m.CTR, m.Clicks, m.Impressions)
    }
}
```

### CTR Benchmarks

**Good CTR Values**:
- For You: 15-25% (personalized should perform best)
- Discovery: 12-20% (blended, should be high)
- Popular: 10-15% (cold start, less personalized)
- Latest: 5-10% (not personalized, just chronological)

**Red Flags**:
- CTR < 5%: Recommendations are poor quality
- CTR decreasing over time: Model needs retraining
- CTR varies wildly: Inconsistent user experience

---

## Configuration

### Gorse Configuration (`backend/gorse.toml`)

#### Database Settings

```toml
[database]
data_store = "postgres://sidechain:password@postgres:5432/gorse?sslmode=disable"
cache_store = "redis://redis:6379"
```

#### Feedback Types

```toml
[recommend.data_source]
positive_feedback_types = ["like", "follow", "download"]
read_feedback_types = ["view", "play"]
negative_feedback_types = ["dislike", "skip", "hide"]

positive_feedback_ttl = 365  # Decay likes after 1 year
read_feedback_ttl = 90        # Decay views after 90 days
```

#### Recommendation Diversity

```toml
[recommend.offline]
enable_popular_recommendation = true
enable_latest_recommendation = true
enable_collaborative_recommendation = true
enable_click_through_prediction = true

# Mix in 10% popular + 20% latest to prevent filter bubbles
explore_recommend = {"popular": 0.1, "latest": 0.2}

# Retrain models regularly
model_fit_period = "60m"      # Every hour
model_search_period = "720m"  # Every 12 hours
```

#### Online Settings

```toml
[recommend.online]
# Fallback if not enough personalized recommendations
fallback_recommend = ["popular", "latest"]
```

### Backend Environment Variables

```bash
# Gorse Connection
GORSE_API_URL=http://gorse:8087
GORSE_API_KEY=sidechain_gorse_api_key

# Sync Interval (hourly by default)
GORSE_SYNC_INTERVAL=1h

# Database
DATABASE_URL=postgres://sidechain:password@postgres:5432/sidechain
```

### Plugin Configuration

No configuration needed - uses backend API endpoints.

---

## Monitoring

### Gorse Dashboard

Access at: `http://localhost:8088` (development)

**Key Metrics to Monitor**:

1. **Active Users**: Should be 80%+ of registered users
2. **Active Items**: Should match number of public posts
3. **Feedback Events/Day**: Should be 100+ (more is better)
4. **Recommendation Cache Hit Rate**: 70-90% is good
5. **Model Performance**: Check CTR prediction accuracy

### Health Checks

#### Check Gorse is Running

```bash
curl http://localhost:8087/api/health
# Should return: {"ready": true}
```

#### Check Recommendation Quality

```bash
# Get CTR metrics
curl -H "Authorization: Bearer $TOKEN" \
  http://localhost:8080/api/v1/recommendations/metrics/ctr?period=24h

# Sample output:
# {
#   "metrics": [
#     {"source": "for-you", "ctr": 15.2, "impressions": 1000, "clicks": 152},
#     {"source": "popular", "ctr": 10.5, "impressions": 500, "clicks": 52}
#   ]
# }
```

#### Check Data Sync

```bash
# Check user count in Gorse
curl -X GET http://localhost:8087/api/users \
  -H "X-API-Key: sidechain_gorse_api_key"

# Check item count in Gorse
curl -X GET http://localhost:8087/api/items \
  -H "X-API-Key: sidechain_gorse_api_key"
```

### Logs to Watch

**Backend Logs**:
```
🔄 Running initial Gorse sync...
✅ Synced 150 users to Gorse
✅ Synced 500 items to Gorse
✅ Synced 2000 feedback events to Gorse
📊 CTR [for-you]: 15.20% (152 clicks / 1000 impressions)
```

**Gorse Logs**:
```
[INFO] Model training started
[INFO] Model training completed: CTR=0.15
[INFO] Recommendation cache refreshed
```

---

## Debugging

### Common Issues

#### 1. Recommendations are Empty

**Symptoms**: `for-you` feed returns empty array

**Causes**:
- User has no interaction history (cold start)
- Gorse hasn't trained models yet
- Data sync failed

**Debug**:
```bash
# Check if user exists in Gorse
curl -X GET http://localhost:8087/api/user/$USER_ID \
  -H "X-API-Key: sidechain_gorse_api_key"

# Check feedback count
curl -X GET http://localhost:8087/api/user/$USER_ID/feedback \
  -H "X-API-Key: sidechain_gorse_api_key"

# If empty, trigger sync
curl -X POST http://localhost:8080/api/admin/sync-gorse \
  -H "Authorization: Bearer $ADMIN_TOKEN"
```

**Solution**: Use `discovery` or `popular` feed for cold start users.

#### 2. Recommendations Don't Update

**Symptoms**: Same recommendations shown repeatedly

**Causes**:
- Cache not refreshing
- Real-time feedback sync disabled
- Model not retraining

**Debug**:
```toml
# Check gorse.toml settings
[recommend]
refresh_recommend_period = "1h"  # How often cache refreshes

[recommend.offline]
model_fit_period = "60m"  # How often model retrains
```

**Solution**: Reduce `refresh_recommend_period` or manually clear cache.

#### 3. CTR is Very Low (< 5%)

**Symptoms**: Users rarely click recommendations

**Causes**:
- Poor model quality
- Not enough training data
- Incorrect feedback types

**Debug**:
```bash
# Check CTR per source
curl http://localhost:8080/api/v1/recommendations/metrics/ctr?period=7d

# Check feedback counts
SELECT feedback_type, COUNT(*)
FROM gorse_feedback
GROUP BY feedback_type;
```

**Solution**:
- Collect more interaction data (likes, plays)
- Verify feedback types are configured correctly
- Wait for more model training cycles

#### 4. Gorse API Errors

**Symptoms**: `500 Internal Server Error` from recommendation endpoints

**Causes**:
- Gorse container not running
- Database connection failed
- API key mismatch

**Debug**:
```bash
# Check Gorse container
docker ps | grep gorse

# Check Gorse logs
docker logs sidechain-gorse

# Test Gorse API directly
curl -X GET http://localhost:8087/api/health
```

**Solution**: Restart Gorse container, check database connection.

### Logging

Enable debug logging:

**Backend**:
```bash
export LOG_LEVEL=debug
go run cmd/server/main.go
```

**Gorse** (`gorse.toml`):
```toml
[master]
verbose = 1  # 0=error, 1=info, 2=debug
```

---

## Performance

### Latency Targets

| Operation | Target | Notes |
|-----------|--------|-------|
| Get recommendations | < 200ms | Gorse cache hit |
| Get recommendations (cache miss) | < 1s | Gorse model inference |
| Track feedback | < 50ms | Async, non-blocking |
| CTR calculation | < 5s | Background job only |

### Optimization Strategies

#### 1. Caching

Gorse has built-in Redis caching:

```toml
[recommend]
cache_size = 100  # Cache top 100 recommendations per user
refresh_recommend_period = "1h"
```

**Tradeoff**: Fresher recommendations vs lower latency

#### 2. Batch Operations

Sync feedback in batches:

```go
// backend/internal/recommendations/gorse_rest.go
func (gc *GorseClient) BatchSyncFeedback(events []FeedbackEvent) error {
    // Send up to 1000 events at once
    chunks := chunkSlice(events, 1000)
    for _, chunk := range chunks {
        gc.makeRequest("PUT", "/api/feedback", chunk)
    }
}
```

#### 3. Parallel Fetching

Discovery feed fetches sources in parallel:

```go
var wg sync.WaitGroup
wg.Add(3)

go func() {
    defer wg.Done()
    popular = gc.GetPopular(limit, 0)
}()
go func() {
    defer wg.Done()
    latest = gc.GetLatest(limit, 0)
}()
go func() {
    defer wg.Done()
    forYou = gc.GetForYouFeed(userID, limit, 0)
}()

wg.Wait()
```

#### 4. Database Indexing

Ensure indexes exist:

```sql
-- Recommendation impressions
CREATE INDEX idx_impressions_user_source ON recommendation_impressions(user_id, source);
CREATE INDEX idx_impressions_post ON recommendation_impressions(post_id);
CREATE INDEX idx_impressions_clicked ON recommendation_impressions(clicked);

-- Recommendation clicks
CREATE INDEX idx_clicks_user ON recommendation_clicks(user_id);
CREATE INDEX idx_clicks_post ON recommendation_clicks(post_id);
```

#### 5. Async Operations

Never block user requests:

```go
// GOOD: Async feedback sync
go h.gorse.SyncFeedback(userID, postID, "like")
c.JSON(200, response)

// BAD: Blocking sync
h.gorse.SyncFeedback(userID, postID, "like")  // Blocks request!
c.JSON(200, response)
```

### Scaling

**Horizontal Scaling** (future):
- Multiple Gorse server nodes behind load balancer
- Worker nodes for offline training
- Distributed cache (Redis cluster)

**Vertical Scaling**:
- Increase Gorse container resources
- Tune PostgreSQL for recommendation queries
- Increase Redis cache size

---

## Summary

The Gorse recommendation system provides personalized, high-quality content discovery for Sidechain users. Key features:

✅ **Real-time feedback sync** - User actions immediately influence recommendations
✅ **Multiple feed types** - For You, Popular, Latest, Discovery
✅ **CTR tracking** - Measure and optimize recommendation quality
✅ **Negative feedback** - Users can hide/skip unwanted content
✅ **Category filtering** - Filter by genre, BPM range
✅ **Diversity tuning** - Prevent filter bubbles with exploration mix
✅ **Production-ready** - Monitoring, error handling, graceful degradation

**Next Steps**:
- Collect baseline CTR metrics with live users
- Monitor Gorse dashboard for system health
- Tune exploration/exploitation ratio based on engagement data
- Consider context-aware recommendations (time of day, mood)

---

**References**:
- [Gorse Documentation](https://docs.gorse.io/)
- [Gorse REST API](https://docs.gorse.io/api/restful-api.html)
- [Collaborative Filtering Explained](https://en.wikipedia.org/wiki/Collaborative_filtering)
- Backend Code: `/backend/internal/recommendations/`
- Plugin Code: `/plugin/src/network/api/FeedClient.cpp`
- Configuration: `/backend/gorse.toml`
