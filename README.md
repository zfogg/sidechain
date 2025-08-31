# Sidechain 🎵

> Social "Stories" for Music Producers - Share loops directly from your DAW

Sidechain is a VST3/AU plugin that brings social media into your music production workflow. Share 8-bar loops, discover what other producers are making, and build a community without ever leaving Ableton Live.

## Why Sidechain?

**The Problem**: Producers are isolated in their DAWs, spending hours perfecting loops that no one ever hears. Existing platforms like SoundCloud are antisocial, Instagram doesn't understand music, and TikTok is for finished tracks, not the creative process.

**The Solution**: A social feed that lives inside your DAW. One click to share what you're working on, infinite scroll to discover fresh inspiration, all without disrupting your creative flow.

## Features

### 🎛️ Native DAW Integration
- Works inside Ableton Live, FL Studio, Logic Pro, and more
- Capture audio from any track or the master bus
- One-click sharing of highlighted regions
- Audio previews play through your existing monitors

### 📱 Social Discovery
- Instagram-style feed of loops from producers worldwide
- Follow your favorite artists and see their latest sketches
- Like, share, and discover trending sounds
- Real-time "🟢 Live in Studio" presence indicators

### ⚡ Producer-First Design
- No export/upload friction - share directly from your session  
- Automatic tempo and key detection
- Genre tagging and smart recommendations
- "Drop to Track" button adds discovered loops to your project

### 🎹 MIDI Magic
- **Pattern Sharing**: Upload MIDI alongside audio for remixing
- **Visual Piano Rolls**: Animated piano rolls that sync with audio playback
- **Hidden Synth Easter Egg**: Secret synthesizer unlocked by chord sequences
- **MIDI Battle Royale**: Weekly producer challenges with creative constraints

### 🔴 Live Features (Coming Soon)
- See who's producing in real-time
- Collaborative playlists and remix chains
- Cross-DAW MIDI collaboration
- Project file exchange

## Quick Start

### For Producers
1. Download the VST plugin
2. Load in your DAW and authenticate
3. Highlight an 8-bar section
4. Click "Share Loop" 
5. Start discovering and following other producers

### For Developers
```bash
# Clone the repository
git clone https://github.com/zfogg/sidechain.git
cd sidechain

# Set up the backend
cd backend
go mod download
cp .env.example .env
# Add your Stream.io credentials to .env
go run cmd/server/main.go

# Set up the VST plugin
brew install juce
cd ../plugin
make
make install
```

See [CLAUDE.md](./CLAUDE.md) for detailed development instructions.

## Architecture

### VST Plugin (C++ with JUCE)
- Cross-platform audio plugin (VST3, AU, AAX)
- Captures audio selections from the DAW
- Embedded social feed UI with audio preview
- WebSocket connection for real-time updates

### Backend (Go + Stream.io)
- RESTful API for authentication and audio upload
- Stream.io integration for social feeds and engagement
- Audio processing pipeline (normalization, compression)
- WebSocket server for live presence features

### Infrastructure
- **Audio Storage**: Cloudflare R2 / AWS S3
- **CDN**: Cloudflare for global distribution
- **Social Engine**: Stream.io Activity Feeds
- **Database**: PostgreSQL for metadata
- **Deployment**: Docker + Railway/Vercel

## Development Roadmap

### Phase 1: Foundation ✅ (In Progress)
- Basic VST shell that loads in DAWs
- Authentication flow with device claiming
- Simple audio capture and upload
- Basic feed display with playback

### Phase 2: Audio Pipeline
- Master bus audio capture
- MP3 encoding and waveform generation  
- CDN upload with progress tracking
- Loudness normalization (-14 LUFS)

### Phase 3: Social Features
- Stream.io feed integration
- Like/follow system
- Real-time updates via WebSockets
- Basic producer profiles

### Phase 4: Discovery & Polish
- Genre tagging and recommendations
- Search and trending algorithms
- Performance optimization
- Cross-DAW compatibility testing

## Tech Stack

### Frontend (VST Plugin)
- **Framework**: JUCE 7
- **Language**: C++17
- **UI**: Native JUCE components + embedded web views
- **Audio**: Real-time processing with DAW integration

### Backend
- **Language**: Go 1.21+
- **Framework**: Gin (HTTP) + Gorilla (WebSockets)
- **Social**: Stream.io Activity Feeds SDK
- **Database**: PostgreSQL with GORM
- **Storage**: Cloudflare R2 / AWS S3

### DevOps
- **Containerization**: Docker
- **Deployment**: Railway (backend) + direct download (VST)
- **Monitoring**: Built-in health checks
- **CI/CD**: GitHub Actions (planned)

## Contributing

We're building in public! Check out our [development plan](./PLAN.md) and [Claude development guide](./CLAUDE.md).

### Getting Started
1. Join our Discord community
2. Pick an issue labeled `good-first-issue`
3. Fork, implement, and submit a PR
4. Become part of the producer social revolution

### Areas We Need Help With
- **C++/JUCE Experts**: VST compatibility and performance
- **Go Backend**: API design and Stream.io integration
- **Audio Processing**: Loudness normalization and encoding
- **UI/UX**: Producer-focused interface design
- **Testing**: Cross-DAW compatibility

## Community

- **Discord**: [Join our community](https://discord.gg/sidechain) (coming soon)
- **Reddit**: r/SidechainVST for discussions and feedback
- **Twitter**: [@SidechainVST](https://twitter.com/sidechainvst) for updates

## License

MIT License - Build on top of Sidechain, sell it, remix it, we don't care. Just make something cool.

## FAQ

**Q: Will this crash my DAW?**
A: We're obsessive about stability. All network operations run on background threads, and we test extensively with high CPU projects.

**Q: What about copyright issues?**
A: Phase 1 focuses on original content sharing. Audio fingerprinting for copyright protection is planned for v2.

**Q: Which DAWs are supported?**
A: Starting with Ableton Live (most producers), then FL Studio and Logic Pro. VST3/AU compatibility means most DAWs should work.

**Q: Is my audio data private?**
A: You control what you share. Private sessions stay private. All uploaded audio can be deleted anytime.

**Q: How much does it cost?**
A: Free for basic features. Pro tier ($5/month) planned with unlimited uploads and analytics.

---

*Built with ❤️ by producers, for producers*