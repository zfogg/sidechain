# Sidechain Backend 🎛️

> The social engine powering music producer stories

The Sidechain backend is a Go-based API server that enables real-time social music sharing directly from DAWs. It bridges the gap between individual music production and community discovery.

## The Vision

**"Instagram Stories, but for beats"**

Traditional social platforms don't understand the music production process:
- **SoundCloud**: Antisocial, focused on finished tracks
- **Instagram**: Great for photos, terrible for audio
- **TikTok**: For final content, not the creative process
- **Discord**: Text-heavy, no audio-first design

**Sidechain solves this** by embedding social discovery directly into the producer's workflow:
- Highlight 8 bars in Ableton → One click → Posted to feed
- Infinite scroll of loops from other producers
- Audio previews through your existing monitors
- Real-time "🟢 Producing Now" presence
- Emoji reactions optimized for music (🎵🔥❤️💯)

## Architecture Overview

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   VST Plugin    │    │  Sidechain API  │    │   getstream.io     │
│                 │────│    (Go)         │────│   (Social)      │
│ Audio Capture   │    │ Authentication  │    │ Feeds & Chat    │
│ Social Feed UI  │    │ Upload Pipeline │    │ Real-time       │
│ Real-time       │    │ Audio Processing│    │ Reactions       │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │
                                │
                       ┌─────────────────┐
                       │   CDN Storage   │
                       │  (R2/S3 + CDN)  │
                       │ Audio + Assets  │
                       └─────────────────┘
```

## Core Services

### 🔐 Authentication Service
- **Device Registration**: Generate unique IDs for VST instances
- **Web Claiming Flow**: OAuth via Google/Discord to claim devices
- **JWT Token Management**: Secure session handling
- **Stream.io User Sync**: Automatic user creation in social graph

### 🎵 Audio Processing Pipeline
- **Multipart Upload**: Handle raw audio from VST clients
- **Format Conversion**: Encode to optimized MP3 (128kbps)
- **Loudness Normalization**: Standardize to -14 LUFS
- **Waveform Generation**: Create SVG visualizations
- **Metadata Extraction**: BPM, key, duration detection
- **CDN Distribution**: Global audio delivery

### 📱 Social Graph Engine (Stream.io Integration)
- **Activity Feeds**: User, timeline, global, notification feeds
- **Follow System**: Producer-to-producer connections
- **Engagement**: Emoji reactions with custom data
- **Real-time Updates**: WebSocket-based live features
- **Content Moderation**: Automated and manual content review

### ⚡ Real-time Presence System
- **Producer Status**: "🟢 Live in Studio" indicators
- **Activity Broadcasting**: "3 friends producing now"
- **Live Notifications**: New posts, reactions, follows
- **Connection Management**: Heartbeat and retry logic

## API Endpoints

### Authentication
```
POST /api/v1/auth/device          # Register new device
POST /api/v1/auth/claim           # Claim device via web
POST /api/v1/auth/verify          # Verify JWT token
GET  /api/v1/auth/oauth/google    # Google OAuth redirect
GET  /api/v1/auth/oauth/discord   # Discord OAuth redirect
```

### Audio Management
```
POST /api/v1/audio/upload         # Upload raw audio + metadata
GET  /api/v1/audio/:id            # Get audio file info
POST /api/v1/audio/process        # Trigger audio processing
GET  /api/v1/audio/:id/waveform   # Get waveform SVG
```

### Social Features
```
GET  /api/v1/feed/global          # Global discovery feed
GET  /api/v1/feed/timeline        # Following feed
GET  /api/v1/feed/user/:id        # User's posts
POST /api/v1/social/follow        # Follow producer
POST /api/v1/social/unfollow      # Unfollow producer
POST /api/v1/social/like          # Like post (with optional emoji)
POST /api/v1/social/react         # Emoji reaction (🎵❤️🔥😍🚀💯)
DELETE /api/v1/social/react/:id   # Remove reaction
```

### User Management
```
GET  /api/v1/users/me             # Current user profile
PUT  /api/v1/users/me             # Update profile
GET  /api/v1/users/:id            # Public profile
GET  /api/v1/users/:id/followers  # Follower list
GET  /api/v1/users/:id/following  # Following list
```

### Real-time (WebSocket)
```
WS   /api/v1/ws                   # WebSocket connection
     - presence.update            # Producer status changes
     - feed.new_post             # New posts in timeline
     - social.new_reaction       # Reactions on your posts
     - social.new_follow         # New followers
