# Sidechain Web Frontend - Testing Guide

## Phase 13: Testing

This document outlines the testing infrastructure and best practices for the Sidechain web frontend.

### Testing Stack

- **Test Runner**: Vitest (Vite-native, faster than Jest)
- **Component Testing**: React Testing Library
- **UI Assertions**: @testing-library/jest-dom
- **Mocking**: Vitest's `vi` module
- **Coverage**: V8 (built-in to Vitest)

### Test Files Structure

```
src/
├── api/
│   └── __tests__/
│       ├── Outcome.test.ts         # Error handling tests
│       ├── FeedClient.test.ts       # API client tests
│       └── ...
├── components/
│   ├── ui/
│   │   └── __tests__/
│   │       └── avatar.test.tsx      # Component tests
│   └── feed/
│       └── __tests__/
│           └── PostCard.test.tsx    # Feature component tests
├── stores/
│   └── __tests__/
│       └── useFeedStore.test.ts    # Store tests
├── utils/
│   └── __tests__/
│       ├── imageOptimization.test.ts   # Utility tests
│       └── monitoring.test.ts
└── test/
    ├── setup.ts                    # Test setup and mocks
    └── utils.tsx                   # Test utilities and helpers
```

### Running Tests

#### All Tests
```bash
pnpm test
```

#### Watch Mode (Auto-run on file changes)
```bash
pnpm test:watch
```

#### Coverage Report
```bash
pnpm test:coverage
```

#### Specific Test File
```bash
pnpm test src/utils/__tests__/imageOptimization.test.ts
```

#### Tests Matching Pattern
```bash
pnpm test --grep "Avatar"
```

### Test Setup (`src/test/setup.ts`)

Global test setup includes:

- **Cleanup**: Automatic cleanup after each test
- **Mocks**: window.matchMedia, IntersectionObserver, localStorage
- **Console**: Error suppression for known warnings
- **Matchers**: @testing-library/jest-dom matchers

### Test Utilities (`src/test/utils.tsx`)

#### renderWithProviders()
Render component with necessary providers (React Query, Auth, etc.):

```typescript
import { renderWithProviders } from '@/test/utils'

const { getByText } = renderWithProviders(<YourComponent />)
```

#### MockDataBuilder
Create realistic test data:

```typescript
import { MockDataBuilder } from '@/test/utils'

const post = MockDataBuilder.createFeedPost({
  likeCount: 10,
  username: 'custom-user'
})

const comment = MockDataBuilder.createComment()
const user = MockDataBuilder.createUser()
const notification = MockDataBuilder.createNotification()
```

### Unit Tests

#### Utility Functions

Test pure functions in isolation:

```typescript
describe('getResponsiveImageSrcSet', () => {
  it('should generate responsive srcset with default sizes', () => {
    const baseUrl = 'https://cdn.example.com/image.jpg'
    const srcset = getResponsiveImageSrcSet(baseUrl)

    expect(srcset).toContain('48w')
    expect(srcset).toContain('96w')
  })
})
```

**Files to Test**:
- `src/utils/imageOptimization.ts` ✅
- `src/utils/monitoring.ts` ✅
- `src/utils/queryConfig.ts` - TODO
- `src/api/types.ts` (Outcome) ✅
- Format/validation utilities

#### API Clients

Test API client methods with mocked HTTP:

```typescript
describe('FeedClient', () => {
  it('should fetch feed and parse posts', async () => {
    const mockResponse = {
      posts: [{ id: '1', ... }]
    }

    // Mock fetch or axios
    vi.mock('@/api/client', () => ({
      apiClient: {
        get: vi.fn().mockResolvedValue(mockResponse)
      }
    }))

    const result = await FeedClient.getFeed('timeline')
    expect(result.isOk()).toBe(true)
  })
})
```

**Files to Test**:
- `src/api/FeedClient.ts` - TODO
- `src/api/CommentsClient.ts` - TODO
- `src/api/SearchClient.ts` - TODO
- `src/api/ReportClient.ts` - TODO

