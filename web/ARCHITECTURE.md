# Sidechain Web Frontend - Architecture Guide

## Overview

This is a modern React web frontend for the Sidechain VST plugin built with production-grade patterns. It mirrors the sophisticated C++ plugin architecture with type-safe error handling, reactive state management, modular API clients, and real-time updates.

## Tech Stack

- **Framework**: React 18 + TypeScript 5 + Vite 5
- **State Management**: Zustand (client state) + React Query (server state)
- **Styling**: Tailwind CSS v3 + shadcn/ui components
- **Real-time**: WebSocket (backend) + getstream.io Chat SDK
- **Audio**: Howler.js (cross-browser playback)
- **HTTP**: Axios with Outcome<T> error pattern
- **Package Manager**: pnpm v9 (locked with corepack)

## Architecture Layers

### 1. API Client Layer (`src/api/`)

All HTTP communication goes through type-safe, domain-specific clients:

```
Request → ApiClient (Axios) → Outcome<T> → Caller
```

**Key Files:**
- `types.ts` - `Outcome<T>` type with monadic operations (map, flatMap, onSuccess, onError)
- `client.ts` - Base Axios instance with interceptors and token management
- `FeedClient.ts` - Feed operations (get, like, save, react, track play)
- `CommentsClient.ts` - Comment CRUD with threading
- `AuthClient.ts` - Authentication (login, register, refresh, claim device)
- `websocket.ts` - Backend WebSocket with reconnection logic

**Outcome<T> Pattern:**
```typescript
// Instead of try-catch
const result = await FeedClient.getFeed(feedType, limit, offset)
if (result.isOk()) {
  const posts = result.getValue()
} else {
  const error = result.getError()
}

// Functional composition
result
  .map(posts => posts.filter(p => p.isLiked))
  .flatMap(liked => processPosts(liked))
  .onSuccess(processed => displayPosts(processed))
  .onError(err => showError(err))
```

### 2. Data Model Layer (`src/models/`)

Type-safe models with static factory methods:

```typescript
class FeedPostModel {
  static fromJson(json: any): FeedPost
  static isValid(post: FeedPost): boolean
  static toJson(post: FeedPost): any
}
```

**Benefits:**
- API response parsing in one place
- Field mapping (snake_case ↔ camelCase)
- Default values for optional fields
- Validation logic centralized

### 3. State Management

**Server State (React Query):**
- Automatic caching (staleTime: 60s, gcTime: 5m)
- Infinite scroll pagination
- Request deduplication
- Automatic refetching

**Client State (Zustand):**
- Feed state with optimistic updates
- Auth state with localStorage persistence
- UI state (modals, notifications, theme)
- Chat state (channels, messages, typing)
- Draft state (persisted)

### 4. Hooks Layer (`src/hooks/`)

**Query Hooks:**
```typescript
const { data, fetchNextPage, hasNextPage } = useFeedQuery(feedType)
```

**Mutation Hooks:**
```typescript
const { mutate: toggleLike } = useLikeMutation()
toggleLike({ postId, shouldLike: true })
// Automatically: optimistic update → API call → confirm/rollback
```

### 5. Component Layer (`src/components/`)

**UI Components (shadcn/ui):**
- Button, Input, Label, Card, Dialog, DropdownMenu, Spinner

**Feature Components:**
- `PostCard` - Audio playback + interactions + metadata
- `FeedList` - Infinite scroll with pagination
- `CommentsPanel` - Comment thread in dialog
- `CommentRow` - Single comment with like button
- `CommentComposer` - New comment form

### 6. Page Layer (`src/pages/`)

**Public Pages:**
- `Login.tsx` - Email/password + Google/Discord OAuth
- `AuthCallback.tsx` - OAuth redirect handler

**Protected Pages:**
- `Feed.tsx` - Main feed with type selector (Timeline/Global/Trending/ForYou)

### 7. Provider Layer (`src/providers/`)

**QueryProvider:**
- React Query setup with sensible defaults
- Stale time: 60s, GC time: 5m, retry: 1

**AuthProvider:**
- Session restoration from localStorage
- WebSocket initialization on login

**ChatProvider:**
- Stream Chat client initialization
- Token acquisition from backend
- Error handling with graceful degradation

## Data Flow

### Optimistic Updates Pattern

Critical for smooth UX - mirrors C++ plugin pattern:

```
User clicks like button
    ↓
1. Update UI immediately (optimistic)
2. Dispatch API request in background
3. On success: Keep UI update
4. On error: Rollback to previous state
```

**Implementation:**

```typescript
// onMutate: Optimistic update
// onSuccess: Confirm update
// onError: Rollback
const { mutate } = useMutation({
  mutationFn: async (data) => {
    const result = await FeedClient.toggleLike(...)
    if (result.isError()) throw new Error(result.getError())
  },
  onMutate: async ({ postId, shouldLike }) => {
    // Save previous state
    const previous = queryClient.getQueryData(['feed'])

    // Update UI immediately
    queryClient.setQueryData(['feed'], (old) => {
      return updatePostInFeed(old, postId, { isLiked: shouldLike })
    })

    return { previous }
  },
  onError: (err, vars, context) => {
    // Rollback on error
    queryClient.setQueryData(['feed'], context.previous)
  }
})
```

### Real-time Updates

WebSocket messages trigger Zustand store updates:

```
Backend event (new_post)
    ↓
WebSocket message received
    ↓
useWebSocket hook processes event
    ↓
useFeedStore.addPostToFeed()
    ↓
Component re-renders with new post
```

## Key Design Patterns

### 1. Outcome<T> for Errors

Never throw exceptions in API layer:

