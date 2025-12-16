import { useQuery } from '@tanstack/react-query'
import { SoundClient } from '@/api/SoundClient'
import { Spinner } from '@/components/ui/spinner'
import { useNavigate } from 'react-router-dom'

/**
 * TrendingSounds - Display trending sounds/loops
 *
 * Features:
 * - Shows top trending sounds
 * - Use count and creator info
 * - Click to navigate to sound page
 * - Loading and error states
 */
export function TrendingSounds({ limit = 10 }: { limit?: number }) {
  const navigate = useNavigate()

  const {
    data: sounds = [],
    isLoading,
    error,
  } = useQuery({
    queryKey: ['trendingSounds', limit],
    queryFn: async () => {
      const result = await SoundClient.getTrendingSounds(limit, 0)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    staleTime: 10 * 60 * 1000, // 10 minutes
    gcTime: 60 * 60 * 1000, // 1 hour
  })

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-8">
        <Spinner size="md" />
      </div>
    )
  }

  if (error || !sounds || sounds.length === 0) {
    return null
  }

  return (
    <section className="bg-card border border-border rounded-lg p-6">
      <h2 className="text-lg font-bold text-foreground mb-4 flex items-center gap-2">
        <span>🔥</span> Trending Sounds
      </h2>

      <div className="space-y-3">
        {sounds.map((sound) => (
          <button
            key={sound.id}
            onClick={() => navigate(`/sounds/${sound.id}`)}
            className="w-full text-left flex items-center gap-3 p-3 rounded-lg hover:bg-bg-secondary transition-colors group"
          >
            {/* Sound thumbnail */}
            <div className="flex-shrink-0 w-12 h-12 bg-gradient-to-br from-coral-pink to-lavender rounded overflow-hidden">
              {sound.waveformUrl && (
                <img
                  src={sound.waveformUrl}
                  alt={sound.title}
                  className="w-full h-full object-cover"
                />
              )}
            </div>

            {/* Sound info */}
            <div className="flex-1 min-w-0">
              <p className="font-medium text-foreground truncate group-hover:text-coral-pink">
                {sound.title}
              </p>
              <p className="text-xs text-muted-foreground truncate">
                by @{sound.creatorUsername}
              </p>
            </div>

            {/* Use count */}
            <div className="flex-shrink-0 text-right">
              <p className="text-sm font-semibold text-foreground">{sound.useCount}</p>
              <p className="text-xs text-muted-foreground">uses</p>
            </div>
          </button>
        ))}
      </div>

      {/* View all link */}
      <button
        onClick={() => navigate('/discover/sounds')}
        className="w-full mt-4 py-2 text-sm text-coral-pink hover:text-rose-pink transition-colors font-semibold"
      >
        View All Trending Sounds →
      </button>
    </section>
  )
}