#### Stores (Zustand)

Test state management:

```typescript
describe('useFeedStore', () => {
  it('should load feed', async () => {
    const { result } = renderHook(() => useFeedStore())

    await act(async () => {
      await result.current.loadFeed('timeline')
    })

    expect(result.current.feeds.timeline.posts.length).toBeGreaterThan(0)
  })
})
```

**Files to Test**:
- `src/stores/useFeedStore.ts` - TODO
- `src/stores/useUserStore.ts` - TODO
- `src/stores/useUIStore.ts` - TODO

### Component Tests

#### Simple Components

Test UI components in isolation:

```typescript
describe('Avatar Component', () => {
  it('should render avatar with image', () => {
    render(<Avatar src="url" alt="User" />)

    expect(screen.getByAltText('User')).toBeInTheDocument()
  })
})
```

**Files to Test**:
- `src/components/ui/avatar.tsx` ✅
- `src/components/ui/button.tsx` - TODO
- `src/components/ui/spinner.tsx` - TODO
- `src/components/report/ReportButton.tsx` - TODO

#### Feature Components

Test components with user interactions:

```typescript
describe('PostCard Component', () => {
  it('should like post on button click', async () => {
    const { getByRole } = renderWithProviders(
      <PostCard post={mockPost} />
    )

    const likeButton = getByRole('button', { name: /like/i })
    await userEvent.click(likeButton)

    expect(likeButton).toHaveClass('text-coral-pink')
  })
})
```

**Files to Test**:
- `src/components/feed/PostCard.tsx` - TODO
- `src/components/comments/CommentRow.tsx` - TODO
- `src/components/discovery/TrendingProducerCard.tsx` - TODO

#### Page Components

Test full page interactions:

```typescript
describe('Feed Page', () => {
  it('should load and display feed posts', async () => {
    renderWithProviders(<Feed />)

    await waitFor(() => {
      expect(screen.getByText('Test Post')).toBeInTheDocument()
    })
  })
})
```

**Files to Test**:
- `src/pages/Feed.tsx` - TODO
- `src/pages/Search.tsx` - TODO
- `src/pages/Discovery.tsx` - TODO

### Integration Tests

#### User Workflows

Test complete user flows:

```typescript
describe('Like Post Workflow', () => {
  it('should like a post with optimistic update', async () => {
    renderWithProviders(<PostCard post={post} />)

    const likeButton = screen.getByRole('button', { name: /like/i })
    expect(post.likeCount).toBe(0)

    await userEvent.click(likeButton)

    // Optimistic update shows immediately
    expect(post.likeCount).toBe(1)

    // API call completes
    await waitFor(() => {
      expect(post.isLiked).toBe(true)
    })
  })
})
```

### Mocking Strategies

#### Mock HTTP Requests

```typescript
import { vi } from 'vitest'

vi.mock('@/api/client', () => ({
  apiClient: {
    get: vi.fn().mockResolvedValue({ posts: [...] }),
    post: vi.fn().mockResolvedValue({ success: true }),
  }
}))
```

#### Mock React Query

```typescript
import { QueryClient } from '@tanstack/react-query'

const queryClient = new QueryClient({
  defaultOptions: {
    queries: { retry: false },
    mutations: { retry: false },
  },
})
```

#### Mock Window APIs

```typescript
beforeEach(() => {
  Object.defineProperty(window, 'scrollTo', {
    value: vi.fn(),
    writable: true,
  })
})
```

### Coverage Goals

**Current Coverage Targets**:
- Lines: 70%
- Functions: 70%
- Branches: 70%
- Statements: 70%

**Coverage by Module**:
- **API Clients**: 90% (critical path)
- **Stores**: 85% (state management)
- **Utils**: 80% (helper functions)
- **Components**: 75% (UI snapshots not required)
- **Pages**: 70% (integration)

### Best Practices

