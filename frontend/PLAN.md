# Sidechain Frontend Implementation Plan

> Modern React app with Vite, pnpm, Turbo, and Stream.io V3

## Tech Stack Rationale

### **Vite + React + TypeScript**
**Why Vite:**
- ⚡ **Lightning fast**: Sub-second hot reload for audio UI development
- 📦 **Modern bundling**: Native ESM, tree-shaking, code splitting
- 🔧 **Zero config**: Works out of the box with React + TS
- 🎵 **Audio optimized**: Great for Web Audio API integration

**Why React 18:**
- 🔄 **Concurrent features**: Perfect for real-time audio streams
- 🎨 **Component ecosystem**: Rich audio/music UI libraries
- 📱 **Mobile performance**: Excellent mobile audio support
- 🌊 **Stream.io integration**: First-class React SDK

### **pnpm + corepack + Turbo**
**Why pnpm:**
- 💾 **Disk efficiency**: Saves ~3GB vs npm/yarn (audio libs are heavy)
- ⚡ **Speed**: 2x faster installs than npm
- 🔒 **Security**: Strict dependency resolution
- 📦 **Workspace support**: Multi-package monorepo ready

**Why corepack:**
- 🎯 **Version consistency**: Locks package manager version
- 🤝 **Team alignment**: Everyone uses same pnpm version
- 🔄 **CI/CD friendly**: Deterministic builds

**Why Turbo:**
- 🚀 **Build caching**: Skip rebuilds when nothing changed
- ⚖️ **Parallel builds**: Build frontend + backend simultaneously
- 📊 **Analytics**: Build performance insights
- 🔧 **Task orchestration**: Complex workflow management

### **Stream.io V3 Browser SDK**
**Why Stream.io V3:**
- 🌊 **Real-time feeds**: Live updates without polling
- 😀 **Emoji reactions**: Built-in support for 🎵❤️🔥😍🚀💯
- 📱 **Mobile optimized**: Touch-friendly interactions
- 🔄 **State management**: Automatic state sync across tabs
- 🎨 **UI components**: Pre-built feed components

## Project Architecture

```
frontend/
├── apps/
│   └── web/                    # Main React app
│       ├── src/
│       │   ├── components/
│       │   ├── pages/
│       │   ├── utils/
│       │   └── main.tsx
│       ├── public/
│       ├── vite.config.ts
│       └── package.json
├── packages/
│   ├── ui/                     # Shared UI components
│   ├── stream-client/          # Stream.io V3 wrapper
│   └── types/                  # Shared TypeScript types
├── turbo.json                  # Turborepo configuration
├── pnpm-workspace.yaml         # pnpm workspace config
└── package.json                # Root package.json with corepack
```

## Implementation Phases

### Phase 1: Modern Tooling Setup 🔧
**Week 1 Day 1-2**

**Goals:** 
- Lightning-fast development environment
- Stream.io V3 integration working
- Device claiming page functional

**Implementation:**
```bash
# Initialize modern stack
echo 'packageManager="pnpm@9.0.0"' >> package.json  # corepack
pnpm create turbo@latest sidechain-frontend --package-manager pnpm
cd sidechain-frontend && pnpm install

# Add Stream.io V3 browser SDK
pnpm add @stream-io/feeds-client @stream-io/feeds-react-sdk

# Setup Vite with audio optimizations
# vite.config.ts - optimize for audio file handling
```

**File Structure:**
```typescript
// packages/stream-client/index.ts
export class SidechainStreamClient {
  private client: FeedsClient;
  
  async connectUser(userId: string, token: string) {
    await this.client.connectUser({ id: userId }, token);
  }
  
  async getGlobalFeed() {
    const feed = this.client.feed("global", "main");
    return await feed.getOrCreate({ watch: true });
  }
  
  async reactWithEmoji(activityId: string, emoji: string) {
    return await this.client.addReaction({
      activity_id: activityId,
      type: "emoji",
      emoji: emoji
    });
  }
}
```

