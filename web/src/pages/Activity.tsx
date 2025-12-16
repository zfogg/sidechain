import { useState } from 'react'
import { useInfiniteQuery } from '@tanstack/react-query'
import { StoryClient } from '@/api/StoryClient'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { StoryCard } from '@/components/stories/StoryCard'
import type { Ephemeral } from '@/models/Ephemeral'

type ActivityFilter = 'following' | 'global'

function getPagesFromData(data: unknown): Ephemeral[] {
  if (data === null || typeof data !== 'object') return []
  if (!('pages' in data)) return []
  const pages = (data as { pages: unknown }).pages
  if (!Array.isArray(pages)) return []
  return pages.flatMap((page) => (Array.isArray(page) ? page : []))
}

/**
 * Activity - Display user activity timeline from followed users
 * Shows posts, likes, comments, follows, and other user actions
 */
export function Activity() {
  const [filter, setFilter] = useState<ActivityFilter>('following')

  // Fetch activity timeline
  const query = useInfiniteQuery<Ephemeral[], Error>({
    queryKey: ['activity', filter],
    queryFn: async ({ pageParam }) => {
      const offset = typeof pageParam === 'number' ? pageParam : 0
      const result =
        filter === 'following'
          ? await StoryClient.getFollowingActivityTimeline(20, offset)
          : await StoryClient.getGlobalActivityTimeline(20, offset)

      if (result.isError()) {
        throw new Error(result.getError())
      }

      return result.getValue()
    },
    getNextPageParam: (lastPage) => {
      return lastPage.length === 20 ? (lastPage.length > 0 ? true : undefined) : undefined
    },
    initialPageParam: 0,
    staleTime: 2 * 60 * 1000, // 2 minutes
  })

  const {
    data,
    isLoading,
    isError,
    error,
    hasNextPage,
    fetchNextPage,
    isFetchingNextPage,
  } = query

  const allStories: Ephemeral[] = getPagesFromData(data)

  return (
    <div className="flex flex-col min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Header */}
      <div className="border-b border-border" style={{ backgroundColor: '#26262C' }}>
        <div className="max-w-4xl mx-auto px-4 py-4">
          <div className="flex items-center justify-between mb-4">
            <h1 className="text-2xl font-bold text-foreground">Activity</h1>
            <div className="text-muted-foreground text-sm">
              {allStories.length} activities
            </div>
          </div>

          {/* Filter Buttons */}
          <div className="flex gap-2">
            <Button
              variant={filter === 'following' ? 'default' : 'outline'}
              onClick={() => setFilter('following')}
              className="gap-2"
            >
              👥 Following
            </Button>
            <Button
              variant={filter === 'global' ? 'default' : 'outline'}
              onClick={() => setFilter('global')}
              className="gap-2"
            >
              🌍 Global
            </Button>
          </div>
        </div>
      </div>

      {/* Content */}
      <main className="flex-1 max-w-4xl mx-auto w-full px-4 py-8">
        {isLoading && (
          <div className="flex items-center justify-center min-h-96">
            <Spinner size="lg" />
          </div>
        )}

        {isError && (
          <div className="bg-red-500/10 border border-red-500/20 rounded-lg p-4 text-center">
            <div className="text-red-400 font-semibold">Failed to load activity</div>
            <div className="text-red-300 text-sm">
              {error instanceof Error ? error.message : 'Unknown error'}
            </div>
          </div>
        )}

        {!isLoading && allStories.length === 0 && (
          <div className="bg-card border border-border rounded-lg p-12 text-center">
            <div className="text-3xl mb-3">📭</div>
            <div className="text-foreground font-semibold mb-2">No activity yet</div>
            <div className="text-muted-foreground">
              {filter === 'following'
                ? 'Follow some users to see their activity here'
                : 'Check back later to see what the community is up to!'}
            </div>
          </div>
        )}

        {/* Stories Timeline */}
        {allStories.length > 0 && (
          <div className="space-y-4">
            {allStories.map((story: Ephemeral) => (
              <StoryCard key={story.id} story={story} />
            ))}
          </div>
        )}

        {/* Load More Button */}
        {hasNextPage && (
          <div className="flex justify-center mt-8">
            <Button
              onClick={() => fetchNextPage()}
              disabled={isFetchingNextPage}
              variant="outline"
              className="px-6"
            >
              {isFetchingNextPage ? (
                <>
                  <Spinner size="sm" className="mr-2" />
                  Loading...
                </>
              ) : (
                'Load More'
              )}
            </Button>
          </div>
        )}
      </main>
    </div>
  )
}
