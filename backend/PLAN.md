# Sidechain Backend Implementation Plan

> Building the social engine for music producer stories

## Architecture Philosophy

**"Every bedroom producer at 3am should be able to share their fire with the world without leaving their creative flow"**

The backend is the social nervous system that connects isolated producers into a living, breathing community. It handles everything the VST can't: persistent state, heavy processing, real-time coordination, and social graph management.

## Core Responsibilities

### 1. **Unified Authentication System**
**Problem**: Producers need persistent identity, but shouldn't be forced to use third-party OAuth.

**Solution**: Email-based account unification with multiple login methods
- **Native accounts**: Email + password registration/login
- **OAuth options**: Google/Discord for convenience  
- **Account unification**: Same email = same account regardless of creation method
- **Seamless switching**: Users can add OAuth to native accounts or vice versa

### 2. **Audio Processing Pipeline** 
**Problem**: Raw DAW audio is huge, inconsistent, and not web-ready.

**Solution**: Server-side audio optimization
- Accept multipart uploads from VST (raw audio + metadata)
- Normalize loudness to -14 LUFS (consistent volume across all posts)
- Encode to 128kbps MP3 (quality vs. bandwidth balance)
- Generate SVG waveforms for visual previews
- Upload to CDN with global distribution

### 3. **Social Graph Engine (Stream.io Integration)**
**Problem**: Building social features from scratch is complex and doesn't scale.

**Solution**: getstream.io Feeds V3 as the social backbone
- Activity feeds for posting and discovery
- Follow/unfollow relationships
- Emoji reactions with custom data (🎵❤️🔥😍🚀💯)
- Real-time updates and notifications
- Proven scalability (powers 100M+ user apps)

### 4. **Real-time Features (Stream.io Handles This)**
**Problem**: Producers want live updates and presence awareness.

**Solution**: Use getstream.io's built-in real-time capabilities
- getstream.io Chat provides presence (online/offline/last_active)
- getstream.io Feeds provide real-time activity updates  
- getstream.io handles WebSocket connections and scaling
- No custom real-time server needed

## Implementation Phases

### Phase 1: Unified Authentication System ⏳
**Current Status**: Basic API structure in place

**What We Actually Need:**
```go
// User model with email-based unification
type User struct {
    ID          string    `db:"id" json:"id"`
    Email       string    `db:"email" json:"email" unique:"true"`
    Username    string    `db:"username" json:"username"`
    PasswordHash *string  `db:"password_hash" json:"-"` // Nullable for OAuth-only users
    GoogleID     *string  `db:"google_id" json:"-"`
    DiscordID    *string  `db:"discord_id" json:"-"`
    CreatedAt   time.Time `db:"created_at" json:"created_at"`
}

// Unified auth service
type AuthService struct {
    DB            *sql.DB
    GoogleConfig  *oauth2.Config
    DiscordConfig *oauth2.Config
    JWTSecret     []byte
}

// Account unification logic
func (s *AuthService) FindOrCreateUserByEmail(email, name string, oauthProvider *string, oauthID *string) (*User, error)
```

**Authentication Endpoints:**
```bash
# Native account creation
POST /api/v1/auth/register
{
  "email": "producer@example.com",
  "username": "beatmaker123", 
  "password": "securepassword"
}

# Native login
POST /api/v1/auth/login
{
  "email": "producer@example.com",
  "password": "securepassword"
}

# OAuth login (Google/Discord)
GET  /api/v1/auth/google     # Redirects to Google OAuth
GET  /api/v1/auth/discord    # Redirects to Discord OAuth
POST /api/v1/auth/oauth/callback # Handles OAuth callbacks

# Account management
POST /api/v1/auth/forgot-password
POST /api/v1/auth/reset-password
POST /api/v1/auth/change-password
GET  /api/v1/auth/me         # Get current user info
POST /api/v1/auth/link-oauth  # Add OAuth to existing account
```