### Phase 2: Device Claiming Flow 🔐
**Week 1 Day 3-4**

**Goals:**
- Complete OAuth integration (Google + Discord)
- Seamless device claiming experience
- Mobile-optimized UI

**Key Components:**
```typescript
// pages/claim/[deviceId].tsx
export default function ClaimDevice({ deviceId }: { deviceId: string }) {
  const [claimStatus, setClaimStatus] = useState<'pending' | 'claiming' | 'success' | 'error'>('pending');
  
  const handleGoogleAuth = async () => {
    // OAuth flow with backend integration
    const authUrl = `/api/auth/google?device_id=${deviceId}`;
    window.location.href = authUrl;
  };
  
  return (
    <div className="min-h-screen bg-gray-900 flex items-center justify-center">
      <ClaimingCard deviceId={deviceId} onAuth={handleGoogleAuth} />
    </div>
  );
}

// components/DeviceClaiming/ClaimingCard.tsx
export function ClaimingCard({ deviceId, onAuth }: ClaimingProps) {
  return (
    <div className="bg-gray-800 p-8 rounded-lg max-w-md">
      <h1 className="text-2xl font-bold text-white mb-4">
        🎵 Claim Your Sidechain Device
      </h1>
      <p className="text-gray-300 mb-6">
        Connect this VST instance to your producer account
      </p>
      <div className="space-y-4">
        <OAuthButton provider="google" onClick={() => onAuth('google')} />
        <OAuthButton provider="discord" onClick={() => onAuth('discord')} />
      </div>
    </div>
  );
}
```

### Phase 3: Audio-First UI Components 🎵
**Week 1 Day 5-7**

**Goals:**
- Rich audio playback in web interface
- Waveform visualizations
- Emoji reaction system

**Key Components:**
```typescript
// components/AudioPlayer/LoopPlayer.tsx
export function LoopPlayer({ audioUrl, waveformSvg }: LoopPlayerProps) {
  const [isPlaying, setIsPlaying] = useState(false);
  const [howl, setHowl] = useState<Howl | null>(null);
  
  useEffect(() => {
    const audio = new Howl({
      src: [audioUrl],
      format: ['mp3'],
      onplay: () => setIsPlaying(true),
      onend: () => setIsPlaying(false)
    });
    setHowl(audio);
    
    return () => audio.unload();
  }, [audioUrl]);
  
  return (
    <div className="flex items-center space-x-4">
      <PlayButton isPlaying={isPlaying} onClick={() => howl?.play()} />
      <WaveformVisualization svg={waveformSvg} />
      <EmojiReactionBar activityId={activityId} />
    </div>
  );
}

// components/AudioPlayer/EmojiReactionBar.tsx
export function EmojiReactionBar({ activityId }: EmojiReactionProps) {
  const streamClient = useStreamClient();
  
  const reactions = ['🎵', '❤️', '🔥', '😍', '🚀', '💯'];
  
  const handleReact = async (emoji: string) => {
    await streamClient.reactWithEmoji(activityId, emoji);
    // Optimistic UI update
  };
  
  return (
    <div className="flex space-x-2">
      {reactions.map(emoji => (
        <EmojiButton key={emoji} emoji={emoji} onClick={handleReact} />
      ))}
    </div>
  );
}
```

### Phase 4: Community Features 👥
**Week 2**

**Goals:**
- Producer discovery and profiles
- Social graph visualization
- Community moderation tools

**Key Pages:**
```typescript
// pages/discover.tsx - Community discovery
export default function Discover() {
  const { feed, loading } = useGlobalFeed();
  
  return (
    <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
      {feed.map(post => (
        <LoopCard key={post.id} post={post} />
      ))}
    </div>
  );
}

// pages/profile/[username].tsx - Producer profiles
export default function ProducerProfile({ username }: ProfileProps) {
  const { producer, posts, loading } = useProducer(username);
  
  return (
    <div className="max-w-4xl mx-auto">
      <ProducerHeader producer={producer} />
      <PostGrid posts={posts} />
    </div>
  );
}
```

