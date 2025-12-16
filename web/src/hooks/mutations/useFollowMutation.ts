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
    onMutate: async ({ shouldFollow }) => {
      // Cancel outgoing queries
      await queryClient.cancelQueries({ queryKey: ['profile'] })

      // Snapshot previous state for rollback
      const previousProfiles = queryClient.getQueriesData({ queryKey: ['profile'] })

      // Update all profile queries optimistically
      queryClient.setQueriesData(
        { queryKey: ['profile'] },
        (old: any) => {
          if (!old) return old
          return {
            ...old,
            isFollowing: shouldFollow,
            followerCount: old.followerCount + (shouldFollow ? 1 : -1),
          }
        }
      )

      return { previousProfiles }
    },
    onError: (_err, _variables, context) => {
      // Rollback on error
      if (context?.previousProfiles) {
        context.previousProfiles.forEach(([queryKey, data]) => {
          queryClient.setQueryData(queryKey, data)
        })
      }
    },
    onSuccess: () => {
      // Invalidate related queries
      queryClient.invalidateQueries({ queryKey: ['profile'] })
    },
  })
}
