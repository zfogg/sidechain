import { useQuery } from '@tanstack/react-query'
import { FeedClient } from '@/api/FeedClient'
import { FeedPost } from '@/models/FeedPost'
import { Spinner } from '@/components/ui/spinner'
import { formatTime } from '@/utils/audio'

interface PinnedPostsSectionProps {
  userId: string
}

/**
 * PinnedPostsSection - Display user's pinned posts prominently on profile
 *
 * Features:
 * - Shows up to 3 pinned posts in a grid
 * - Displays post metadata (BPM, key, genre)
 * - Interactive cards with audio preview
 * - Loading and empty states
 * - Responsive grid layout
 */
export function PinnedPostsSection({ userId }: PinnedPostsSectionProps) {
  const {
    data: pinnedPosts = [],
    isLoading,
    error,
  } = useQuery({
    queryKey: ['pinnedPosts', userId],
    queryFn: async () => {
      const result = await FeedClient.getUserPinnedPosts(userId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    staleTime: 5 * 60 * 1000, // 5 minutes
    gcTime: 30 * 60 * 1000, // 30 minutes
  })

  // Don't show section if no pinned posts
  if (!isLoading && (!pinnedPosts || pinnedPosts.length === 0)) {
    return null
  }

  return (
    <section className="bg-card border border-border rounded-lg p-6 mb-8">
      <h2 className="text-lg font-bold text-foreground mb-4 flex items-center gap-2">
        <span>📌</span> Pinned Posts
      </h2>

      {isLoading ? (
        <div className="flex items-center justify-center py-12">
          <Spinner size="md" />
        </div>
      ) : error ? (
        <div className="text-center py-8 text-muted-foreground">
          Failed to load pinned posts
        </div>
      ) : pinnedPosts.length > 0 ? (
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {pinnedPosts.map((post) => (
            <PinnedPostCard key={post.id} post={post} />
          ))}
        </div>
      ) : null}
    </section>
  )
}

interface PinnedPostCardProps {
  post: FeedPost
}

/**
 * PinnedPostCard - Individual pinned post card
 */
function PinnedPostCard({ post }: PinnedPostCardProps) {
  return (
    <div className="bg-bg-secondary border border-border rounded-lg overflow-hidden hover:border-coral-pink/50 hover:shadow-lg transition-all hover:scale-105 cursor-pointer group">
      {/* Cover Art / Waveform Preview */}
      <div className="relative h-32 bg-gradient-to-br from-coral-pink/20 to-lavender/20 flex items-center justify-center overflow-hidden">
        {post.waveformUrl ? (
          <img
            src={post.waveformUrl}
            alt={post.filename}
            className="w-full h-full object-cover group-hover:scale-110 transition-transform"
          />
        ) : (
          <div className="text-4xl opacity-30">🎵</div>
        )}

        {/* Play Button Overlay */}
        {post.audioUrl && (
          <div className="absolute inset-0 flex items-center justify-center bg-black/50 opacity-0 group-hover:opacity-100 transition-opacity">
            <div className="w-12 h-12 bg-coral-pink rounded-full flex items-center justify-center text-white">
              <svg
                className="w-6 h-6 ml-0.5"
                fill="currentColor"
                viewBox="0 0 24 24"
              >
                <path d="M8 5v14l11-7z" />
              </svg>
            </div>
          </div>
        )}
      </div>

      {/* Post Info */}
      <div className="p-3">
        {/* Title */}
        <h3 className="font-semibold text-sm text-foreground truncate mb-2">
          {post.filename}
        </h3>

        {/* Metadata */}
        <div className="text-xs text-muted-foreground space-y-1 mb-3">
          {post.bpm && (
            <div className="flex items-center gap-2">
              <span className="font-medium">{post.bpm}</span> BPM
              {post.key && <span className="text-xs">• {post.key}</span>}
            </div>
          )}

          {post.daw && (
            <div>
              <span className="font-medium">{post.daw}</span>
            </div>
          )}

          {post.genre && post.genre.length > 0 && (
            <div className="flex flex-wrap gap-1">
              {post.genre.slice(0, 2).map((g) => (
                <span key={g} className="bg-bg-tertiary px-2 py-0.5 rounded text-xs">
                  {g}
                </span>
              ))}
              {post.genre.length > 2 && (
                <span className="text-xs text-muted-foreground">+{post.genre.length - 2}</span>
              )}
            </div>
          )}
        </div>

        {/* Stats */}
        <div className="flex gap-3 text-xs text-muted-foreground border-t border-border pt-2">
          <div className="flex items-center gap-1">
            <span>❤️</span>
            <span>{post.likeCount}</span>
          </div>
          <div className="flex items-center gap-1">
            <span>💬</span>
            <span>{post.commentCount}</span>
          </div>
          <div className="flex items-center gap-1">
            <span>🎧</span>
            <span>{post.playCount}</span>
          </div>
        </div>
      </div>
    </div>
  )
}