**Account Unification Logic:**
```go
func (s *AuthService) HandleOAuthCallback(provider, code string) (*AuthResponse, error) {
    // 1. Get user info from OAuth provider
    userInfo, err := s.getOAuthUserInfo(provider, code)
    if err != nil {
        return nil, err
    }
    
    // 2. Check if user exists by email
    existingUser, err := s.FindUserByEmail(userInfo.Email)
    if err == nil {
        // User exists - link OAuth to existing account
        err = s.linkOAuthProvider(existingUser.ID, provider, userInfo.OAuthID)
        if err != nil {
            return nil, err
        }
        return s.generateAuthResponse(existingUser), nil
    }
    
    // 3. Create new user with OAuth info
    newUser := &User{
        ID:       generateUUID(),
        Email:    userInfo.Email,
        Username: generateUsername(userInfo.Name),
    }
    
    if provider == "google" {
        newUser.GoogleID = &userInfo.OAuthID
    } else if provider == "discord" {
        newUser.DiscordID = &userInfo.OAuthID
    }
    
    err = s.CreateUser(newUser)
    if err != nil {
        return nil, err
    }
    
    return s.generateAuthResponse(newUser), nil
}

func (s *AuthService) RegisterNativeUser(email, username, password string) (*AuthResponse, error) {
    // 1. Check if user exists by email
    existingUser, err := s.FindUserByEmail(email)
    if err == nil {
        if existingUser.PasswordHash == nil {
            // OAuth-only user trying to add password - allow it
            hashedPassword, _ := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
            existingUser.PasswordHash = &hashedPassword
            err = s.UpdateUser(existingUser)
            return s.generateAuthResponse(existingUser), err
        }
        return nil, errors.New("user already exists with this email")
    }
    
    // 2. Create new native user
    hashedPassword, _ := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
    newUser := &User{
        ID:           generateUUID(),
        Email:        email,
        Username:     username,
        PasswordHash: &hashedPassword,
    }
    
    err = s.CreateUser(newUser)
    return s.generateAuthResponse(newUser), err
}
```

**Implementation Order:**
1. Add database schema for unified user model
2. Implement native registration/login endpoints
3. Add OAuth endpoints with email unification
4. Add password reset flow
5. Add account linking endpoints
6. Test all auth flows work seamlessly

### Phase 1B: Audio Upload Foundation ⏳
**Current Status**: Placeholder upload endpoint exists

**Missing Components:**
```go
// Audio processing service
type AudioProcessor struct {
    FFmpegPath   string
    LamePath     string
    TempDir      string
    CDNUploader  CDNUploader
}

// Storage abstraction
type CDNUploader interface {
    Upload(ctx context.Context, data []byte, filename string) (string, error)
    Delete(ctx context.Context, url string) error
}
```

**Implementation Order:**
1. Add multipart file upload handling
2. Integrate FFmpeg for loudness normalization
3. Add LAME for MP3 encoding
4. Implement SVG waveform generation
5. Create CDN upload interface (R2/S3)
6. Store metadata in database

### Phase 2: getstream.io Integration (Real API) 🔄
**Current Status**: V3 SDK added but methods are stubbed

**Missing Components:**
```go
// Proper getstream.io V3 implementation
func (c *Client) CreateLoopActivity(userID string, activity *Activity) error {
    ctx := context.Background()
    
    // Real V3 API call
    feedsClient := c.FeedsClient.Feeds()
    activityReq := &getstream.AddActivityRequest{
        Text: getstream.PtrTo(activity.Object),
        Type: getstream.PtrTo("loop_post"),
        Custom: map[string]interface{}{
            "audio_url":     activity.AudioURL,
            "bpm":          activity.BPM,
            "key":          activity.Key,
            // ... full metadata
        },
    }
    
    return feedsClient.AddActivity(ctx, activityReq)
}
```

**Implementation Order:**
1. Research V3 API docs and working examples
2. Implement real feed creation and activity posting
3. Add proper follow/unfollow with V3 API
4. Implement emoji reactions with custom data
5. Add feed pagination and caching
6. Test with real getstream.io dashboard

### Phase 3: Database Layer 📊
**Current Status**: No persistence layer