## Stream.io V3 Integration Strategy

### **Client-Side Real-time**
```typescript
// utils/stream.ts
import { FeedsClient } from '@stream-io/feeds-react-sdk';

export const initializeStreamClient = (apiKey: string) => {
  const client = new FeedsClient(apiKey);
  
  // Connect user after device claiming
  const connectUser = async (userId: string, token: string) => {
    await client.connectUser({ id: userId }, token);
    
    // Subscribe to real-time events
    const notifications = client.feed("notification", userId);
    await notifications.getOrCreate({ watch: true });
    
    notifications.state.subscribe((state) => {
      // Handle new reactions, follows, etc.
      showNotification(state.activities);
    });
  };
  
  return { client, connectUser };
};
```

### **React Hooks for Stream.io**
```typescript
// hooks/useStreamFeed.ts
export function useGlobalFeed() {
  const [feed, setFeed] = useState<Activity[]>([]);
  const [loading, setLoading] = useState(true);
  const streamClient = useStreamClient();
  
  useEffect(() => {
    const globalFeed = streamClient.feed("global", "main");
    
    globalFeed.getOrCreate({ watch: true }).then(() => {
      setFeed(globalFeed.state.getLatestValue().activities);
      setLoading(false);
      
      // Subscribe to real-time updates
      globalFeed.state.subscribe((state) => {
        setFeed(state.activities);
      });
    });
  }, [streamClient]);
  
  return { feed, loading };
}
```

## Development Workflow

### **Package Scripts (Turbo-powered)**
```json
{
  "scripts": {
    "dev": "turbo run dev",
    "build": "turbo run build", 
    "test": "turbo run test",
    "lint": "turbo run lint",
    "dev:web": "turbo run dev --filter=web",
    "build:packages": "turbo run build --filter=./packages/*"
  }
}
```

### **Turbo Configuration**
```json
// turbo.json
{
  "tasks": {
    "build": {
      "dependsOn": ["^build"],
      "outputs": ["dist/**", ".next/**"]
    },
    "dev": {
      "cache": false,
      "persistent": true
    },
    "test": {
      "dependsOn": ["^build"]
    }
  }
}
```

### **Development Commands**
```bash
# Start development (frontend + backend simultaneously)
turbo run dev

# Install dependencies
pnpm install

# Add Stream.io V3 dependencies
pnpm add @stream-io/feeds-client @stream-io/feeds-react-sdk

# Build for production
turbo run build

# Test across packages
turbo run test
```

## Integration with Backend

### **API Client (TypeScript)**
```typescript
// utils/api.ts
export class SidechainAPI {
  constructor(private baseUrl: string) {}
  
  async claimDevice(deviceId: string, oauthProvider: string, code: string) {
    return this.post('/api/v1/auth/claim', {
      device_id: deviceId,
      oauth_provider: oauthProvider,
      oauth_code: code
    });
  }
  
  async getGlobalFeed(limit = 20, offset = 0) {
    return this.get(`/api/v1/feed/global?limit=${limit}&offset=${offset}`);
  }
  
  async reactToPost(activityId: string, emoji: string) {
    return this.post('/api/v1/social/react', {
      activity_id: activityId,
      emoji: emoji
    });
  }
}
```

### **Stream.io V3 Browser Integration**
```typescript
// components/StreamProvider.tsx
import { FeedsClient } from '@stream-io/feeds-client';

const StreamContext = createContext<FeedsClient | null>(null);

export function StreamProvider({ children, apiKey }: StreamProviderProps) {
  const [client, setClient] = useState<FeedsClient | null>(null);
  
  useEffect(() => {
    const streamClient = new FeedsClient(apiKey);
    setClient(streamClient);
    
    return () => streamClient.disconnect();
  }, [apiKey]);
  
  return (
    <StreamContext.Provider value={client}>
      {children}
    </StreamContext.Provider>
  );
}

export const useStreamClient = () => {
  const client = useContext(StreamContext);
  if (!client) throw new Error('StreamProvider not found');
  return client;
};
```

