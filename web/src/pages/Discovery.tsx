import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { GorseClient } from '@/api/GorseClient'
import { PostCard } from '@/components/feed/PostCard'
import { TrendingProducerCard } from '@/components/discovery/TrendingProducerCard'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'

type DiscoveryTab = 'for-you' | 'trending' | 'producers' | 'genres'

/**
 * Discovery - Recommendation and discovery page powered by Gorse
 *
 * Features:
 * - Personalized recommendations based on listening history
 * - Trending posts and producers
 * - Genre-based discovery
 * - Similar posts based on current selection
 * - Real-time trending updates
 */
export function Discovery() {
  const [activeTab, setActiveTab] = useState<DiscoveryTab>('for-you')
  const [timeWindow, setTimeWindow] = useState<'day' | 'week' | 'month'>('week')

  // Fetch personalized recommendations
  const { data: recommendations, isLoading: isLoadingRecs } = useQuery({
    queryKey: ['gorse-recommendations'],
    queryFn: async () => {
      const result = await GorseClient.getRecommendations(50, 0)
      return result.isOk() ? result.getValue() : []
    },
    enabled: activeTab === 'for-you',
  })

  // Fetch trending posts
  const { data: trendingPosts, isLoading: isLoadingTrending } = useQuery({
    queryKey: ['gorse-trending', timeWindow],
    queryFn: async () => {
      const result = await GorseClient.getTrendingPosts(50, 0, timeWindow)
      return result.isOk() ? result.getValue() : []
    },
    enabled: activeTab === 'trending',
  })

  // Fetch trending producers
  const { data: trendingProducers, isLoading: isLoadingProducers } = useQuery({
    queryKey: ['gorse-producers'],
    queryFn: async () => {
      const result = await GorseClient.getTrendingProducers(30, 0)
      return result.isOk() ? result.getValue() : []
    },
    enabled: activeTab === 'producers',
  })

  // Fetch genre recommendations
  const { data: genreRecommendations, isLoading: isLoadingGenres } = useQuery({
    queryKey: ['gorse-genres'],
    queryFn: async () => {
      // Get recommendations for top 3 genres
      const genres = ['House', 'Techno', 'Ambient']
      const results = await Promise.all(
        genres.map((genre) => GorseClient.getGenreRecommendations(genre, 20, 0))
      )
      return results.flatMap((r) => (r.isOk() ? r.getValue() : []))
    },
    enabled: activeTab === 'genres',
  })

  const isLoading =
    (activeTab === 'for-you' && isLoadingRecs) ||
    (activeTab === 'trending' && isLoadingTrending) ||
    (activeTab === 'producers' && isLoadingProducers) ||
    (activeTab === 'genres' && isLoadingGenres)

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Header */}
      <div className="sticky top-0 z-40 bg-card/95 backdrop-blur border-b border-border">
        <div className="max-w-6xl mx-auto px-4 py-6">
          <h1 className="text-3xl font-bold text-foreground mb-6">Discover</h1>

          {/* Tab Navigation */}
          <div className="flex gap-2 overflow-x-auto pb-2">
            <Button
              variant={activeTab === 'for-you' ? 'default' : 'outline'}
              onClick={() => setActiveTab('for-you')}
              className="gap-2 whitespace-nowrap"
            >
              <span>✨</span>
              For You
            </Button>
            <Button
              variant={activeTab === 'trending' ? 'default' : 'outline'}
              onClick={() => setActiveTab('trending')}
              className="gap-2 whitespace-nowrap"
            >
              <span>🔥</span>
              Trending
            </Button>
            <Button
              variant={activeTab === 'producers' ? 'default' : 'outline'}
              onClick={() => setActiveTab('producers')}
              className="gap-2 whitespace-nowrap"
            >
              <span>👤</span>
              Producers
            </Button>
            <Button
              variant={activeTab === 'genres' ? 'default' : 'outline'}
              onClick={() => setActiveTab('genres')}
              className="gap-2 whitespace-nowrap"
            >
              <span>🎵</span>
              Genres
            </Button>
          </div>

          {/* Trending Time Window Filter */}
          {activeTab === 'trending' && (
            <div className="flex gap-2 mt-4">
              <span className="text-sm text-muted-foreground self-center">Trending:</span>
              {(['day', 'week', 'month'] as const).map((window) => (
                <Button
                  key={window}
                  variant={timeWindow === window ? 'default' : 'outline'}
                  onClick={() => setTimeWindow(window)}
                  size="sm"
                >
                  {window === 'day' ? 'Today' : window === 'week' ? 'This Week' : 'This Month'}
                </Button>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Content */}
      <main className="max-w-6xl mx-auto px-4 py-8">
        {isLoading ? (
          <div className="flex justify-center py-12">
            <Spinner size="lg" />
          </div>
        ) : activeTab === 'for-you' ? (
          <div className="space-y-4">
            <h2 className="text-2xl font-bold text-foreground mb-6">Recommended For You</h2>
            {recommendations && recommendations.length > 0 ? (
              <div className="space-y-4">
                {recommendations.map((rec) => (
                  <div key={rec.post.id} className="group">
                    <PostCard post={rec.post} />
                    {rec.reason && (
                      <div className="text-xs text-muted-foreground mt-2 px-4">
                        💡 {rec.reason}
                      </div>
                    )}
                  </div>
                ))}
              </div>
            ) : (
              <div className="bg-card border border-border rounded-lg p-12 text-center space-y-4">
                <div className="text-4xl">🔄</div>
                <div className="text-lg font-semibold text-foreground">
                  Recommendations coming soon
                </div>
                <p className="text-muted-foreground max-w-md mx-auto">
                  Like and listen to posts to get personalized recommendations
                </p>
              </div>
            )}
          </div>
        ) : activeTab === 'trending' ? (
          <div className="space-y-4">
            <h2 className="text-2xl font-bold text-foreground mb-6">
              Trending This {timeWindow === 'day' ? 'Week' : timeWindow === 'month' ? 'Month' : 'Week'}
            </h2>
            {trendingPosts && trendingPosts.length > 0 ? (
              <div className="space-y-4">
                {trendingPosts.map((post) => (
                  <PostCard key={post.id} post={post} />
                ))}
              </div>
            ) : (
              <div className="bg-card border border-border rounded-lg p-12 text-center space-y-4">
                <div className="text-4xl">📊</div>
                <div className="text-lg font-semibold text-foreground">No trending posts yet</div>
              </div>
            )}
          </div>
        ) : activeTab === 'producers' ? (
          <div className="space-y-4">
            <h2 className="text-2xl font-bold text-foreground mb-6">Trending Producers</h2>
            {trendingProducers && trendingProducers.length > 0 ? (
              <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
                {trendingProducers.map((producer) => (
                  <TrendingProducerCard key={producer.id} producer={producer} />
                ))}
              </div>
            ) : (
              <div className="bg-card border border-border rounded-lg p-12 text-center space-y-4">
                <div className="text-4xl">👥</div>
                <div className="text-lg font-semibold text-foreground">No trending producers</div>
              </div>
            )}
          </div>
        ) : (
          <div className="space-y-4">
            <h2 className="text-2xl font-bold text-foreground mb-6">Genre Picks</h2>
            {genreRecommendations && genreRecommendations.length > 0 ? (
              <div className="space-y-4">
                {genreRecommendations.map((post) => (
                  <PostCard key={post.id} post={post} />
                ))}
              </div>
            ) : (
              <div className="bg-card border border-border rounded-lg p-12 text-center space-y-4">
                <div className="text-4xl">🎵</div>
                <div className="text-lg font-semibold text-foreground">
                  No genre recommendations yet
                </div>
              </div>
            )}
          </div>
        )}
      </main>
    </div>
  )
}