1. **Test Behavior, Not Implementation**
   ```typescript
   // ✅ Good - tests what user sees
   expect(screen.getByText('Like')).toHaveClass('text-coral-pink')

   // ❌ Bad - tests internal state
   expect(component.state.isLiked).toBe(true)
   ```

2. **Use Semantic Queries**
   ```typescript
   // ✅ Good - accessible
   getByRole('button', { name: /like/i })

   // ❌ Bad - fragile
   getByTestId('like-btn')
   ```

3. **Test User Interactions**
   ```typescript
   // ✅ Good - simulates real usage
   await userEvent.click(likeButton)

   // ❌ Bad - doesn't test integration
   component.props.onClick()
   ```

4. **Async Patterns**
   ```typescript
   // ✅ Good - waits for data
   await waitFor(() => {
     expect(screen.getByText('Loaded')).toBeInTheDocument()
   })

   // ❌ Bad - races condition
   expect(screen.getByText('Loaded')).toBeInTheDocument()
   ```

5. **Mock External Dependencies**
   ```typescript
   // ✅ Good - isolates code
   vi.mock('@/api/client')

   // ❌ Bad - makes real requests
   // No mock, actual API calls in tests
   ```

### Debugging Tests

#### Run Single Test
```bash
pnpm test --grep "Avatar should render"
```

#### Verbose Output
```bash
pnpm test --reporter=verbose
```

#### Debug Mode
```bash
node --inspect-brk ./node_modules/vitest/vitest.mjs
```

Then open `chrome://inspect` in Chrome.

#### Print Screen
```typescript
import { screen } from '@testing-library/react'

it('should render', () => {
  render(<Component />)
  screen.debug() // Prints entire DOM
})
```

### Common Test Patterns

#### Testing Mutations
```typescript
describe('Like Mutation', () => {
  it('should toggle like status', async () => {
    const { mutate } = useLikeMutation()

    await act(async () => {
      await mutate({ postId: '123', shouldLike: true })
    })

    expect(mockAPICall).toHaveBeenCalledWith({
      postId: '123',
      shouldLike: true
    })
  })
})
```

#### Testing Store Subscriptions
```typescript
describe('Feed Store Subscriptions', () => {
  it('should update when feed changes', () => {
    const store = useFeedStore()
    const subscriber = vi.fn()

    store.subscribe(subscriber)
    act(() => {
      store.switchFeedType('global')
    })

    expect(subscriber).toHaveBeenCalled()
  })
})
```

#### Testing Error States
```typescript
describe('Error Handling', () => {
  it('should display error message on API failure', async () => {
    vi.mock('@/api/client', () => ({
      apiClient: {
        get: vi.fn().mockRejectedValue(new Error('Network error'))
      }
    }))

    render(<Component />)

    await waitFor(() => {
      expect(screen.getByText(/network error/i)).toBeInTheDocument()
    })
  })
})
```

### Test Checklist

- [ ] Unit tests for all utilities
- [ ] Unit tests for all API clients
- [ ] Unit tests for all stores
- [ ] Component tests for all UI components
- [ ] Component tests for feature components
- [ ] Integration tests for critical workflows
- [ ] Error state tests
- [ ] Loading state tests
- [ ] Accessibility tests
- [ ] Performance tests for infinite scroll

### CI/CD Integration

Tests should run automatically on:
- Pull requests (block merge if tests fail)
- Pre-commit (fast subset of tests)
- On-demand (full test suite)

```bash
# Run in CI
pnpm test:ci
```

### Performance Testing

Monitor test execution time:

```bash
pnpm test -- --reporter=verbose
```

Slow tests should be optimized or marked as slow:

```typescript
it('slow integration test', { timeout: 10000 }, async () => {
  // Test that takes 5+ seconds
})
```

### Future Improvements

- [ ] E2E tests with Playwright/Cypress
- [ ] Visual regression tests with Percy/Chromatic
- [ ] Performance benchmarks with Lighthouse CI
- [ ] Accessibility tests with axe-core
- [ ] Visual snapshots for components
