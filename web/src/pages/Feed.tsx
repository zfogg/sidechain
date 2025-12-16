import { useState } from 'react'
import { FeedList } from '@/components/feed/FeedList'
import { Button } from '@/components/ui/button'
import type { FeedType } from '@/api/FeedClient'
import { useWebSocket } from '@/hooks/useWebSocket'

/**
 * Feed - Main feed page with multiple feed types
 *
 * Features:
 * - Timeline (from followed users)
 * - Global (all users)
 * - Trending (most popular)
 * - For You (personalized recommendations)
 * - Real-time updates via WebSocket
 */
export function Feed() {
  const [feedType, setFeedType] = useState<FeedType>('timeline')

  // Initialize WebSocket connection for real-time updates
  useWebSocket()

  const feedTypes: { type: FeedType; label: string; icon: string }[] = [
    { type: 'timeline', label: 'Timeline', icon: '📰' },
    { type: 'global', label: 'Global', icon: '🌍' },
    { type: 'trending', label: 'Trending', icon: '🔥' },
    { type: 'forYou', label: 'For You', icon: '✨' },
  ]

  return (
    <div className="min-h-screen bg-background">
      {/* Header */}
      <div className="sticky top-0 z-40 bg-card/95 backdrop-blur border-b border-border">
        <div className="max-w-2xl mx-auto px-4 py-4">
          <h1 className="text-2xl font-bold mb-4">Sidechain Feed</h1>

          {/* Feed Type Selector */}
          <div className="flex gap-2 overflow-x-auto pb-2">
            {feedTypes.map(({ type, label, icon }) => (
              <Button
                key={type}
                onClick={() => setFeedType(type)}
                variant={feedType === type ? 'default' : 'outline'}
                className="gap-2 whitespace-nowrap"
              >
                <span>{icon}</span>
                {label}
              </Button>
            ))}
          </div>
        </div>
      </div>

      {/* Feed Content */}
      <main className="max-w-2xl mx-auto px-4 py-8">
        <FeedList feedType={feedType} />
      </main>
    </div>
  )
}
