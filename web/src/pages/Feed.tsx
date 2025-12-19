import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { FeedList } from '@/components/feed/FeedList'
import { TrendingProducerCard } from '@/components/discovery/TrendingProducerCard'
import { PostCard } from '@/components/feed/PostCard'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { RealtimeIndicator } from '@/components/feed/RealtimeIndicator'
import { GorseClient } from '@/api/GorseClient'
import type { FeedType } from '@/api/FeedClient'
import { useWebSocket } from '@/hooks/useWebSocket'

type DiscoveryTab = 'timeline' | 'global' | 'trending' | 'forYou' | 'producers' | 'genres'

/**
 * Feed - Main feed page with multiple feed types and discovery options
 *
 * Features:
 * - Timeline (from followed users)
 * - Global (all users)
 * - Trending (most popular by time period)
 * - For You (personalized recommendations)
 * - Trending Producers
 * - Genre recommendations
 * - Real-time updates via WebSocket
 */
export function Feed() {
  const [activeTab, setActiveTab] = useState<DiscoveryTab>('timeline')
  const [timeWindow, setTimeWindow] = useState<'day' | 'week' | 'month'>('week')

  // Initialize WebSocket connection for real-time updates
  useWebSocket()

  // Fetch personalized recommendations
  const { data: recommendations, isLoading: isLoadingRecs } = useQuery({
    queryKey: ['gorse-recommendations'],
    queryFn: async () => {
      const result = await GorseClient.getRecommendations(50, 0)
      return result.isOk() ? result.getValue() : []
    },
    enabled: activeTab === 'forYou',
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
      const genres = ['House', 'Techno', 'Ambient']
      const results = await Promise.all(
        genres.map((genre) => GorseClient.getGenreRecommendations(genre, 20, 0))
      )
      return results.flatMap((r) => (r.isOk() ? r.getValue() : []))
    },
    enabled: activeTab === 'genres',
  })

  const feedTypes: { type: DiscoveryTab; label: string; icon: string }[] = [
    { type: 'timeline', label: 'Timeline', icon: '📰' },
    { type: 'global', label: 'Global', icon: '🌍' },
    { type: 'trending', label: 'Trending', icon: '🔥' },
    { type: 'forYou', label: 'For You', icon: '✨' },
    { type: 'producers', label: 'Producers', icon: '👤' },
    { type: 'genres', label: 'Genres', icon: '🎵' },
  ]

  const isLoading =
    (activeTab === 'forYou' && isLoadingRecs) ||
    (activeTab === 'producers' && isLoadingProducers) ||
    (activeTab === 'genres' && isLoadingGenres)

  const isFeedTab = ['timeline', 'global', 'trending'].includes(activeTab)

  return (
    <div className="min-h-screen bg-background">
      {/* Realtime Indicator */}
      <RealtimeIndicator />

      {/* Header */}
      <div className="sticky top-0 z-40 bg-card/95 backdrop-blur border-b border-border">
        <div className="max-w-4xl flex flex-col items-center mx-auto px-4 py-4">
          <h1 className="text-2xl font-bold mb-4">Sidechain Feed</h1>

          {/* Feed Type Selector */}
          <div className="flex gap-2 overflow-x-auto pb-2">
            {feedTypes.map(({ type, label, icon }) => (
              <Button
                key={type}
                onClick={() => setActiveTab(type)}
                variant={activeTab === type ? 'default' : 'outline'}
                className="gap-2 whitespace-nowrap"
              >
                <span>{icon}</span>
                {label}
              </Button>
            ))}
          </div>

          {/* Time Window Filter for Trending */}
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

      {/* Feed Content */}
      <main className="max-w-2xl mx-auto px-4 py-8">
        {isFeedTab ? (
          <FeedList feedType={activeTab as FeedType} />
        ) : isLoading ? (
          <div className="flex justify-center py-12">
            <Spinner size="lg" />
          </div>
        ) : activeTab === 'forYou' ? (
          <div className="space-y-4">
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
        ) : activeTab === 'producers' ? (
          <div className="space-y-4">
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
