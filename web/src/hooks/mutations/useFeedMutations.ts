import { useMutation, useQueryClient } from '@tanstack/react-query'
import { FeedClient } from '@/api/FeedClient'
import type { FeedPost } from '@/models/FeedPost'
import { useFeedStore } from '@/stores/useFeedStore'

/**
 * useLikeMutation - Mutation for liking/unliking a post with optimistic updates
 * Mirrors C++ plugin's optimistic update pattern with automatic rollback on error
 *
 * Usage:
 * ```tsx
 * const { mutate: toggleLike, isPending } = useLikeMutation()
 * const handleLike = () => toggleLike({ postId: 'post-123', shouldLike: true })
 * ```
 */
export function useLikeMutation() {
  const queryClient = useQueryClient()
  const feedStore = useFeedStore()

  return useMutation({
    mutationFn: async ({ postId, shouldLike }: { postId: string; shouldLike: boolean }) => {
      console.log('[Like] Starting mutation:', { postId, shouldLike })
      const result = await FeedClient.toggleLike(postId, shouldLike)
      console.log('[Like] API response:', { result: result.isOk() ? 'success' : 'error', error: result.isError() ? result.getError() : null })
      if (result.isError()) {
        console.error('[Like] Error from API:', result.getError())
        throw new Error(result.getError())
      }
      console.log('[Like] Mutation succeeded')
    },
    onMutate: async ({ postId, shouldLike }) => {
      console.log('[Like] onMutate starting:', { postId, shouldLike })

      // Cancel any outgoing refetches to prevent overwriting optimistic update
      await queryClient.cancelQueries({ queryKey: ['feed'], exact: false })

      // Snapshot the previous data for all feed queries
      const previousFeeds = queryClient.getQueriesData<any>({ queryKey: ['feed'], exact: false })
      console.log('[Like] Found queries to update:', previousFeeds.length)

      // Optimistic update: Update all feed queries (timeline, global, trending, etc.)
      queryClient.setQueriesData({ queryKey: ['feed'], exact: false }, (old: any) => {
        console.log('[Like] Updating cache for key:', old, 'hasPages:', !!old?.pages)
        if (!old?.pages) return old

        const updated = {
          ...old,
          pages: old.pages.map((page: FeedPost[]) =>
            page.map((post) => {
              if (post.id === postId) {
                console.log('[Like] Found post to update, changing isLiked from', post.isLiked, 'to', shouldLike)
                return {
                  ...post,
                  isLiked: shouldLike,
                  likeCount: post.likeCount + (shouldLike ? 1 : -1),
                }
              }
              return post
            })
          ),
        }
        console.log('[Like] Returning updated cache')
        return updated
      })

      // Also update Zustand store for consistency
      feedStore.updatePost(postId, {
        isLiked: shouldLike,
        likeCount:
          (feedStore.feeds[feedStore.currentFeedType]?.posts.find((p) => p.id === postId)?.likeCount || 0) +
          (shouldLike ? 1 : -1),
      })

      // Update gorse-recommendations query as well (for "For You" tab)
      queryClient.setQueriesData({ queryKey: ['gorse-recommendations'], exact: false }, (old: any) => {
        if (!Array.isArray(old)) return old

        return old.map((rec: any) =>
          rec.post?.id === postId
            ? {
                ...rec,
                post: {
                  ...rec.post,
                  isLiked: shouldLike,
                  likeCount: rec.post.likeCount + (shouldLike ? 1 : -1),
                },
              }
            : rec
        )
      })

      // Update genre-recommendations query
      queryClient.setQueriesData({ queryKey: ['gorse-genres'], exact: false }, (old: any) => {
        if (!Array.isArray(old)) return old

        return old.map((post: FeedPost) =>
          post.id === postId
            ? {
                ...post,
                isLiked: shouldLike,
                likeCount: post.likeCount + (shouldLike ? 1 : -1),
              }
            : post
        )
      })

      return { previousFeeds }
    },
    onError: (err, _variables, context: any) => {
      console.error('[Like] Mutation failed:', err)
      // Rollback React Query cache on error
      if (context?.previousFeeds) {
        console.log('[Like] Rolling back', context.previousFeeds.length, 'queries')
        context.previousFeeds.forEach(([queryKey, data]: [any, any]) => {
          queryClient.setQueryData(queryKey, data)
        })
      }

      // Zustand store rollback is automatic through queryClient rollback
    },
    onSuccess: () => {
      // Optionally invalidate queries to ensure freshness
      // queryClient.invalidateQueries({ queryKey: ['feed'] })
    },
  })
}

