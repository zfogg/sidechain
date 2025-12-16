import { describe, it, expect, beforeEach, vi } from 'vitest'
import { renderHook, act } from '@testing-library/react'
import { useFeedStore } from '../useFeedStore'
import { FeedClient } from '@/api/FeedClient'
import { MockDataBuilder } from '@/test/utils'

// Mock the FeedClient
vi.mock('@/api/FeedClient')

describe('useFeedStore', () => {
  beforeEach(() => {
    // Clear store before each test
    useFeedStore.setState({
      feeds: {},
      currentFeedType: 'timeline',
    })
    vi.clearAllMocks()
  })

  describe('loadFeed', () => {
    it('should load feed data', async () => {
      const mockPost = MockDataBuilder.createFeedPost()
      vi.mocked(FeedClient.getFeed).mockResolvedValue([mockPost])

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      expect(result.current.feeds.timeline.posts).toHaveLength(1)
      expect(result.current.feeds.timeline.posts[0].id).toBe(mockPost.id)
    })

    it('should handle empty feed', async () => {
      vi.mocked(FeedClient.getFeed).mockResolvedValue([])

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('global')
      })

      expect(result.current.feeds.global.posts).toHaveLength(0)
    })

    it('should set loading state', async () => {
      const { result } = renderHook(() => useFeedStore())

      const loadPromise = act(async () => {
        await result.current.loadFeed('timeline')
      })

      // During loading, should be loading
      expect(result.current.feeds.timeline?.isLoading).toBe(true)

      await loadPromise

      // After loading, should not be loading
      expect(result.current.feeds.timeline.isLoading).toBe(false)
    })

    it('should handle API errors', async () => {
      const error = new Error('Network error')
      vi.mocked(FeedClient.getFeed).mockRejectedValue(error)

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      expect(result.current.feeds.timeline.error).toBeTruthy()
      expect(result.current.feeds.timeline.posts).toHaveLength(0)
    })

    it('should update lastUpdated timestamp', async () => {
      const before = Date.now()
      vi.mocked(FeedClient.getFeed).mockResolvedValue([])

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      const after = Date.now()
      expect(result.current.feeds.timeline.lastUpdated).toBeGreaterThanOrEqual(before)
      expect(result.current.feeds.timeline.lastUpdated).toBeLessThanOrEqual(after)
    })
  })

  describe('switchFeedType', () => {
    it('should switch current feed type', () => {
      const { result } = renderHook(() => useFeedStore())

      act(() => {
        result.current.switchFeedType('global')
      })

      expect(result.current.currentFeedType).toBe('global')
    })

    it('should load feed if not cached', async () => {
      const mockPost = MockDataBuilder.createFeedPost()
      vi.mocked(FeedClient.getFeed).mockResolvedValue([mockPost])

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        result.current.switchFeedType('trending')
      })

      expect(result.current.feeds.trending.posts).toHaveLength(1)
    })
  })

  describe('toggleLike', () => {
    it('should optimistically update like status', async () => {
      const mockPost = MockDataBuilder.createFeedPost({
        id: 'post-1',
        isLiked: false,
        likeCount: 5,
      })

      vi.mocked(FeedClient.getFeed).mockResolvedValue([mockPost])
      vi.mocked(FeedClient.toggleLike).mockResolvedValue(undefined)

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      await act(async () => {
        await result.current.toggleLike('post-1')
      })

      const updated = result.current.feeds.timeline.posts[0]
      expect(updated.isLiked).toBe(true)
      expect(updated.likeCount).toBe(6)
    })

    it('should rollback on error', async () => {
      const mockPost = MockDataBuilder.createFeedPost({
        id: 'post-1',
        isLiked: false,
        likeCount: 5,
      })

      vi.mocked(FeedClient.getFeed).mockResolvedValue([mockPost])
      vi.mocked(FeedClient.toggleLike).mockRejectedValue(new Error('API error'))

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      const originalPost = { ...result.current.feeds.timeline.posts[0] }

      await act(async () => {
        await result.current.toggleLike('post-1')
      })

      const post = result.current.feeds.timeline.posts[0]
      expect(post.isLiked).toBe(originalPost.isLiked)
      expect(post.likeCount).toBe(originalPost.likeCount)
    })
  })

  describe('loadMore', () => {
    it('should load additional posts', async () => {
      const page1 = [MockDataBuilder.createFeedPost({ id: 'post-1' })]
      const page2 = [MockDataBuilder.createFeedPost({ id: 'post-2' })]

      vi.mocked(FeedClient.getFeed).mockResolvedValueOnce(page1).mockResolvedValueOnce(page2)

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      expect(result.current.feeds.timeline.posts).toHaveLength(1)

      await act(async () => {
        await result.current.loadMore()
      })

      expect(result.current.feeds.timeline.posts).toHaveLength(2)
    })

    it('should update hasMore flag', async () => {
      const mockPost = MockDataBuilder.createFeedPost()
      vi.mocked(FeedClient.getFeed).mockResolvedValue([mockPost])

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      // When API returns less than limit, there are no more posts
      expect(result.current.feeds.timeline.hasMore).toBe(false)
    })

    it('should not load if already loading', async () => {
      vi.mocked(FeedClient.getFeed).mockImplementation(
        () =>
          new Promise((resolve) =>
            setTimeout(() => resolve([MockDataBuilder.createFeedPost()]), 100)
          )
      )

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        result.current.loadFeed('timeline')
        result.current.loadMore() // Call while loading
      })

      // Should not make second request
      expect(vi.mocked(FeedClient.getFeed).mock.calls).toHaveLength(1)
    })
  })

  describe('updatePost', () => {
    it('should update post in all feeds', async () => {
      const mockPost = MockDataBuilder.createFeedPost({ id: 'post-1', playCount: 0 })
      vi.mocked(FeedClient.getFeed).mockResolvedValue([mockPost])

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
        await result.current.loadFeed('global')
      })

      act(() => {
        result.current.updatePost('post-1', { playCount: 5 })
      })

      expect(result.current.feeds.timeline.posts[0].playCount).toBe(5)
      expect(result.current.feeds.global.posts[0].playCount).toBe(5)
    })
  })

  describe('incrementPlayCount', () => {
    it('should increment play count', async () => {
      const mockPost = MockDataBuilder.createFeedPost({ id: 'post-1', playCount: 10 })
      vi.mocked(FeedClient.getFeed).mockResolvedValue([mockPost])

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      act(() => {
        result.current.incrementPlayCount('post-1')
      })

      expect(result.current.feeds.timeline.posts[0].playCount).toBe(11)
    })

    it('should track play event on server', async () => {
      const mockPost = MockDataBuilder.createFeedPost({ id: 'post-1' })
      vi.mocked(FeedClient.getFeed).mockResolvedValue([mockPost])

      const { result } = renderHook(() => useFeedStore())

      await act(async () => {
        await result.current.loadFeed('timeline')
      })

      act(() => {
        result.current.incrementPlayCount('post-1')
      })

      // Should call trackPlay
      expect(vi.mocked(FeedClient.trackPlay)).toHaveBeenCalledWith('post-1')
    })
  })
})