```

## Data Models

### Audio Post
```go
type AudioPost struct {
    ID           string    `json:"id"`
    UserID       string    `json:"user_id"`
    AudioURL     string    `json:"audio_url"`
    WaveformSVG  string    `json:"waveform_svg"`
    BPM          int       `json:"bpm"`
    Key          string    `json:"key"`
    Genre        []string  `json:"genre"`
    DAW          string    `json:"daw"`
    DurationBars int       `json:"duration_bars"`
    PlayCount    int       `json:"play_count"`
    LikeCount    int       `json:"like_count"`
    CreatedAt    time.Time `json:"created_at"`
}
```

### Producer Profile
```go
type Producer struct {
    ID            string    `json:"id"`
    Username      string    `json:"username"`
    DisplayName   string    `json:"display_name"`
    Bio           string    `json:"bio"`
    FollowerCount int       `json:"follower_count"`
    FollowingCount int      `json:"following_count"`
    PostCount     int       `json:"post_count"`
    IsLive        bool      `json:"is_live"`
    LastActive    time.Time `json:"last_active"`
    DAWPreference string    `json:"daw_preference"`
}
```

### Emoji Reactions
```go
type Reaction struct {
    ID         string                 `json:"id"`
    UserID     string                 `json:"user_id"`
    PostID     string                 `json:"post_id"`
    Type       string                 `json:"type"`        // "like", "fire", "music"
    Emoji      string                 `json:"emoji"`       // "❤️", "🔥", "🎵"
    CustomData map[string]interface{} `json:"custom_data"`
    CreatedAt  time.Time              `json:"created_at"`
}
```

## Technical Implementation

### Phase 1: Authentication & Foundation ✅
- [x] Device ID generation and registration
- [x] getstream.io client integration 
- [x] Basic API endpoints
- [x] JWT token handling
- [ ] OAuth web flow (Google/Discord)
- [ ] Device claiming interface

### Phase 2: Audio Pipeline 🔄
- [ ] Multipart file upload handling
- [ ] LAME MP3 encoding integration
- [ ] FFmpeg loudness normalization (-14 LUFS)
- [ ] SVG waveform generation
- [ ] CDN upload (Cloudflare R2/AWS S3)
- [ ] Audio metadata extraction

### Phase 3: Social Engine 🔄
- [x] getstream.io V3 feeds integration
- [x] Emoji reaction system
- [ ] Feed algorithms and ranking
- [ ] Content caching layer
- [ ] Notification system

### Phase 4: Real-time Features ⏳
- [ ] WebSocket server implementation
- [ ] Producer presence tracking
- [ ] Live notification broadcasting
- [ ] Connection management with retry logic

## Environment Configuration

The backend requires these environment variables:

```bash
# getstream.io (Social Engine)
STREAM_API_KEY=your_stream_api_key
STREAM_API_SECRET=your_stream_api_secret

# Audio Storage (CDN)
STORAGE_PROVIDER=r2  # r2, s3, local
CLOUDFLARE_R2_ENDPOINT=
CLOUDFLARE_R2_ACCESS_KEY=
CLOUDFLARE_R2_SECRET_KEY=
CLOUDFLARE_R2_BUCKET=sidechain-audio

# Database (Metadata)
DATABASE_URL=postgres://user:pass@localhost/sidechain?sslmode=disable

# Audio Processing
AUDIO_PROCESSING_ENABLED=true
FFMPEG_PATH=/usr/local/bin/ffmpeg
LAME_PATH=/usr/local/bin/lame
TARGET_LOUDNESS=-14  # LUFS

# OAuth Providers
GOOGLE_CLIENT_ID=
GOOGLE_CLIENT_SECRET=
DISCORD_CLIENT_ID=
DISCORD_CLIENT_SECRET=

# Security
JWT_SECRET=your_jwt_secret_here
CORS_ORIGINS=*

# Performance
MAX_UPLOAD_SIZE=50MB
AUDIO_CACHE_TTL=3600
FEED_CACHE_TTL=300
```

## Key Features

### 🎯 Producer-First Design
- **Zero Friction Sharing**: Highlight → Click → Posted
- **No Export Workflow**: Direct from session buffer
- **DAW Integration**: Respects producer workflow
- **Audio Quality**: Professional 128kbps MP3 with loudness matching

### 📈 Engagement Optimized
- **Instant Gratification**: Immediate upload confirmation
- **Visual Feedback**: Real-time reaction animations
- **Discovery Algorithm**: Trending and personalized feeds
- **Presence Awareness**: Know when friends are making music

### 🔒 Privacy & Security
- **Selective Sharing**: Only share what you choose
- **Content Control**: Delete anytime
- **Device Security**: Secure token management
- **Moderation Tools**: Community guidelines enforcement

### ⚡ Performance Requirements
- **< 100ms**: Audio preview start time
- **< 2%**: Crash rate tolerance
- **< 500ms**: Feed load time
- **99%**: Upload success rate

## Development Workflow

### Quick Start
```bash
# Install dependencies
make backend-deps

# Start development server
make backend-dev

# Run tests
make test-backend

# Build for production
make backend
```

### Testing
```bash
# Unit tests
go test ./internal/...

# Integration tests
go test ./tests/integration/...

# Load tests
go test ./tests/load/...

# Test with real VST
# 1. Start backend: make backend-dev
# 2. Load Sidechain.vst3 in Ableton
# 3. Test authentication flow
```

## Why Go?

**Perfect for Audio Social Network:**
- **Concurrency**: Handle thousands of simultaneous uploads
- **Performance**: Process audio without blocking other requests  
- **Memory Efficiency**: Critical when handling large audio buffers
- **Binary Deployment**: Single executable, no dependency hell
- **Network I/O**: Excellent for real-time WebSocket connections

**Stream.io + Go = ❤️**
- Native Go SDK for getstream.io Feeds V3
- Built-in JSON handling for API responses
- Excellent HTTP client for CDN uploads
- Goroutines perfect for background audio processing

## Production Deployment

The backend is designed for:
- **Railway/Fly.io**: Instant Go deployment
- **Cloudflare R2**: Global audio CDN
- **PostgreSQL**: Metadata persistence
- **Redis**: Session and cache management
- **Stream.io**: Social graph at scale

---

**This backend powers the dream: Every bedroom producer at 3am can instantly share their fire with the world, without leaving their creative flow. 🔥**