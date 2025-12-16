import { FeedClient } from '@/api/FeedClient'
import { PostCard } from '@/components/feed/PostCard'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import type { FeedPost } from '@/models/FeedPost'
import { useInfiniteQuery } from '@tanstack/react-query'
import { useState } from 'react'

type TimePeriod = 'weekly' | 'monthly' | 'alltime'

function getPagesFromData(data: unknown): FeedPost[] {
  if (data === null || typeof data !== 'object') return []
  if (!('pages' in data)) return []
  const pages = (data as { pages: unknown }).pages
  if (!Array.isArray(pages)) return []
  return pages.flatMap((page) => (Array.isArray(page) ? page : []))
}

/**
 * Trending - Display trending loops/posts with time period filter
 * Shows the most popular posts by engagement (likes, plays, comments)
 */
export function Trending() {
  const [timePeriod, setTimePeriod] = useState<TimePeriod>('weekly')

  // Fetch trending posts with infinite scroll
  const query = useInfiniteQuery<FeedPost[], Error>({
    queryKey: ['trending', timePeriod],
    queryFn: async ({ pageParam }) => {
      // Backend supports time period as query param
      const offset = typeof pageParam === 'number' ? pageParam : 0
      const result = await FeedClient.getFeed('trending', 20, offset)

      if (result.isError()) {
        throw new Error(result.getError())
      }

      return result.getValue()
    },
    getNextPageParam: (lastPage) => {
      return lastPage.length === 20 ? (lastPage.length > 0 ? true : undefined) : undefined
    },
    initialPageParam: 0,
    staleTime: 5 * 60 * 1000, // 5 minutes
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

  const allPosts: FeedPost[] = getPagesFromData(data)

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Header */}
      <div
        className="border-b border-border sticky top-16 z-40"
        style={{ backgroundColor: '#26262C' }}
      >
        <div className="max-w-4xl mx-auto px-4 py-6">
          <div className="flex items-center justify-between mb-4">
            <h1 className="text-3xl font-bold text-foreground">Trending</h1>
            <div className="text-muted-foreground text-sm">
              {allPosts.length} posts
            </div>
          </div>

          {/* Time Period Filter */}
          <div className="flex gap-2">
            <Button
              variant={timePeriod === 'weekly' ? 'default' : 'outline'}
              onClick={() => setTimePeriod('weekly')}
              className="gap-2"
            >
              📅 This Week
            </Button>
            <Button
              variant={timePeriod === 'monthly' ? 'default' : 'outline'}
              onClick={() => setTimePeriod('monthly')}
              className="gap-2"
            >
              📊 This Month
            </Button>
            <Button
              variant={timePeriod === 'alltime' ? 'default' : 'outline'}
              onClick={() => setTimePeriod('alltime')}
              className="gap-2"
            >
              🏆 All Time
            </Button>
          </div>
        </div>
      </div>

      {/* Content */}
      <main className="max-w-4xl mx-auto px-4 py-8">
        {isLoading && (
          <div className="flex items-center justify-center min-h-96">
            <Spinner size="lg" />
          </div>
        )}

        {isError && (
          <div className="bg-red-500/10 border border-red-500/20 rounded-lg p-4 text-center">
            <div className="text-red-400 font-semibold">Failed to load trending posts</div>
            <div className="text-red-300 text-sm">
              {error instanceof Error ? error.message : 'Unknown error'}
            </div>
          </div>
        )}

        {!isLoading && allPosts.length === 0 && (
          <div className="bg-card border border-border rounded-lg p-12 text-center">
            <div className="text-3xl mb-3">📈</div>
            <div className="text-foreground font-semibold mb-2">No trending posts yet</div>
            <div className="text-muted-foreground">
              Check back later to see what's trending!
            </div>
          </div>
        )}

        {/* Posts List */}
        {allPosts.length > 0 && (
          <div className="space-y-6">
            {allPosts.map((post: FeedPost, index: number) => (
              <div key={post.id} className="relative">
                {/* Rank badge */}
                <div className="absolute -left-12 top-0 hidden xl:flex items-center justify-center">
                  <div className="bg-gradient-to-r from-coral-pink to-rose-pink rounded-full w-10 h-10 flex items-center justify-center text-white font-bold text-sm">
                    #{index + 1}
                  </div>
                </div>

                <PostCard post={post} />
              </div>
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