**Missing Components:**
```go
// Database models
type User struct {
    ID            string    `db:"id" json:"id"`
    StreamUserID  string    `db:"stream_user_id" json:"stream_user_id"`
    Username      string    `db:"username" json:"username"`
    Email         string    `db:"email" json:"email"`
    FollowerCount int       `db:"follower_count" json:"follower_count"`
    CreatedAt     time.Time `db:"created_at" json:"created_at"`
}

type AudioPost struct {
    ID           string    `db:"id" json:"id"`
    UserID       string    `db:"user_id" json:"user_id"`
    AudioURL     string    `db:"audio_url" json:"audio_url"`
    WaveformSVG  string    `db:"waveform_svg" json:"waveform_svg"`
    BPM          int       `db:"bpm" json:"bpm"`
    Key          string    `db:"key" json:"key"`
    PlayCount    int       `db:"play_count" json:"play_count"`
    CreatedAt    time.Time `db:"created_at" json:"created_at"`
}
```

**Implementation Order:**
1. Add PostgreSQL and migration dependencies
2. Design database schema for users, posts, devices
3. Create migration system
4. Add database connection and health checks
5. Implement repository pattern for data access
6. Add indexes for feed queries

### Phase 4: Real-time Features (Stream.io Only) ✅
**Current Status**: getstream.io handles all real-time needs

