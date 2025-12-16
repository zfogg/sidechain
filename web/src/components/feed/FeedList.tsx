import { useEffect, useRef, useCallback } from 'react'
import { useFeedQuery } from '@/hooks/queries/useFeedQuery'
import { PostCard } from './PostCard'
import { Spinner } from '@/components/ui/spinner'
import { Button } from '@/components/ui/button'
import type { FeedType } from '@/api/FeedClient'

interface FeedListProps {
  feedType: FeedType
  onPostCommentClick?: () => void
}

/**
 * FeedList - Infinite scroll feed with pagination
 *
 * Features:
 * - Infinite scroll pagination using React Query
 * - Automatic loading of next page when scrolling to bottom
 * - Skeleton loading states
 * - Error handling with retry
 * - Real-time updates via WebSocket
 */
export function FeedList({ feedType }: FeedListProps) {
  const {
    data,
    isLoading,
    isFetchingNextPage,
    hasNextPage,
    fetchNextPage,
    error,
    refetch,
  } = useFeedQuery(feedType)

  const loaderRef = useRef<HTMLDivElement>(null)

  // Intersection Observer for infinite scroll
  useEffect(() => {
    const observer = new IntersectionObserver(
      (entries) => {
        if (entries[0].isIntersecting && hasNextPage && !isFetchingNextPage) {
          fetchNextPage()
        }
      },
      { threshold: 0.1 }
    )

    if (loaderRef.current) {
      observer.observe(loaderRef.current)
    }

    return () => {
      observer.disconnect()
    }
  }, [hasNextPage, isFetchingNextPage, fetchNextPage])

  const posts = data?.pages.flat() || []

  if (isLoading) {
    return (
      <div className="space-y-4">
        {/* Skeleton loaders */}
        {Array.from({ length: 3 }).map((_, i) => (
          <div
            key={i}
            className="bg-card border border-border rounded-lg overflow-hidden animate-pulse"
          >
            <div className="p-4 flex gap-3">
              <div className="w-10 h-10 rounded-full bg-muted" />
              <div className="flex-1">
                <div className="h-4 bg-muted rounded w-32 mb-2" />
                <div className="h-3 bg-muted rounded w-24" />
              </div>
            </div>
            <div className="p-4 space-y-3">
              <div className="h-4 bg-muted rounded w-3/4" />
              <div className="h-2 bg-muted rounded" />
              <div className="h-8 bg-muted rounded" />
            </div>
          </div>
        ))}
      </div>
    )
  }

  if (error) {
    return (
      <div className="bg-card border border-border rounded-lg p-6 text-center space-y-4">
        <div className="text-destructive font-medium">Failed to load feed</div>
        <div className="text-sm text-muted-foreground">
          {error instanceof Error ? error.message : 'An error occurred'}
        </div>
        <Button onClick={() => refetch()}>Retry</Button>
      </div>
    )
  }

  if (posts.length === 0) {
    return (
      <div className="bg-card border border-border rounded-lg p-12 text-center space-y-4">
        <div className="text-muted-foreground text-lg">No posts yet</div>
        <div className="text-sm text-muted-foreground">
          Follow creators or check out trending sounds to get started
        </div>
      </div>
    )
  }

  return (
    <div className="space-y-4">
      {/* Posts */}
      {posts.map((post) => (
        <PostCard key={post.id} post={post} />
      ))}

      {/* Loading indicator for next page */}
      {isFetchingNextPage && (
        <div className="flex justify-center py-8">
          <Spinner size="md" />
        </div>
      )}

      {/* Intersection observer trigger */}
      <div ref={loaderRef} className="py-4" />

      {/* End of feed message */}
      {!hasNextPage && posts.length > 0 && (
        <div className="text-center py-8 text-muted-foreground">
          You've reached the end of the feed
        </div>
      )}
    </div>
  )
}
