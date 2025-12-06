# CMake Modularization Proposal

## Current State

- **Single monolithic CMakeLists.txt** (~1004 lines)
- All sources compiled directly into one plugin target
- Test library duplicates source compilation
- Slower incremental builds as project grows
- Hard to test components in isolation

## Benefits of Modularization

1. **Faster Incremental Builds**: Only rebuild changed modules
2. **Better Parallelization**: Modules compile independently in parallel
3. **Easier Testing**: Test libraries in isolation
4. **Clearer Dependencies**: Explicit library dependencies
5. **Better Maintainability**: Smaller, focused CMake files

## Proposed Structure

```
plugin/
├── CMakeLists.txt                    # Main file - minimal, delegates to subdirs
├── cmake/
│   ├── SidechainUtil.cmake          # Utility functions
│   ├── SidechainDependencies.cmake  # External deps (ASIO, websocketpp, etc.)
│   └── SidechainOptions.cmake       # CMake options
└── src/
    ├── core/
    │   └── CMakeLists.txt           # PluginProcessor/PluginEditor only
    ├── util/
    │   └── CMakeLists.txt           # SidechainUtil library
    ├── models/
    │   └── CMakeLists.txt           # SidechainModels library (depends on util)
    ├── network/
    │   └── CMakeLists.txt           # SidechainNetwork library (depends on util)
    ├── audio/
    │   └── CMakeLists.txt           # SidechainAudio library (depends on util, network)
    ├── stores/
    │   └── CMakeLists.txt           # SidechainStores library (depends on util, models, network)
    └── ui/
        └── CMakeLists.txt           # SidechainUI library (depends on all above)
```

## Dependency Hierarchy

```
core (PluginProcessor/Editor)
  └─ ui
      └─ stores
          ├─ models
          │   └─ util
          └─ network
              ├─ util
              └─ external deps (asio, websocketpp)
      └─ audio
          ├─ util
          └─ network
              ├─ util
              └─ external deps
      └─ util
```

## Module Libraries

### 1. SidechainUtil (Static Library)
- Base utilities with minimal dependencies
- Sources: util/*.cpp
- Dependencies: JUCE core modules only

### 2. SidechainModels (Static Library)
- Data structures and DTOs
- Sources: models/*.cpp
- Dependencies: SidechainUtil, JUCE core

### 3. SidechainNetwork (Static Library)
- Network clients and WebSocket
- Sources: network/*.cpp
- Dependencies: SidechainUtil, ASIO, websocketpp, JUCE

### 4. SidechainAudio (Static Library)
- Audio capture, playback, analysis
- Sources: audio/*.cpp
- Dependencies: SidechainUtil, SidechainNetwork, libkeyfinder, JUCE audio modules

### 5. SidechainStores (Static Library)
- State management and caching
- Sources: stores/*.cpp
- Dependencies: SidechainUtil, SidechainModels, SidechainNetwork, JUCE

### 6. SidechainUI (Static Library)
- All UI components
- Sources: ui/**/*.cpp
- Dependencies: All above libraries, JUCE GUI modules

### 7. Sidechain (Plugin Target)
- Plugin entry points only
- Sources: PluginProcessor.cpp, PluginEditor.cpp
- Dependencies: All library modules

## Example: Modular CMakeLists.txt Structure

### Main CMakeLists.txt (simplified)
```cmake
cmake_minimum_required(VERSION 3.22)
project(Sidechain VERSION ${SIDECHAIN_VERSION} LANGUAGES CXX C)

# Include configuration
include(cmake/SidechainOptions.cmake)
include(cmake/SidechainDependencies.cmake)
include(cmake/SidechainUtil.cmake)

# Build libraries in dependency order
add_subdirectory(src/util)
add_subdirectory(src/models)
add_subdirectory(src/network)
add_subdirectory(src/audio)
add_subdirectory(src/stores)
add_subdirectory(src/ui)
add_subdirectory(src/core)  # Plugin target - depends on all libs
```

### src/util/CMakeLists.txt (example)
```cmake
add_library(SidechainUtil STATIC
    Time.cpp Time.h
    Json.cpp Json.h
    Log.cpp Log.h
    # ... etc
)

target_include_directories(SidechainUtil PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(SidechainUtil PUBLIC
    juce::juce_core
    juce::juce_data_structures
)

# Set visibility to hidden by default
set_target_properties(SidechainUtil PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
```

### src/network/CMakeLists.txt (example)
```cmake
add_library(SidechainNetwork STATIC
    NetworkClient.cpp NetworkClient.h
    WebSocketClient.cpp WebSocketClient.h
    # ... etc
)

target_include_directories(SidechainNetwork PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(SidechainNetwork PUBLIC
    SidechainUtil
    juce::juce_core
    juce::juce_events
    juce::juce_cryptography
)

if(SIDECHAIN_HAS_ASIO)
    target_link_libraries(SidechainNetwork PRIVATE asio)
endif()

if(SIDECHAIN_HAS_WEBSOCKETPP)
    target_link_libraries(SidechainNetwork PRIVATE websocketpp)
endif()
```

### src/core/CMakeLists.txt (plugin target)
```cmake
juce_add_plugin(Sidechain
    # ... plugin metadata ...
)

target_sources(Sidechain PRIVATE
    PluginProcessor.cpp PluginProcessor.h
    PluginEditor.cpp PluginEditor.h
)

target_link_libraries(Sidechain PRIVATE
    SidechainUtil
    SidechainModels
    SidechainNetwork
    SidechainAudio
    SidechainStores
    SidechainUI
    # ... JUCE modules ...
)
```

## Migration Strategy

### Phase 1: Extract Utilities (Low Risk)
1. Create `src/util/CMakeLists.txt`
2. Build `SidechainUtil` library
3. Update main target to link against it
4. Test build

### Phase 2: Extract Models & Network (Medium Risk)
1. Extract models library
2. Extract network library
3. Update dependencies
4. Test build

### Phase 3: Extract Audio, Stores, UI (Higher Risk)
1. Extract remaining libraries
2. Update all dependencies
3. Comprehensive testing

### Phase 4: Cleanup
1. Remove duplicate test library code
2. Update test CMakeLists.txt to use libraries
3. Document new structure

## Considerations

### When to Modularize?

**Do it now if:**
- Build times are becoming slow (>30s incremental)
- You want better test isolation
- Multiple developers working on different modules
- Project is actively growing

**Consider waiting if:**
- Project is stable/small
- Build times are acceptable
- Planning major refactoring soon

### Trade-offs

**Pros:**
- Faster incremental builds
- Better organization
- Easier testing
- Clear dependency graph

**Cons:**
- Initial migration effort
- More CMake files to maintain
- Need to understand dependency graph
- Potential linker issues if circular deps exist

## Recommendation

**Yes, reorganize now**, but incrementally:

1. Start with `SidechainUtil` (lowest risk, immediate benefit)
2. Then `SidechainModels` and `SidechainNetwork`
3. Finally `SidechainAudio`, `SidechainStores`, and `SidechainUI`

This approach:
- Provides immediate benefits (faster util rebuilds)
- Allows learning/iteration
- Minimizes risk
- Can be done over time

## Alternative: Keep Current Structure

If you decide to keep the monolithic structure:
- Add clear comments separating sections
- Consider using CMake functions to reduce duplication
- Document the dependency graph in comments
- Revisit when build times become problematic