```typescript
// ✅ Good
const result = await fetchUser(id)
if (result.isOk()) { /* use data */ }
else { /* handle error */ }

// ❌ Bad
try {
  const user = await fetchUser(id) // throws
} catch (e) { /* handle */ }
```

### 2. Modular API Clients

Domain-specific clients instead of monolithic service:

```typescript
// ✅ Good
await FeedClient.toggleLike(postId, shouldLike)
await CommentsClient.createComment(postId, content)

// ❌ Bad
await ApiService.request('POST', '/social/like', { postId })
```

### 3. Type-Safe Models

Static factory methods for JSON parsing:

```typescript
// ✅ Good
const post = FeedPostModel.fromJson(apiResponse)
if (FeedPostModel.isValid(post)) { /* use post */ }

// ❌ Bad
const post = apiResponse as FeedPost // Unsafe cast
```

### 4. Zustand for Side Effects

State updates trigger side effects automatically:

```typescript
// ✅ Good - Zustand action
feedStore.toggleLike(postId) // Updates UI + stores + subscribes

// ❌ Bad - Manual management
setIsLiked(!isLiked)
await toggleLike()
updateLikeCount()
```

### 5. Separation of Concerns

Each layer has a single responsibility:

```
API Layer: HTTP communication + error handling
Model Layer: JSON parsing + validation
Store Layer: State management + side effects
Hook Layer: React Query + custom logic
Component Layer: UI rendering only
```

## Performance Optimizations

### Code Splitting

Vite automatically creates chunks:

```
stream-io.js  (getstream.io - 200kb)
audio.js      (Howler.js - 50kb)
vendor.js     (React, React-DOM - 300kb)
query.js      (React Query - 100kb)
index.js      (App code - 150kb)
```

### Caching Strategy

```typescript
// React Query
staleTime: 60s      // Consider data fresh for 1 minute
gcTime: 5m          // Keep in memory for 5 minutes
retry: 1            // Retry failed requests once

// Zustand
persist()           // Save to localStorage

// Browser Cache
no-cache headers    // Always validate with server
```

### Request Deduplication

React Query automatically deduplicates identical requests:

```typescript
// Only one request sent even if called 3 times
useFeedQuery('timeline')
useFeedQuery('timeline')
useFeedQuery('timeline')
```

## Error Handling

### API Errors

All errors converted to `Outcome<T>`:

```typescript
const result = await FeedClient.getFeed(...)
if (result.isError()) {
  const message = result.getError()
  useUIStore.addNotification({ type: 'error', message })
}
```

### Network Errors

WebSocket auto-reconnects with exponential backoff:

```
Attempt 1: wait 1s
Attempt 2: wait 2s
Attempt 3: wait 4s
Attempt 4: wait 8s
Attempt 5: wait 16s
Attempts 6-10: wait 30s (max)
Give up after 10 attempts
```

### UI Errors

Error boundaries can wrap routes:

```typescript
<ErrorBoundary>
  <Route path="/feed" element={<Feed />} />
</ErrorBoundary>
```

## Testing Strategy

### Unit Tests
- Outcome<T> operations
- Model validation
- Store actions
- Mutation logic

### Integration Tests
- Login flow (email + OAuth)
- Device claiming
- Feed pagination
- Optimistic updates
- Real-time WebSocket updates

### E2E Tests
- Complete user journeys
- Audio playback
- Comment threads
- Like/save interactions

## Files to Know

| Path | Purpose |
|------|---------|
| `api/types.ts` | Outcome<T> implementation |
| `api/client.ts` | Axios + token management |
| `stores/useFeedStore.ts` | Feed state + optimistic |
| `hooks/queries/useFeedQuery.ts` | Infinite scroll |
| `hooks/mutations/*.ts` | Mutation logic |
| `components/feed/PostCard.tsx` | Core UI component |
| `providers/*.tsx` | Setup + initialization |
| `App.tsx` | Routing + provider nesting |

## Development Workflow

### Add New Feature

1. **Create API client** (`api/NewFeatureClient.ts`)
   - Type-safe methods returning `Outcome<T>`
   - Handle JSON parsing with models

2. **Create models** (`models/NewFeature.ts`)
   - Factory methods: fromJson, toJson, isValid

3. **Create hooks** (`hooks/mutations/useNewFeature.ts`)
   - Mutation with optimistic updates
   - Rollback on error

4. **Create components** (`components/NewFeature.tsx`)
   - Use hooks for data/mutations
   - Render with loading states
   - Handle errors gracefully

5. **Integrate to store** (`stores/useNewStore.ts`)
   - Add state for feature
   - Subscribe to WebSocket updates

6. **Test end-to-end**
   - Unit test model validation
   - Integration test API flow
   - E2E test user interaction

## Security

- JWT tokens stored in localStorage (cleared on logout)
- HTTPS enforced in production
- API tokens injected via Axios interceptors
- Stream Chat tokens obtained from backend
- CORS handled by backend

## Deployment

### Build

```bash
pnpm build
# Creates optimized dist/ with code splitting
```

### Deploy

```bash
# Upload dist/ to CDN/hosting
# Set environment variables:
VITE_API_URL=https://api.sidechain.com
VITE_STREAM_API_KEY=...
VITE_GOOGLE_CLIENT_ID=...
VITE_DISCORD_CLIENT_ID=...
```

### Monitor

- Error tracking (Sentry/similar)
- Performance monitoring (web vitals)
- API usage metrics
- User analytics (Posthog/similar)

## Further Reading

- [React Query Docs](https://tanstack.com/query/latest)
- [Zustand Docs](https://github.com/pmndrs/zustand)
- [Tailwind Docs](https://tailwindcss.com)
- [Stream Chat Docs](https://getstream.io/chat/)
- [TypeScript Handbook](https://www.typescriptlang.org/docs/)