## Mobile-First Responsive Design

### **Tailwind Configuration**
```javascript
// tailwind.config.js
module.exports = {
  content: ['./src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      colors: {
        sidechain: {
          bg: '#18181c',
          surface: '#202024', 
          primary: '#00d4ff',
          accent: '#ff6b35'
        }
      },
      animation: {
        'waveform': 'waveform 2s ease-in-out infinite',
        'emoji-react': 'bounce 0.3s ease-in-out'
      }
    }
  },
  plugins: [
    require('@tailwindcss/forms'),
    require('@tailwindcss/typography')
  ]
}
```

### **Audio-Optimized Components**
```typescript
// components/ui/PlayButton.tsx
export function PlayButton({ isPlaying, onClick, size = 'md' }: PlayButtonProps) {
  const sizeClasses = {
    sm: 'w-8 h-8',
    md: 'w-12 h-12', 
    lg: 'w-16 h-16'
  };
  
  return (
    <button 
      onClick={onClick}
      className={`
        ${sizeClasses[size]} 
        bg-sidechain-primary hover:bg-blue-400 
        rounded-full flex items-center justify-center 
        transition-all duration-200 transform hover:scale-105
        active:scale-95
      `}
    >
      {isPlaying ? '⏸️' : '▶️'}
    </button>
  );
}

// components/ui/WaveformDisplay.tsx  
export function WaveformDisplay({ svgData, isPlaying }: WaveformProps) {
  return (
    <div className={`
      relative overflow-hidden rounded-lg bg-gray-800
      ${isPlaying ? 'animate-pulse' : ''}
    `}>
      <div 
        dangerouslySetInnerHTML={{ __html: svgData }}
        className="w-full h-20"
      />
      {isPlaying && (
        <div className="absolute inset-0 bg-sidechain-primary opacity-20 animate-pulse" />
      )}
    </div>
  );
}
```

## Key Frontend Routes

### **Device Claiming Flow**
```typescript
// pages/claim/[deviceId].tsx
export default function ClaimDevice() {
  const { deviceId } = useRouter().query;
  const [user, setUser] = useState(null);
  const streamClient = useStreamClient();
  
  const handleOAuthSuccess = async (token: string, userInfo: any) => {
    // 1. Claim device with backend
    await api.claimDevice(deviceId, 'google', authCode);
    
    // 2. Connect to Stream.io with user token
    await streamClient.connectUser({ 
      id: userInfo.id,
      name: userInfo.name 
    }, token);
    
    // 3. Show success and redirect
    setUser(userInfo);
    showSuccessMessage();
  };
  
  return (
    <ClaimingFlow 
      deviceId={deviceId} 
      onSuccess={handleOAuthSuccess}
    />
  );
}
```

### **Community Discovery**
```typescript
// pages/discover.tsx
export default function Discover() {
  const { feed, loading } = useGlobalFeed();
  const [filter, setFilter] = useState({ genre: 'all', bpm: 'all' });
  
  return (
    <div className="container mx-auto px-4 py-8">
      <DiscoverHeader filters={filter} onFilterChange={setFilter} />
      <LoopGrid 
        loops={feed}
        loading={loading}
        onLoadMore={() => loadMorePosts()}
      />
    </div>
  );
}
```

### **Producer Profiles**
```typescript
// pages/@[username].tsx (Next.js dynamic route)
export default function ProducerProfile() {
  const { username } = useRouter().query;
  const { producer, posts } = useProducer(username);
  const streamClient = useStreamClient();
  
  const handleFollow = async () => {
    await streamClient.followUser(producer.id);
    // Optimistic UI update
  };
  
  return (
    <div className="max-w-4xl mx-auto">
      <ProducerHeader producer={producer} onFollow={handleFollow} />
      <PostGrid posts={posts} />
    </div>
  );
}
```

