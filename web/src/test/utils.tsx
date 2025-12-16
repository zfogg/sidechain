import { ReactNode } from 'react'
import { render, RenderOptions } from '@testing-library/react'
import { QueryProvider } from '@/providers/QueryProvider'

/**
 * Custom render function that includes providers
 * Use this instead of render() in component tests
 */
export function renderWithProviders(
  ui: ReactNode,
  options?: Omit<RenderOptions, 'wrapper'>
) {
  function Wrapper({ children }: { children: ReactNode }) {
    return <QueryProvider>{children}</QueryProvider>
  }

  return render(ui, { wrapper: Wrapper, ...options })
}

/**
 * Mock API response builder
 * Helps construct realistic test data
 */
export class MockDataBuilder {
  static createFeedPost(overrides = {}) {
    return {
      id: 'post-123',
      foreignId: 'activity-123',
      actor: 'user:user-123',
      verb: 'posted',
      object: 'track:track-123',
      userId: 'user-123',
      username: 'testuser',
      displayName: 'Test User',
      userAvatarUrl: 'https://api.dicebear.com/7.x/avataaars/svg?seed=user-123',
      audioUrl: 'https://cdn.example.com/audio.mp3',
      waveformUrl: 'https://cdn.example.com/waveform.png',
      filename: 'Test Loop',
      durationSeconds: 30,
      bpm: 120,
      key: 'C',
      daw: 'Ableton Live',
      genre: ['House', 'Techno'],
      likeCount: 5,
      playCount: 20,
      commentCount: 2,
      saveCount: 1,
      repostCount: 0,
      isLiked: false,
      isSaved: false,
      isReposted: false,
      isFollowing: false,
      isOwnPost: false,
      reactionCounts: { like: 5 },
      userReaction: '',
      createdAt: new Date(),
      updatedAt: new Date(),
      ...overrides,
    }
  }

  static createComment(overrides = {}) {
    return {
      id: 'comment-123',
      postId: 'post-123',
      userId: 'user-456',
      username: 'commenter',
      displayName: 'Comment User',
      userAvatarUrl: 'https://api.dicebear.com/7.x/avataaars/svg?seed=user-456',
      content: 'Great track!',
      likeCount: 0,
      replyCount: 0,
      isLiked: false,
      isOwnComment: false,
      parentId: undefined,
      createdAt: new Date(),
      updatedAt: new Date(),
      ...overrides,
    }
  }

  static createUser(overrides = {}) {
    return {
      id: 'user-123',
      username: 'testuser',
      displayName: 'Test User',
      email: 'test@example.com',
      profilePictureUrl: 'https://api.dicebear.com/7.x/avataaars/svg?seed=user-123',
      bio: 'Test bio',
      followerCount: 100,
      followingCount: 50,
      postCount: 10,
      isFollowing: false,
      isOwnProfile: false,
      createdAt: new Date(),
      ...overrides,
    }
  }

  static createNotification(overrides = {}) {
    return {
      id: 'notif-123',
      type: 'post_liked',
      userId: 'user-123',
      username: 'testuser',
      displayName: 'Test User',
      userAvatarUrl: 'https://api.dicebear.com/7.x/avataaars/svg?seed=user-123',
      targetId: 'post-123',
      targetType: 'post',
      isRead: false,
      createdAt: new Date(),
      ...overrides,
    }
  }
}

/**
 * Wait for async operations to complete
 */
export async function waitForAsync() {
  return new Promise((resolve) => setTimeout(resolve, 0))
}

/**
 * Create a resolved promise for tests
 */
export function resolvedPromise<T>(value: T) {
  return Promise.resolve(value)
}

/**
 * Create a rejected promise for tests
 */
export function rejectedPromise(reason: Error) {
  return Promise.reject(reason)
}
