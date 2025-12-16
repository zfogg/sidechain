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
      const result = await FeedClient.toggleLike(postId, shouldLike)
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onMutate: async ({ postId, shouldLike }) => {
      // Cancel any outgoing refetches to prevent overwriting optimistic update
      await queryClient.cancelQueries({ queryKey: ['feed'] })

      // Snapshot the previous data
      const previousFeeds = queryClient.getQueriesData<any>({ queryKey: ['feed'] })

      // Optimistic update: Update both React Query cache and Zustand store
      queryClient.setQueriesData({ queryKey: ['feed'] }, (old: any) => {
        if (!old?.pages) return old

        return {
          ...old,
          pages: old.pages.map((page: FeedPost[]) =>
            page.map((post) =>
              post.id === postId
                ? {
                    ...post,
                    isLiked: shouldLike,
                    likeCount: post.likeCount + (shouldLike ? 1 : -1),
                  }
                : post
            )
          ),
        }
      })

      // Also update Zustand store for consistency
      feedStore.updatePost(postId, {
        isLiked: shouldLike,
        likeCount:
          (feedStore.feeds[feedStore.currentFeedType]?.posts.find((p) => p.id === postId)?.likeCount || 0) +
          (shouldLike ? 1 : -1),
      })

      return { previousFeeds }
    },
    onError: (_err, _variables, context: any) => {
      // Rollback React Query cache on error
      if (context?.previousFeeds) {
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
      await queryClient.cancelQueries({ queryKey: ['feed'] })

      const previousFeeds = queryClient.getQueriesData<any>({ queryKey: ['feed'] })

      queryClient.setQueriesData({ queryKey: ['feed'] }, (old: any) => {
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
    mutationFn: async (postId: string) => {
      const result = await FeedClient.trackPlay(postId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onMutate: (postId) => {
      // Optimistic update: increment play count immediately
      feedStore.incrementPlayCount(postId)
    },
  })
}