## Audio Playback Strategy

### **Howler.js Integration**
```typescript
// utils/audioPlayer.ts
class AudioPlayerManager {
  private currentPlayer: Howl | null = null;
  
  async playLoop(audioUrl: string) {
    // Stop current playback
    this.currentPlayer?.stop();
    
    // Create new player
    this.currentPlayer = new Howl({
      src: [audioUrl],
      format: ['mp3'],
      volume: 0.7,
      onplay: () => this.onPlayStart(),
      onend: () => this.onPlayEnd(),
      onloaderror: () => this.onError()
    });
    
    this.currentPlayer.play();
  }
  
  // Global audio control (only one loop plays at a time)
  stopAll() {
    this.currentPlayer?.stop();
  }
}

export const audioPlayer = new AudioPlayerManager();
```

### **Stream.io V3 Real-time Integration**
```typescript
// hooks/useRealtimeUpdates.ts
export function useRealtimeUpdates() {
  const streamClient = useStreamClient();
  const [notifications, setNotifications] = useState([]);
  
  useEffect(() => {
    if (!streamClient) return;
    
    // Subscribe to real-time feed updates
    const globalFeed = streamClient.feed("global", "main");
    globalFeed.getOrCreate({ watch: true });
    
    globalFeed.state.subscribe((state) => {
      // New posts appear in real-time
      showToast("🎵 New loop from " + state.activities[0]?.actor);
    });
    
    // Subscribe to personal notifications
    const notificationFeed = streamClient.feed("notification", userId);
    notificationFeed.state.subscribe((state) => {
      setNotifications(state.activities);
    });
  }, [streamClient]);
  
  return { notifications };
}
```

## Performance Optimizations

### **Vite Configuration**
```typescript
// vite.config.ts
export default defineConfig({
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          'stream-io': ['@stream-io/feeds-client'],
          'audio': ['howler'],
          'vendor': ['react', 'react-dom']
        }
      }
    }
  },
  server: {
    proxy: {
      '/api': 'http://localhost:8787' // Proxy to Go backend
    }
  }
});
```

### **Code Splitting Strategy**
```typescript
// Lazy load heavy components
const ProducerProfile = lazy(() => import('./pages/ProducerProfile'));
const AudioStudio = lazy(() => import('./pages/AudioStudio'));

// Preload critical paths
export function App() {
  useEffect(() => {
    // Preload device claiming assets
    import('./components/DeviceClaiming');
  }, []);
  
  return (
    <Suspense fallback={<LoadingSpinner />}>
      <Routes>
        <Route path="/claim/:deviceId" element={<ClaimDevice />} />
        <Route path="/@:username" element={<ProducerProfile />} />
        <Route path="/discover" element={<Discover />} />
      </Routes>
    </Suspense>
  );
}
```

## Success Metrics

### **Device Claiming (Critical Path)**
- **Load Time**: < 1 second on mobile
- **Completion Rate**: > 95% success rate
- **Mobile Experience**: Touch-optimized for phones
- **Error Handling**: Clear error messages for OAuth failures

### **Audio Experience**
- **Preview Start**: < 200ms audio start time
- **Streaming**: Progressive loading for large files
- **Mobile Audio**: Works reliably on iOS Safari
- **Emoji Reactions**: < 100ms response time

### **Real-time Features**
- **WebSocket Reliability**: < 1% connection drops
- **Live Updates**: New posts appear within 2 seconds
- **Presence Accuracy**: Producer status updates within 5 seconds

---

**The frontend's mission: Turn device claiming from a chore into an exciting moment where producers join the community. Fast, beautiful, and mobile-first.** 📱🎵