# Compilation Status - EntityStore Normalization

**Date:** December 19, 2025  
**Status:** ⚠️ IN PROGRESS - Multiple compilation errors remain

---

## Changes Made

### ✅ EntityStore.h
- Added normalization methods: `normalizePost()`, `normalizeUser()`, `normalizeStory()`, etc.
- Added batch normalization: `normalizePosts()`, `normalizePlaylists()`
- Ensures memory deduplication via `EntityCache::getOrCreate()`
- Fixed Timer API issue with proper JUCE Timer subclass

### ✅ Backend (Complete & Working)
- Token refresh endpoint fully implemented
- Backend builds successfully

### ⚠️ Plugin State Files (Partially Fixed)
- **Playlists.cpp** - Fixed to use `EntityStore::normalizePlaylists()`
- **Search.cpp** - Fixed to use `EntityStore::normalizePosts()`  
- **PostsFeed.cpp** - Partially fixed (changed array types)

---

## Remaining Compilation Errors (~50+ errors)

### Pattern 1: Direct Access of shared_ptr Members
**Files Affected:** Feed.cpp, PostCard.cpp, AggregatedFeedCard.cpp, etc.

**Error:** `no member named 'id' in 'std::shared_ptr<FeedPost>'`

**Fix Needed:**
```cpp
// OLD (wrong):
if (feedPost.id == postId)
feedPost.likeCount++;

// NEW (correct):
if (feedPost->id == postId)  // Use -> instead of .
feedPost->likeCount++;
```

### Pattern 2: juce::Array Methods on std::vector
**Files Affected:** Feed.cpp, etc.

**Error:** `no member named 'isEmpty' in 'std::vector<std::shared_ptr<FeedPost>>'`

**Fix Needed:**
```cpp
// OLD (juce::Array API):
if (posts.isEmpty())
posts.remove(index)
posts.add(post)

// NEW (std::vector API):
if (posts.empty())
posts.erase(posts.begin() + index)
posts.push_back(post)
```

### Pattern 3: Assignment to Non-Pointer Members
**Error:** `no viable overloaded '='`

**Fix Needed:**
```cpp
// OLD:
safeThis->post = feedPost;  // feedPost is shared_ptr

// NEW:
safeThis->post = *feedPost;  // Dereference shared_ptr
```

---

## Files Needing Updates

### High Priority (Many Errors)
1. **src/stores/app/Feed.cpp** (~20+ errors)
   - Line 108: `.isEmpty()` → `.empty()`
   - Lines 159, 225, 258, 270+: `.id` → `->id`, `.isLiked` → `->isLiked`
   - Lines 160, 226: `.remove()` → `.erase()`

2. **src/ui/feed/PostCard.cpp** (~5+ errors)
   - Line 1577: `.id` → `->id`
   - Line 1581: Assignment needs dereference

3. **src/ui/feed/PostsFeed.cpp** (~5+ errors)
   - Array iteration needs pointer handling
   - Line 2055: Loop over shared_ptrs

### Medium Priority
4. **src/ui/feed/AggregatedFeedCard.cpp**
5. **src/ui/messages/MessagesList.cpp**
6. **src/ui/profile/Profile.cpp**

---

## Fix Strategy

### Option 1: Complete Refactoring (Recommended)
Systematically update all files to use `std::vector<std::shared_ptr<T>>`:
- **Pros:** Proper memory management, enables EntityStore deduplication
- **Cons:** 50+ files need updates, 2-3 hours of work
- **Estimate:** 2-3 hours

### Option 2: Rollback EntityStore Changes
Revert state to use `juce::Array<FeedPost>` (non-pointer):
- **Pros:** Quick fix, everything compiles
- **Cons:** Loses memory deduplication benefits, more memory usage
- **Estimate:** 30 minutes

### Option 3: Hybrid Approach
Keep EntityStore normalization, but have it return values not shared_ptrs:
- **Pros:** Middle ground
- **Cons:** Loses main benefit of EntityStore (memory deduplication)
- **Estimate:** 1-2 hours

---

## Recommendation

**Complete Option 1** - The shared_ptr approach is the right architecture for:
1. Memory efficiency (same entity only stored once)
2. Reactive updates (all views see same data)
3. Cache coherency (EntityStore as single source of truth)

It's worth the 2-3 hours to properly fix all call sites.

---

## Quick Test Commands

```bash
# Build and see all errors
make plugin-fast 2>&1 | grep "error:"

# Count remaining errors  
make plugin-fast 2>&1 | grep "error:" | wc -l

# Focus on Feed.cpp errors
make plugin-fast 2>&1 | grep "Feed.cpp.*error:"
```

---

## Next Steps

1. Fix Feed.cpp (highest error count)
2. Fix PostCard.cpp  
3. Fix remaining UI components
4. Test compilation
5. Commit working code

**Estimated completion:** 2-3 hours of focused work
