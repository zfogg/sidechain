# Compilation Fixes Needed

**Status:** Priority 1 & 2 implementation in progress  
**Remaining Build Errors:** ~11 errors related to type mismatches

---

## Current Build Errors

### 1. Type Mismatch in Playlists.cpp (Line 66)
```cpp
// ERROR: no viable overloaded '='
state.playlists = playlistsList;  // juce::Array<juce::var> → vector<shared_ptr<Playlist>>
```

**Fix Required:**
- Parse `juce::Array<juce::var>` to `std::vector<std::shared_ptr<Playlist>>`
- Similar fixes needed in Stories.cpp, Sounds.cpp, etc.

### 2. ValidationError Namespace Issues
Multiple files referencing `ValidationError` without `Json::` prefix:
- Draft.h:59
- Conversation.h:54  
- Message.h:112
- Notification.h:56
- User.h:141

**Fix Required:**
```cpp
// Change
SIDECHAIN_JSON_TYPE(Draft)  // Uses ValidationError

// To use
Json::ValidationError  // Properly namespaced
```

### 3. EntityStore nlohmann::json vs juce::var Mismatch
`EntityStore.h:213` - `FeedPost::fromJson()` expects `juce::var` but receiving `nlohmann::json`

**Fix Required:**
- Either convert `nlohmann::json` → `juce::var` before calling
- Or update `FeedPost::fromJson()` to accept `nlohmann::json` (breaking change)

---

## Completed Changes

### ✅ Backend
- Added `POST /api/v1/auth/refresh` endpoint
- Implemented token refresh handler
- Route registered in main.go

### ✅ Plugin  
- Added `AUTH_REFRESH` constant
- Added `refreshAuthToken()` to NetworkClient.h
- Implemented refresh logic in AuthClient.cpp
- Fixed `EntityStore.h` Timer callback API (JUCE Timer issue)
- Added missing includes: Story.h, Playlist.h to AppState.h
- Fixed namespace issues for Story/Playlist/Sound models

---

## Recommended Fix Approach

### Short Term (Get It Building)
1. Comment out broken state assignments in:
   - `Playlists.cpp:66`
   - `Stories.cpp` (similar lines)
   - `Sounds.cpp` (similar lines)

2. Add `using Json::ValidationError` in affected model files

3. Convert `nlohmann::json` to `juce::var` in EntityStore:
```cpp
// EntityStore.h line 213
auto jsonVar = Json::nlohmannToJuceVar(data);  // Helper needed
auto updatedPost = FeedPost::fromJson(jsonVar);
```

### Long Term (Proper Fix)
1. **Migrate all models to nlohmann::json** (Preferred)
   - Update FeedPost, Story, Playlist, Sound to use nlohmann::json
   - Remove juce::var dependency from models
   - Consistent with new models (User, Message, Notification, etc.)

2. **Update AppStore state parsing**
   - Change from `juce::Array<juce::var>` to typed vectors
   - Parse JSON responses directly to typed models
   - Remove intermediate juce::var step

---

## Build Command
```bash
make plugin-fast  # Current: fails with 11 errors
```

## Priority
**High** - Blocks all plugin development until resolved
**Effort:** 2-3 hours to get building, 1-2 days for proper refactor

---

## Notes
- Token refresh backend/plugin implementation is COMPLETE
- Build errors are from incidental changes to EntityStore/AppState
- Core Priority 1 work is done, just needs compilation fixes
