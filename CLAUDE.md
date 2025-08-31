# Claude Development Guide

## Project Overview
Sidechain is a social VST plugin that enables music producers to share and discover loops directly from their DAW. It consists of:
- **VST Plugin**: C++ with JUCE framework for audio capture and social feed UI
- **Backend**: Go server with Stream.io integration for social features
- **Infrastructure**: Audio CDN, authentication, and real-time updates

## Development Commands

### Quick Start
```bash
# Install all dependencies (JUCE, Go packages)
make install-deps

# Build everything
make all

# Start development environment
make dev
```

### VST Plugin (C++/JUCE)
```bash
# Build VST plugin
make plugin

# Generate/update plugin project files
make plugin-project

# Clean plugin build files
make plugin-clean
```

### Backend (Go)
```bash
# Build backend
make backend

# Run built backend
make backend-run

# Start development server (with hot reload)
make backend-dev

# Run tests
make test-backend

# Or manual commands:
cd backend
go run cmd/server/main.go
go test ./...
```

### Database
```bash
# Run migrations
cd backend
go run cmd/migrate/main.go up

# Create new migration
go run cmd/migrate/main.go create migration_name
```

### Testing
```bash
# Test VST plugin (requires DAW)
# 1. Build plugin with: make
# 2. Load in Ableton Live
# 3. Test audio capture and upload

# Test backend API
cd backend
go test ./internal/handlers/... -v

# Integration tests
go test ./tests/integration/... -v
```

### Environment Setup
```bash
# Install all dependencies (JUCE 8.0.8 + Go packages)
make install-deps

# Setup environment variables
cp backend/.env.example backend/.env
# Edit backend/.env with your Stream.io credentials

# Check what was installed
make deps-info
```

### Deployment
```bash
# Build release version
make release

# Deploy backend
docker build -t sidechain-backend .
docker run -p 8080:8080 sidechain-backend

# Package VST for distribution
make package
```

## Common Development Tasks

### Adding New API Endpoints
1. Define route in `backend/internal/routes/`
2. Implement handler in `backend/internal/handlers/`
3. Add tests in corresponding `*_test.go` file
4. Update VST networking code in `plugin/Source/`

### Audio Processing Changes
1. Modify audio capture in `plugin/Source/AudioCapture.cpp`
2. Update backend processing in `backend/internal/audio/`
3. Test with various audio formats and DAWs

### UI/Feed Changes
1. Frontend changes: `plugin/Source/UI/`
2. Backend feed logic: `backend/internal/stream/`
3. Test social interactions and real-time updates

## Troubleshooting

### VST Issues
- **Plugin not loading**: Check VST directory permissions
- **Audio dropouts**: Ensure network calls are on background threads
- **Crashes**: Enable debug symbols and check JUCE logs

### Backend Issues
- **Stream.io errors**: Check API keys and rate limits
- **Audio upload failures**: Verify CDN configuration and file permissions
- **WebSocket disconnects**: Check connection pooling and timeouts

## Architecture Notes

### Threading Model
- **Audio Thread**: Never block, no network calls
- **UI Thread**: Handle user interactions, update feed
- **Background Threads**: Network requests, file I/O

### Security
- Never expose Stream.io server keys to VST
- Use JWT tokens for VST authentication
- Validate all audio uploads server-side

### Performance
- Cache feed data locally in VST
- Compress audio before upload (target 128kbps MP3)
- Use CDN for global audio distribution