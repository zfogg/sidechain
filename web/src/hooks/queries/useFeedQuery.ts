import { useInfiniteQuery } from '@tanstack/react-query'
import { FeedClient, type FeedType } from '@/api/FeedClient'

/**
 * useFeedQuery - Infinite query hook for paginated feed loading
 * Mirrors C++ FeedStore pagination pattern with React Query's infinite scroll support
 *
 * Usage:
 * ```tsx
 * const { data, fetchNextPage, hasNextPage, isFetchingNextPage } = useFeedQuery('timeline')
 * ```
 */
export function useFeedQuery(feedType: FeedType, limit = 20) {
  return useInfiniteQuery({
    queryKey: ['feed', feedType],
    queryFn: async ({ pageParam = 0 }) => {
      const result = await FeedClient.getFeed(feedType, limit, pageParam)
      if (result.isOk()) {
        return result.getValue()
      }
      throw new Error(result.getError())
    },
    getNextPageParam: (lastPage, allPages) => {
      // If last page returned fewer items than limit, no more pages
      if (lastPage.length < limit) {
        return undefined
      }
      // Next page starts at current total count
      const totalFetched = allPages.flat().length
      return totalFetched
    },
    // Keep data fresh while user is actively scrolling
    staleTime: 60 * 1000, // 1 minute
    // Keep in memory for 5 minutes after last access
    gcTime: 5 * 60 * 1000,
    // Retry failed requests once
    retry: 1,
  })
}

/**
 * useFeedQuery with custom limit
 */
export function useFeedQueryWithLimit(feedType: FeedType, limit: number) {
  return useFeedQuery(feedType, limit)
}