**What getstream.io Provides:**
- Real-time feed updates (new posts appear instantly)
- Presence tracking (who's online/offline)
- Real-time reactions and notifications
- WebSocket connection management
- Automatic reconnection and scaling

**No Custom Implementation Needed:**
- VST connects directly to getstream.io WebSocket
- Frontend uses getstream.io React SDK
- Backend just provides authentication tokens
- getstream.io handles all the complexity

## Critical Backend Decisions

### 1. **Database Strategy**
- **PostgreSQL**: Primary database for user/post metadata
- **Redis**: Session storage and caching layer
- **Stream.io**: Social graph and feed data (don't duplicate)

### 2. **Audio Storage Strategy**
- **Cloudflare R2**: Primary CDN for global distribution
- **Local Processing**: Server-side audio optimization
- **Aggressive Caching**: 10-minute CDN cache headers
- **Background Processing**: Queue system for heavy operations

### 3. **Stream.io Integration Pattern**
- **Single Source of Truth**: getstream.io owns social relationships
- **Metadata Enrichment**: Backend adds audio-specific data
- **Hybrid Approach**: Database for search, getstream.io for feeds
- **Real-time Sync**: Keep local cache fresh via webhooks

### 4. **Security Model**
- **Device-Based Auth**: Unique device IDs, not passwords
- **JWT with Refresh**: Short-lived tokens, automatic refresh
- **OAuth Only**: No custom user/password system
- **Stream.io Delegation**: Let getstream.io handle permissions

## Backend Service Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    HTTP API Layer                       │
│  /auth /audio /feed /social /users /ws                  │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────┴───────────────────────────────────┐
│                 Service Layer                           │
│  AuthService │ AudioService │ SocialService             │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────┴───────────────────────────────────┐
│                Integration Layer                        │
│  getstream.io Client │ Database │ CDN │ OAuth Providers    │
└─────────────────────────────────────────────────────────┘
```

## Simplified Backend - What Do We ACTUALLY Need?

### **Essential Components (Can't Ship Without):**
1. **Unified auth system** - Native accounts + OAuth with email unification
2. **Audio upload** - Accept MP3, normalize, upload to CDN  
3. **Stream.io proxy** - Create users, post activities
4. **JWT tokens** - Authenticate VST requests
5. **User database** - PostgreSQL for account management

### **What getstream.io Still Handles:**
✅ **Social feeds** - Global, timeline, user feeds
✅ **Real-time updates** - New posts, reactions appear instantly
✅ **Presence system** - Online/offline status built-in
✅ **Emoji reactions** - Native support with custom data
✅ **WebSocket management** - Connection handling, scaling

### **Account Unification Examples:**

**Scenario 1: OAuth First, Then Native**
```
1. User logs in with Google → Creates account with email "user@gmail.com"
2. Later tries to register with email "user@gmail.com" + password
3. Backend detects existing account, adds password to OAuth account
4. User can now login with either Google OR email+password
```

**Scenario 2: Native First, Then OAuth**  
```
1. User registers with email "user@gmail.com" + password
2. Later tries to login with Google (same email)
3. Backend detects existing account, links Google ID to native account
4. User can now login with either email+password OR Google
```

**MVP Definition (Week 1):**
- VST shows login options: "Email/Password" OR "Google" OR "Discord"
- All methods create/link to same account if emails match
- VST uploads loops → Backend processes → Posts to getstream.io
- **Total backend code: ~800 lines Go** (auth is more complex now)

### **Week 2: Audio Pipeline** 🎵
Focus: VST can upload audio and see it processed

**MVP Definition**:
- VST uploads 8-bar MP3 to backend
- Backend normalizes and stores on CDN
- Backend creates getstream.io activity with audio URL
- VST shows upload success

### **Week 3: Social Feed** 📱
Focus: VST can view feed of other producer's loops

**MVP Definition**:
- Backend serves paginated global feed
- VST displays scrollable list of posts
- Audio preview playback works
- Basic emoji reactions function

### **Week 4: Real-time Features** ⚡
Focus: Live producer presence and notifications

**MVP Definition**:
- WebSocket connection between VST and backend
- "🟢 Live in Studio" status tracking
- Real-time feed updates
- Presence notifications

## Key Go Packages Needed

```go
// Core web framework
"github.com/gin-gonic/gin"
"github.com/gin-contrib/cors"

// OAuth and security
"golang.org/x/oauth2"
"golang.org/x/oauth2/google"
"github.com/golang-jwt/jwt/v5"
"github.com/gorilla/sessions"

// Database and auth
"github.com/lib/pq"                    // PostgreSQL driver
"github.com/golang-migrate/migrate/v4" // Migrations
"github.com/jmoiron/sqlx"             // SQL toolkit
"golang.org/x/crypto/bcrypt"          // Password hashing
"github.com/golang-jwt/jwt/v5"        // JWT tokens

// getstream.io
"github.com/GetStream/getstream-go/v3" // Feeds V3
"github.com/GetStream/stream-chat-go/v5" // Chat

// Audio processing
"github.com/go-audio/wav"     // WAV parsing
"github.com/hajimehoshi/go-mp3" // MP3 encoding helper

// CDN and storage
"github.com/aws/aws-sdk-go-v2" // S3/R2 upload
"github.com/minio/minio-go/v7" // Alternative S3 client

// Real-time
"github.com/gorilla/websocket" // WebSocket server
"github.com/redis/go-redis/v9" // Redis for caching

// Utilities
"github.com/google/uuid"       // ID generation
"github.com/joho/godotenv"     // Environment variables
```

## Success Metrics

### **Technical Performance**
- **Upload Speed**: < 5 seconds for 8-bar loop
- **Feed Load**: < 500ms for 20 posts
- **Audio Preview**: < 100ms start time
- **Uptime**: 99.9% availability
- **Memory Usage**: < 100MB per concurrent user

### **Developer Experience**
- **Build Time**: < 10 seconds for backend changes
- **Hot Reload**: Live development without restarts
- **Test Coverage**: > 80% for critical paths
- **Documentation**: Every endpoint documented with examples

### **User Experience Goals**
- **Authentication**: One-click device claiming
- **Sharing**: Instant feedback on post success
- **Discovery**: Addictive feed scrolling
- **Presence**: Always know who's creating

## Risk Mitigation

### **Stream.io Dependency**
- **Risk**: Service downtime or API changes
- **Mitigation**: Local caching, graceful degradation, abstraction layer

### **Audio Processing Load**
- **Risk**: CPU spikes during upload processing
- **Mitigation**: Background job queue, rate limiting, horizontal scaling

### **WebSocket Complexity**
- **Risk**: Connection management issues at scale
- **Mitigation**: Battle-tested Gorilla WebSocket, connection pooling

### **Database Performance**
- **Risk**: Slow queries as userbase grows
- **Mitigation**: Proper indexes, query optimization, caching strategy

---

**The backend's job is simple: Make sharing music as addictive as sharing photos, but built specifically for the producer workflow. Every design decision should optimize for the 3am bedroom producer getting instant dopamine from their community.** 🎵🔥