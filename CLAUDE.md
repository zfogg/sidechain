# Claude Development Guide

## Project Overview
Sidechain is a social VST plugin that enables music producers to share and discover loops directly from their DAW. It consists of:
- **VST Plugin**: C++ with JUCE framework for audio capture and social feed UI
- **Backend**: Go server with getstream.io integration for social features
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
# Build VST plugin (smart - only regenerates when needed)
make plugin

# Build VST plugin (fast - skip project regeneration)
make plugin-fast

# Generate/update plugin project files (force regeneration)
make plugin-project

# Build debug version
make plugin-debug

# Clean plugin build files
make plugin-clean
```

**Development Tip**: Use `make plugin-fast` for iterative development - it's ~40% faster by skipping project file regeneration. Only use `make plugin` when you add new source files.

**CMake Presets**: The project includes CMake presets for easier configuration:
```bash
# List available presets
cmake --list-presets -S plugin

# Configure using a preset (automatically uses Ninja generator)
cmake --preset default -S plugin -B plugin/build
cmake --preset debug -S plugin -B plugin/build
cmake --preset release -S plugin -B plugin/build

# Build using a preset
cmake --build --preset default
```

**Generator Note**: The Makefile automatically detects and uses Ninja if available for faster parallel builds. CMake presets also default to Ninja. If you run `cmake` directly, specify `-G Ninja` or use a preset.

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
# Install all dependencies (JUCE 8.0.11 + Go packages)
make install-deps

# Setup environment variables
cp backend/.env.example backend/.env
# Edit backend/.env with your getstream.io credentials

# Check what was installed
make deps-info
```

## C++ Standards & Modern Features

### C++26 Baseline
The Sidechain VST plugin uses **C++26** as the baseline standard, configured in `plugin/cmake/CompilerFlags.cmake`.

**C++ Version Timeline**:
- **C++17**: Baseline prior to Phase 1
- **C++20**: Legacy/deprecated (JUCE compatibility)
- **C++26**: Current standard (as of Phase 1, December 2024)

### Compiler Support

| Platform | Compiler | Version | C++26 Support |
|----------|----------|---------|---------------|
| macOS    | Clang    | 16+     | ✅ Full       |
| Linux    | GCC      | 14+     | ✅ Full       |
| Windows  | MSVC     | 2022+   | ⚠️ Partial    |

### Approved Modern C++ Features

#### Smart Pointers & Memory Management
- `std::unique_ptr<T>` - For exclusive ownership (primary pattern)
- `std::shared_ptr<T>` - For shared ownership (caches, observers)
- **NO** raw `new`/`delete` - Always use RAII
- **NO** `new` in audio thread - Pre-allocate all resources

**Pattern**:
```cpp
std::unique_ptr<AudioCapture> capture;  // Owns the resource
std::shared_ptr<ImageCache> imageCache;  // Shared across UI components
```

#### Modern Type Features
- `std::optional<T>` - For optional values (C++17)
- `std::variant<Args...>` - Type-safe unions (C++17)
- Structured bindings: `auto [x, y] = getPoint();` (C++17)
- Concepts (C++20) - For generic constraint validation

**Pattern**:
```cpp
std::optional<User> getUser(const juce::String& id);  // May not exist
auto [success, data] = loadFromFile(path);  // Structured binding
```

#### Concurrency & Threading (Audio-Safe)
- `std::atomic<T>` - Lock-free inter-thread communication (audio thread safe)
- `std::thread` - For background operations (never on audio thread)
- `std::mutex` / `std::shared_mutex` - For protecting shared data (not on audio thread)
- **NO** locks on audio thread - Use lock-free or pre-computed data

**Pattern**:
```cpp
std::atomic<bool> isRecording { false };  // Audio thread reads, UI thread writes
std::mutex dataLock;  // Only used on UI/background threads
```

#### Functional Programming (C++17+)
- `std::function<>` - For callbacks and async operations
- Lambda expressions with capture: `[this, &ref](auto x) { /* ... */ }`
- Higher-order functions: map, filter, reduce patterns

**Pattern**:
```cpp
Async::run<juce::Image>(
    [this]() { return downloadImage(); },  // Background work
    [this](const auto& img) { setImage(img); }  // UI update
);
```

