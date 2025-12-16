import { useQuery, useInfiniteQuery } from '@tanstack/react-query'
import { RemixClient } from '@/api/RemixClient'
import { Spinner } from '@/components/ui/spinner'
import { Button } from '@/components/ui/button'
import { useNavigate } from 'react-router-dom'

interface RemixesListProps {
  postId: string
}

/**
 * RemixesList - Display all remixes of a post with pagination
 *
 * Features:
 * - Infinite scroll pagination
 * - Creator attribution
 * - Like and save buttons
 * - Click to view remix details
 */
export function RemixesList({ postId }: RemixesListProps) {
  const navigate = useNavigate()

  const {
    data,
    isLoading,
    error,
    hasNextPage,
    fetchNextPage,
    isFetchingNextPage,
  } = useInfiniteQuery({
    queryKey: ['postRemixes', postId],
    queryFn: async ({ pageParam = 0 }) => {
      const result = await RemixClient.getPostRemixes(postId, 20, pageParam)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    getNextPageParam: (lastPage) => {
      return lastPage.length === 20 ? lastPage.length : undefined
    },
    initialPageParam: 0,
  })

  const remixes = data?.pages.flat() || []

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-12">
        <Spinner size="lg" />
      </div>
    )
  }

  if (error) {
    return (
      <div className="text-center py-8 text-muted-foreground">
        <p>Failed to load remixes</p>
      </div>
    )
  }

  if (remixes.length === 0) {
    return (
      <div className="text-center py-12 text-muted-foreground">
        <p className="text-lg mb-2">No remixes yet</p>
        <p className="text-sm">Be the first to remix this post!</p>
      </div>
    )
  }

  return (
    <div>
      <div className="space-y-4">
        {remixes.map((remix) => (
          <RemixCard
            key={remix.id}
            remix={remix}
            onViewDetails={() => navigate(`/remixes/${remix.id}`)}
          />
        ))}
      </div>

      {/* Load More */}
      {hasNextPage && (
        <div className="flex justify-center mt-8">
          <Button
            onClick={() => fetchNextPage()}
            disabled={isFetchingNextPage}
            variant="outline"
          >
            {isFetchingNextPage ? <Spinner size="sm" /> : 'Load More Remixes'}
          </Button>
        </div>
      )}
    </div>
  )
}

interface RemixCardProps {
  remix: any
  onViewDetails: () => void
}

/**
 * RemixCard - Individual remix card in the list
 */
function RemixCard({ remix, onViewDetails }: RemixCardProps) {
  return (
    <button
      onClick={onViewDetails}
      className="w-full text-left bg-card border border-border rounded-lg p-4 hover:border-coral-pink/50 hover:shadow-lg transition-all group"
    >
      {/* Header */}
      <div className="flex items-start justify-between mb-2">
        <div className="flex-1 min-w-0">
          <h3 className="font-semibold text-foreground group-hover:text-coral-pink transition-colors truncate">
            {remix.title}
          </h3>
          <p className="text-sm text-muted-foreground mt-1">
            by @{remix.username}
          </p>
        </div>
        <div className="flex-shrink-0 ml-2">
          {remix.isOwnRemix && (
            <span className="inline-block px-2 py-1 bg-coral-pink/10 border border-coral-pink/20 rounded text-xs font-semibold text-coral-pink">
              Your Remix
            </span>
          )}
        </div>
      </div>

      {/* Description */}
      {remix.description && (
        <p className="text-sm text-muted-foreground mb-2 line-clamp-2">{remix.description}</p>
      )}

      {/* Metadata */}
      <div className="flex items-center gap-2 mb-3 text-xs text-muted-foreground">
        {remix.bpm && <span>{remix.bpm} BPM</span>}
        {remix.key && <span className="text-border">•</span>}
        {remix.key && <span>Key: {remix.key}</span>}
        {remix.genre && remix.genre.length > 0 && (
          <>
            <span className="text-border">•</span>
            <span>{remix.genre[0]}</span>
          </>
        )}
      </div>

      {/* Stats */}
      <div className="flex items-center gap-4 pt-2 border-t border-border">
        <div className="flex items-center gap-1">
          <span className="text-sm font-semibold text-foreground">{remix.likeCount}</span>
          <span className="text-xs text-muted-foreground">♥️</span>
        </div>
        <div className="flex items-center gap-1">
          <span className="text-sm font-semibold text-foreground">{remix.playCount}</span>
          <span className="text-xs text-muted-foreground">▶</span>
        </div>
        <div className="flex items-center gap-1 ml-auto">
          <span className="text-sm font-semibold text-foreground">{remix.remixCount}</span>
          <span className="text-xs text-muted-foreground">remixes</span>
        </div>
      </div>
    </button>
  )
}
