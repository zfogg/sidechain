import { useMutation, useQueryClient } from '@tanstack/react-query'
import { UserClient } from '@/api/UserClient'

interface FollowData {
  userId: string
  shouldFollow: boolean
}

/**
 * useFollowMutation - Mutation hook for following/unfollowing users
 *
 * Features:
 * - Optimistic updates for instant UI feedback
 * - Automatic cache invalidation
 * - Error rollback
 */
export function useFollowMutation() {
  const queryClient = useQueryClient()

  return useMutation({
    mutationFn: async ({ userId, shouldFollow }: FollowData) => {
      if (shouldFollow) {
        const result = await UserClient.followUser(userId)
        if (result.isError()) {
          throw new Error(result.getError())
        }
      } else {
        const result = await UserClient.unfollowUser(userId)
        if (result.isError()) {
          throw new Error(result.getError())
        }
      }
    },
    onMutate: async ({ userId, shouldFollow }) => {
      console.log('[Follow] Starting mutation:', { userId, shouldFollow })
      
      // Don't cancel queries - this can cause React Query to queue refetches
      // that overwrite our optimistic updates. Instead, we'll update optimistically
      // and let the queries naturally refetch when they become stale or on navigation.
      
      // Snapshot ALL previous state for rollback (including profile queries)
      const previousAllQueries = queryClient.getQueriesData({})
      console.log('[Follow] Found', previousAllQueries.length, 'queries in cache')

      // Update ALL queries that might contain this user or posts by this user
      // This matches the pattern used by useLikeMutation and useSaveMutation
      let updatedCount = 0
      queryClient.setQueriesData({}, (old: any) => {
        if (!old) return old

        // Handle infinite query structure (useInfiniteQuery) - { pages: [[...], [...]], pageParams: [...] }
        if (old.pages && Array.isArray(old.pages)) {
          let hasChanges = false
          const updatedPages = old.pages.map((page: any[]) => {
            if (!Array.isArray(page)) return page
            const updatedPage = page.map((item: any) => {
              // Update user objects (profile queries, user lists)
              if (item.id === userId) {
                hasChanges = true
                return {
                  ...item,
                  isFollowing: shouldFollow,
                  followerCount: item.followerCount !== undefined
                    ? item.followerCount + (shouldFollow ? 1 : -1)
                    : item.followerCount,
                }
              }
              // Update posts by this user
              if (item.userId === userId) {
                hasChanges = true
                console.log('[Follow] Updating post in feed:', item.id, 'old.isFollowing:', item.isFollowing, 'new:', shouldFollow)
                return { ...item, isFollowing: shouldFollow }
              }
              // Handle wrapped posts in recommendations { post: FeedPost }
              if (item.post && item.post.userId === userId) {
                hasChanges = true
                return {
                  ...item,
                  post: {
                    ...item.post,
                    isFollowing: shouldFollow,
                  },
                }
              }
              return item
            })
            return updatedPage
          })
          
          if (hasChanges) {
            updatedCount++
            console.log('[Follow] Updated infinite query with pages structure')
            return {
              ...old,
              pages: updatedPages,
            }
          }
          return old
        }

        // Handle flat arrays (gorse-producers, search results, etc.)
        if (Array.isArray(old)) {
          let hasChanges = false
          const updated = old.map((item: any) => {
            // Update user objects
            if (item.id === userId) {
              hasChanges = true
              return {
                ...item,
                isFollowing: shouldFollow,
                followerCount: item.followerCount !== undefined
                  ? item.followerCount + (shouldFollow ? 1 : -1)
                  : item.followerCount,
              }
            }
            // Update posts by this user
            if (item.userId === userId) {
              hasChanges = true
              return { ...item, isFollowing: shouldFollow }
            }
            // Handle wrapped posts { post: FeedPost }
            if (item.post && item.post.userId === userId) {
              hasChanges = true
              return {
                ...item,
                post: {
                  ...item.post,
                  isFollowing: shouldFollow,
                },
              }
            }
            return item
          })
          
          if (hasChanges) {
            updatedCount++
            console.log('[Follow] Updated array query')
            return updated
          }
          return old
        }

        // Handle single user object (profile queries)
        // Profile queries use ['profile', username] and return { id, username, isFollowing, ... }
        if (old.id === userId) {
          updatedCount++
          console.log('[Follow] Updated profile query for user:', userId, 'old.isFollowing:', old.isFollowing, 'new:', shouldFollow)
          const updated = {
            ...old,
            isFollowing: shouldFollow,
            followerCount: old.followerCount !== undefined
              ? Math.max(0, old.followerCount + (shouldFollow ? 1 : -1))
              : old.followerCount,
          }
          console.log('[Follow] Profile query updated:', updated)
          return updated
        }

        // Handle objects with users array (e.g., { users: [...] })
        if (old.users && Array.isArray(old.users)) {
          let hasChanges = false
          const updatedUsers = old.users.map((user: any) => {
            if (user.id === userId) {
              hasChanges = true
              return {
                ...user,
                isFollowing: shouldFollow,
                followerCount: user.followerCount !== undefined
                  ? user.followerCount + (shouldFollow ? 1 : -1)
                  : user.followerCount,
              }
            }
            return user
          })
          
          if (hasChanges) {
            updatedCount++
            console.log('[Follow] Updated query with users array')
            return {
              ...old,
              users: updatedUsers,
            }
          }
          return old
        }

        return old
      })

      console.log('[Follow] Updated', updatedCount, 'queries optimistically')
      
      // Explicitly update profile queries (they use ['profile', username] as key)
      // This ensures profile header follow button updates
      // We do this separately because the main setQueriesData might not catch all profile queries
      const profileQueries = queryClient.getQueriesData({ queryKey: ['profile'] })
      console.log('[Follow] Found', profileQueries.length, 'profile queries')
      profileQueries.forEach(([queryKey, data]: [any, any]) => {
        if (data && data.id === userId) {
          console.log('[Follow] Explicitly updating profile query:', queryKey, data.username, 'old.isFollowing:', data.isFollowing, 'new:', shouldFollow)
          queryClient.setQueryData(queryKey, {
            ...data,
            isFollowing: shouldFollow,
            followerCount: data.followerCount !== undefined
              ? Math.max(0, data.followerCount + (shouldFollow ? 1 : -1))
              : data.followerCount,
          })
          console.log('[Follow] Profile query updated')
        }
      })
      
      // Also explicitly update userPosts queries (used in UserPostsSection on profile page)
      // These use ['userPosts', userId] as the key
      queryClient.setQueriesData(
        { queryKey: ['userPosts'] },
        (old: any) => {
          if (Array.isArray(old)) {
            let hasChanges = false
            const updated = old.map((post: any) => {
              if (post.userId === userId) {
                hasChanges = true
                console.log('[Follow] Updating post in userPosts query:', post.id)
                return { ...post, isFollowing: shouldFollow }
              }
              return post
            })
            if (hasChanges) {
              return updated
            }
          }
          return old
        }
      )
      
      // Mark feed queries as fresh to prevent immediate refetches that might overwrite
      // the optimistic update before Stream.io has propagated the follow relationship
      queryClient.setQueriesData({ queryKey: ['feed'] }, (old: any) => {
        // Just return the old data - this marks it as fresh without changing it
        return old
      })
      
      return { previousAllQueries }
    },
    onError: (err, variables, context) => {
      console.error('[Follow] Mutation failed:', err, variables)
      // Rollback ALL queries on error
      if (context?.previousAllQueries) {
        console.log('[Follow] Rolling back', context.previousAllQueries.length, 'queries due to error')
        context.previousAllQueries.forEach(([queryKey, data]) => {
          queryClient.setQueryData(queryKey, data)
        })
      }
    },
    onSuccess: () => {
      // Don't invalidate immediately - let optimistic updates persist
      // The backend cache is invalidated, so the next natural refetch will get fresh data
      // Immediate invalidation causes race conditions where Stream.io eventual consistency
      // might return stale data, overwriting the optimistic update
      // 
      // The optimistic update is the source of truth until:
      // 1. User refreshes the page (will get fresh data from backend)
      // 2. Query naturally becomes stale and refetches (after staleTime)
      // 3. User navigates away and back (will refetch)
      //
      // This prevents the UI from flickering back to "not following" immediately after following
    },
  })
}