#### Reactive Programming (With RxCpp - Phase 1)
- Observable/Observer pattern: `rx::observable<T>`
- Operators: `map()`, `filter()`, `flatMap()`, `debounce()`
- Schedulers: Thread management with `subscribe_on()` / `observe_on()`

**Pattern**:
```cpp
networkClient->getFeed(type)
    .pipe(
        rx::operators::map([](auto data) { return parseFeed(data); }),
        rx::operators::debounce(std::chrono::milliseconds(300))
    )
    .subscribe([this](auto posts) { displayFeed(posts); });
```

#### Container & Algorithm Features (C++17+)
- Range-based for loops: `for (const auto& item : container)`
- `std::find_if()` / `std::transform()` / `std::accumulate()`
- `juce::Array<T>` - JUCE's container (preferred for JUCE code)
- `std::vector<T>` - Standard container (for generic code)
- **NO** STL containers on audio thread - Use lock-free only

**Pattern**:
```cpp
auto it = std::find_if(posts.begin(), posts.end(),
    [id](const FeedPost& p) { return p.id == id; });
```

#### Error Handling (Custom Outcome<T> Pattern)
- Custom `Outcome<T, E>` type - Result/error wrapper
- Monadic operations: `.map()`, `.flatMap()`, `.onSuccess()`, `.onError()`
- **NO** exceptions in networking layer
- **Optional**: `std::expected<T, E>` when upgrading to C++23 fully

**Pattern**:
```cpp
fetchUser(id)
    .flatMap([](const User& u) { return fetchProfile(u.id); })
    .onSuccess([this](const Profile& p) { displayProfile(p); })
    .onError([this](const juce::String& err) { showError(err); });
```

### Features to Avoid

#### C++26 Features Not Yet Adopted
- Reflection (still experimental)
- Pattern matching (partial in some compilers)
- Contracts (requires compiler support)
- Modules (ecosystem not ready for JUCE)

#### Forbidden Patterns
- **C-style casts**: Use `static_cast<>`, `dynamic_cast<>`, `const_cast<>`
- **Global variables**: Inject via constructors
- **Macros**: Use `constexpr` or `inline` functions instead
- **Raw pointers**: Use `unique_ptr` or `shared_ptr`
- **Exceptions on audio thread**: Pre-allocate, validate input before audio thread
- **Allocation on audio thread**: Pre-allocate in constructor/setup

### Compiler Flags

**C++26 Configuration** (`plugin/cmake/CompilerFlags.cmake`):
```cmake
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

**Compiler-Specific Notes**:
- **Clang**: Full C++26 support, use `-std=c++26`
- **GCC**: Full C++26 support as of GCC 14, use `-std=c++26`
- **MSVC**: Limited C++26, use `/std:c++latest` for experimental features

### Dependencies & External Libraries

| Library      | Purpose            | Standard | Status     |
|--------------|-------------------|----------|-----------|
| JUCE 8.0.11  | Audio/UI          | C++17    | Stable    |
| RxCpp 4.1.1  | Reactive patterns | C++14+   | Stable    |
| websocketpp  | WebSocket         | C++11+   | Stable    |
| stduuid      | UUID generation   | C++20+   | Stable    |
| ASIO 1.14.1  | Async I/O         | C++11+   | Stable    |

### Learning Resources

- **Modern C++**: Scott Meyers "Effective Modern C++" (20 items on C++17/20)
- **Concurrent Code**: "C++ Concurrency in Action" (thread safety)
- **JUCE**: https://docs.juce.com/master/index.html
- **RxCpp**: https://github.com/ReactiveX/RxCpp

### Questions?

Consult the **Architecture Notes** below for threading model and safety constraints. When in doubt, prioritize safety on the audio thread over convenience.

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
- Never expose getstream.io server keys to VST
- Use JWT tokens for VST authentication
- Validate all audio uploads server-side

### Performance
- Cache feed data locally in VST
- Compress audio before upload (target 128kbps MP3)
- Use CDN for global audio distribution

## Git
Only commit what you're working on. Other Claudes may be working in parallel,
or the user may be. Be careful with git reset and git checkout and git stash.