/**
 * useSaveMutation - Mutation for saving/unsaving a post
 */
export function useSaveMutation() {
  const queryClient = useQueryClient()
  const feedStore = useFeedStore()

  return useMutation({
    mutationFn: async ({ postId, shouldSave }: { postId: string; shouldSave: boolean }) => {
      const result = await FeedClient.toggleSave(postId, shouldSave)
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onMutate: async ({ postId, shouldSave }) => {
      await queryClient.cancelQueries({ queryKey: ['feed'], exact: false })

      const previousFeeds = queryClient.getQueriesData<any>({ queryKey: ['feed'], exact: false })

      queryClient.setQueriesData({ queryKey: ['feed'], exact: false }, (old: any) => {
        if (!old?.pages) return old

        return {
          ...old,
          pages: old.pages.map((page: FeedPost[]) =>
            page.map((post) =>
              post.id === postId
                ? {
                    ...post,
                    isSaved: shouldSave,
                    saveCount: post.saveCount + (shouldSave ? 1 : -1),
                  }
                : post
            )
          ),
        }
      })

      // Update gorse-recommendations query as well (for "For You" tab)
      queryClient.setQueriesData({ queryKey: ['gorse-recommendations'], exact: false }, (old: any) => {
        if (!Array.isArray(old)) return old

        return old.map((rec: any) =>
          rec.post?.id === postId
            ? {
                ...rec,
                post: {
                  ...rec.post,
                  isSaved: shouldSave,
                  saveCount: rec.post.saveCount + (shouldSave ? 1 : -1),
                },
              }
            : rec
        )
      })

      // Update genre-recommendations query
      queryClient.setQueriesData({ queryKey: ['gorse-genres'], exact: false }, (old: any) => {
        if (!Array.isArray(old)) return old

        return old.map((post: FeedPost) =>
          post.id === postId
            ? {
                ...post,
                isSaved: shouldSave,
                saveCount: post.saveCount + (shouldSave ? 1 : -1),
              }
            : post
        )
      })

      feedStore.updatePost(postId, {
        isSaved: shouldSave,
        saveCount:
          (feedStore.feeds[feedStore.currentFeedType]?.posts.find((p) => p.id === postId)?.saveCount || 0) +
          (shouldSave ? 1 : -1),
      })

      return { previousFeeds }
    },
    onError: (_err, _variables, context: any) => {
      if (context?.previousFeeds) {
        context.previousFeeds.forEach(([queryKey, data]: [any, any]) => {
          queryClient.setQueryData(queryKey, data)
        })
      }
    },
  })
}

/**
 * useReactMutation - Mutation for adding emoji reactions to posts
 */
export function useReactMutation() {
  const queryClient = useQueryClient()

  return useMutation({
    mutationFn: async ({ postId, emoji }: { postId: string; emoji: string }) => {
      const result = await FeedClient.react(postId, emoji)
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['feed'] })
    },
  })
}

/**
 * usePlayTrackMutation - Mutation for tracking play events
 */
export function usePlayTrackMutation() {
  const feedStore = useFeedStore()

  return useMutation({
    mutationFn: async ({ postId, duration = 0 }: { postId: string; duration?: number }) => {
      const result = await FeedClient.trackPlay(postId, duration)
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onMutate: ({ postId }) => {
      // Optimistic update: increment play count immediately
      feedStore.incrementPlayCount(postId)
    },
  })
}
