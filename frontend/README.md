# Sidechain Frontend 🌐

> Web interface for device claiming and community management

The Sidechain frontend is a lightweight web app that handles the one critical task the VST can't: **device claiming via OAuth**. It's the bridge between anonymous VST instances and persistent producer identities.

## Purpose

### **The Device Claiming Problem**
VST plugins are ephemeral - they get loaded and unloaded constantly. But social identity needs to be persistent. The frontend solves this by:

1. **VST generates device ID** → Shows in plugin UI
2. **Producer opens web interface** → Authenticates with Google/Discord  
3. **Web app claims the device** → Links VST to persistent account
4. **Backend returns JWT token** → VST becomes "Connected as: username"

### **Core Use Cases**

**Primary Flow (99% of usage):**
- Producer installs Sidechain VST
- VST shows device claiming URL
- Producer visits URL, signs in with Google/Discord
- Device is claimed, VST immediately shows connected status
- Producer never needs to open web interface again

**Secondary Flows:**
- **Profile Management**: Update username, bio, preferences
- **Community Discovery**: Browse trending producers (web-optimized view)
- **Account Settings**: Manage connected devices, privacy settings
- **Moderation Tools**: Report content, block users

## Design Philosophy

### **Mobile-First, Producer-Focused**
- **Lightning Fast**: Sub-1-second page loads
- **Mobile Optimized**: Most producers will claim on phones
- **Dark Theme**: Matches producer aesthetic
- **Audio-First**: Waveforms and play buttons everywhere
- **Zero Friction**: Minimal clicks to complete claiming

### **Not a Full Social Platform**
The frontend is **intentionally minimal**. The VST is where producers spend their time. The web interface is just for:
- Account setup and device claiming
- Profile management  
- Community discovery when away from DAW
- Administrative tasks

## Technology Stack

### **Framework: React + Next.js**
**Why React:**
- Fast development for interactive UI
- Great audio playback libraries
- Mobile-responsive components
- getstream.io has React SDK

**Why Next.js:**
- Server-side rendering for fast initial loads
- API routes for OAuth callbacks
- Built-in optimization
- Vercel deployment ready

### **Styling: Tailwind CSS**
- Rapid UI development
- Dark theme out of the box
- Mobile-first responsive design
- Small bundle size

### **Audio: Howler.js**
- Cross-browser audio playback
- Streaming support for loop previews
- Web Audio API integration
- Mobile compatibility

### **State Management: Zustand**
- Lightweight alternative to Redux
- Perfect for device claiming state
- Persistent storage support
- TypeScript friendly

## Project Structure

```
frontend/
├── src/
│   ├── components/
│   │   ├── AudioPlayer/        # Loop preview components
│   │   ├── UserProfile/        # Profile management
│   │   └── Layout/             # Shared layout components
│   ├── pages/
│   │   ├── claim/[deviceId]/   # Device claiming page
│   │   ├── profile/            # User profile pages
│   │   ├── discover/           # Community discovery
│   │   └── api/                # Next.js API routes
│   ├── utils/
│   │   ├── auth.ts             # OAuth helpers
│   │   ├── api.ts              # Backend API client
│   │   └── audio.ts            # Audio utilities
│   └── styles/
│       └── globals.css         # Tailwind + custom styles
├── public/
│   ├── icons/                  # Sidechain branding
│   └── audio/                  # Demo loops
└── package.json
```

## Key Features

### 🔐 **Device Claiming Flow**
**URL Pattern**: `https://sidechain.app/claim/{deviceId}`

**User Journey:**
1. Producer clicks "Open Browser" in VST
2. Web page opens with device info and "Sign in with Google/Discord"
3. OAuth flow completes, device is claimed
4. Success page shows "Device linked! Return to your DAW"
5. VST automatically detects authentication

### 📱 **Producer Profiles**
**URL Pattern**: `https://sidechain.app/@{username}`

**Features:**
- Public profile with recent loops
- Follower/following counts
- Bio and DAW preference
- Play buttons for all posts
- Follow/unfollow actions

### 🌍 **Community Discovery**
**URL Pattern**: `https://sidechain.app/discover`

**Features:**
- Trending loops (web-optimized player)
- Producer recommendations
- Genre-based discovery
- Search functionality

### ⚙️ **Account Management**
**URL Pattern**: `https://sidechain.app/settings`

**Features:**
- Connected devices list
- Privacy settings
- Account deletion
- Download user data

## Integration Points

### **Backend API Communication**
```typescript
// Device claiming
POST /api/v1/auth/claim
{
  "device_id": "uuid",
  "oauth_provider": "google",
  "oauth_code": "auth_code"
}

// Profile updates
PUT /api/v1/users/me
{
  "username": "newname",
  "bio": "Making beats since 2020"
}
```

### **Stream.io Integration**
```typescript
// Direct client-side connection for real-time features
const client = StreamChat.getInstance('api_key');
await client.connectUser(user, token);

// Subscribe to user notifications
const feed = client.feed('notification', userId);
feed.subscribe(data => {
  // Show notification for new followers, reactions
});
```

### **OAuth Providers**
```typescript
// Google OAuth configuration
const googleConfig = {
  clientId: process.env.GOOGLE_CLIENT_ID,
  redirectUri: `${baseUrl}/api/auth/google/callback`,
  scope: 'openid profile email'
};

// Discord OAuth configuration  
const discordConfig = {
  clientId: process.env.DISCORD_CLIENT_ID,
  redirectUri: `${baseUrl}/api/auth/discord/callback`,
  scope: 'identify email'
};
```

## Deployment Strategy

### **Vercel (Recommended)**
- **Next.js Optimized**: Zero-config deployment
- **Edge Functions**: Fast OAuth callbacks
- **Global CDN**: Sub-100ms page loads worldwide
- **Environment Variables**: Secure credential management

### **Custom Domain Setup**
- **Primary**: `sidechain.app`
- **Claiming**: `sidechain.app/claim/{deviceId}`
- **Profiles**: `sidechain.app/@{username}`
- **API Proxy**: Routes to Go backend

## Success Metrics

### **Device Claiming (Critical Path)**
- **Completion Rate**: > 95% of users who start claiming finish
- **Time to Complete**: < 30 seconds from VST click to connected
- **Mobile Success**: > 90% success rate on mobile devices
- **Error Rate**: < 1% OAuth failures

### **Community Engagement**
- **Profile Views**: Average 5+ profile visits per claimed device
- **Discovery Usage**: 30% of users browse trending loops
- **Return Visits**: 40% return to web interface after claiming

---

**The frontend's job: Get producers from anonymous VST to authenticated community member in under 30 seconds. Everything else is bonus.** 🚀