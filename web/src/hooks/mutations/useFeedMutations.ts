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

      // Debug: Log all queries in cache
      const allQueries = queryClient.getQueriesData<any>({})
      console.log('[Like] All queries in cache:', allQueries.map(([key]) => key))

      // Cancel any outgoing refetches to prevent overwriting optimistic update
      await queryClient.cancelQueries({ queryKey: ['feed'], exact: false })
      await queryClient.cancelQueries({ queryKey: ['gorse-recommendations'], exact: false })
      await queryClient.cancelQueries({ queryKey: ['post'], exact: false })
      await queryClient.cancelQueries({ queryKey: ['user'], exact: false })

      // Snapshot the previous data for all feed queries
      const previousFeeds = queryClient.getQueriesData<any>({})
      console.log('[Like] Snapshotted all queries:', previousFeeds.length)

      // Optimistic update: Update ANY query that contains this post
      let foundAndUpdated = false
      queryClient.setQueriesData({}, (old: any) => {
        // Handle paginated feed queries (pages: FeedPost[][])
        if (old?.pages && Array.isArray(old.pages)) {
          const updated = {
            ...old,
            pages: old.pages.map((page: FeedPost[]) =>
              page.map((post) => {
                if (post.id === postId) {
                  console.log('[Like] Found post in paginated query, updating from isLiked:', post.isLiked)
                  foundAndUpdated = true
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
          return updated
        }

        // Handle flat arrays (gorse-recommendations, gorse-genres, user posts, etc.)
        if (Array.isArray(old)) {
          const updated = old.map((item: any) => {
            // Handle wrapped posts { post: FeedPost }
            if (item?.post?.id === postId) {
              console.log('[Like] Found post in recommendation, updating from isLiked:', item.post.isLiked)
              foundAndUpdated = true
              return {
                ...item,
                post: {
                  ...item.post,
                  isLiked: shouldLike,
                  likeCount: item.post.likeCount + (shouldLike ? 1 : -1),
                },
              }
            }
            // Handle direct posts
            if (item?.id === postId) {
              console.log('[Like] Found direct post in array, updating from isLiked:', item.isLiked)
              foundAndUpdated = true
              return {
                ...item,
                isLiked: shouldLike,
                likeCount: item.likeCount + (shouldLike ? 1 : -1),
              }
            }
            return item
          })
          return updated
        }

        // Handle single post objects
        if (old?.id === postId) {
          console.log('[Like] Found single post object, updating from isLiked:', old.isLiked)
          foundAndUpdated = true
          return {
            ...old,
            isLiked: shouldLike,
            likeCount: old.likeCount + (shouldLike ? 1 : -1),
          }
        }

        return old
      })
      console.log('[Like] Updated cache, found and updated post:', foundAndUpdated)

      // Also update Zustand store for consistency
      feedStore.updatePost(postId, {
        isLiked: shouldLike,
        likeCount:
          (feedStore.feeds[feedStore.currentFeedType]?.posts.find((p) => p.id === postId)?.likeCount || 0) +
          (shouldLike ? 1 : -1),
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
      console.log('[Save] Starting mutation:', { postId, shouldSave })
      const result = await FeedClient.toggleSave(postId, shouldSave)
      console.log('[Save] API response:', { result: result.isOk() ? 'success' : 'error', error: result.isError() ? result.getError() : null })
      if (result.isError()) {
        console.error('[Save] Error from API:', result.getError())
        throw new Error(result.getError())
      }
      console.log('[Save] Mutation succeeded')
    },
    onMutate: async ({ postId, shouldSave }) => {
      console.log('[Save] onMutate starting:', { postId, shouldSave })

      // Debug: Log all queries in cache
      const allQueries = queryClient.getQueriesData<any>({})
      console.log('[Save] All queries in cache:', allQueries.map(([key]) => key))

      // Cancel any outgoing refetches
      await queryClient.cancelQueries({})

      // Snapshot all previous data
      const previousFeeds = queryClient.getQueriesData<any>({})
      console.log('[Save] Snapshotted all queries:', previousFeeds.length)

      // Optimistic update: Update ANY query that contains this post
      let foundAndUpdated = false
      queryClient.setQueriesData({}, (old: any) => {
        // Handle paginated feed queries (pages: FeedPost[][])
        if (old?.pages && Array.isArray(old.pages)) {
          const updated = {
            ...old,
            pages: old.pages.map((page: FeedPost[]) =>
              page.map((post) => {
                if (post.id === postId) {
                  console.log('[Save] Found post in paginated query, updating from isSaved:', post.isSaved)
                  foundAndUpdated = true
                  return {
                    ...post,
                    isSaved: shouldSave,
                    saveCount: post.saveCount + (shouldSave ? 1 : -1),
                  }
                }
                return post
              })
            ),
          }
          return updated
        }

        // Handle flat arrays (gorse-recommendations, gorse-genres, user posts, etc.)
        if (Array.isArray(old)) {
          const updated = old.map((item: any) => {
            // Handle wrapped posts { post: FeedPost }
            if (item?.post?.id === postId) {
              console.log('[Save] Found post in recommendation, updating from isSaved:', item.post.isSaved)
              foundAndUpdated = true
              return {
                ...item,
                post: {
                  ...item.post,
                  isSaved: shouldSave,
                  saveCount: item.post.saveCount + (shouldSave ? 1 : -1),
                },
              }
            }
            // Handle direct posts
            if (item?.id === postId) {
              console.log('[Save] Found direct post in array, updating from isSaved:', item.isSaved)
              foundAndUpdated = true
              return {
                ...item,
                isSaved: shouldSave,
                saveCount: item.saveCount + (shouldSave ? 1 : -1),
              }
            }
            return item
          })
          return updated
        }

        // Handle single post objects
        if (old?.id === postId) {
          console.log('[Save] Found single post object, updating from isSaved:', old.isSaved)
          foundAndUpdated = true
          return {
            ...old,
            isSaved: shouldSave,
            saveCount: old.saveCount + (shouldSave ? 1 : -1),
          }
        }

        return old
      })
      console.log('[Save] Updated cache, found and updated post:', foundAndUpdated)

      feedStore.updatePost(postId, {
        isSaved: shouldSave,
        saveCount:
          (feedStore.feeds[feedStore.currentFeedType]?.posts.find((p) => p.id === postId)?.saveCount || 0) +
          (shouldSave ? 1 : -1),
      })

      return { previousFeeds }
    },
    onError: (err, _variables, context: any) => {
      console.error('[Save] Mutation failed:', err)
      if (context?.previousFeeds) {
        console.log('[Save] Rolling back', context.previousFeeds.length, 'queries')
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
