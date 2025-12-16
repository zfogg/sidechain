import { useNavigate } from 'react-router-dom'
import { TrendingProducer } from '@/api/GorseClient'
import { Button } from '@/components/ui/button'
import { useFollowMutation } from '@/hooks/mutations/useFollowMutation'
import { DirectMessageButton } from '@/components/chat/DirectMessageButton'

interface TrendingProducerCardProps {
  producer: TrendingProducer
}

/**
 * TrendingProducerCard - Card component for displaying trending producers
 */
export function TrendingProducerCard({ producer }: TrendingProducerCardProps) {
  const navigate = useNavigate()
  const { mutate: toggleFollow, isPending: isFollowPending } = useFollowMutation()

  return (
    <div className="bg-card border border-border rounded-lg p-6 hover:border-coral-pink/50 transition-colors flex flex-col h-full">
      {/* Avatar & Stats Badge */}
      <div className="mb-4">
        <div className="relative">
          <img
            src={
              producer.profilePictureUrl ||
              `https://api.dicebear.com/7.x/avataaars/svg?seed=${producer.id}`
            }
            alt={producer.displayName}
            className="w-20 h-20 rounded-full mx-auto cursor-pointer hover:opacity-80 transition-opacity"
            onClick={() => navigate(`/profile/${producer.username}`)}
          />
          {/* Trending Badge */}
          <div className="absolute -top-1 -right-1 bg-coral-pink text-white text-xs font-bold px-2 py-1 rounded-full">
            🔥 #{Math.round(producer.trendingScore)}
          </div>
        </div>
      </div>

      {/* Name & Bio */}
      <div className="text-center mb-4 flex-1">
        <h3
          className="font-semibold text-foreground cursor-pointer hover:text-coral-pink transition-colors"
          onClick={() => navigate(`/profile/${producer.username}`)}
        >
          {producer.displayName}
        </h3>
        <p className="text-sm text-muted-foreground">@{producer.username}</p>
        {producer.bio && (
          <p className="text-xs text-muted-foreground mt-2 line-clamp-2">{producer.bio}</p>
        )}
      </div>

      {/* Stats */}
      <div className="grid grid-cols-2 gap-2 mb-4 text-center border-t border-border pt-3">
        <div>
          <div className="text-sm font-semibold text-foreground">{producer.postCount}</div>
          <div className="text-xs text-muted-foreground">Posts</div>
        </div>
        <div>
          <div className="text-sm font-semibold text-foreground">
            {producer.followerCount > 1000
              ? `${(producer.followerCount / 1000).toFixed(1)}K`
              : producer.followerCount}
          </div>
          <div className="text-xs text-muted-foreground">Followers</div>
        </div>
      </div>

      {/* Actions */}
      <div className="flex gap-2">
        <Button
          onClick={() => navigate(`/profile/${producer.username}`)}
          variant="outline"
          size="sm"
          className="flex-1"
        >
          View
        </Button>
        <Button
          onClick={() => toggleFollow({ userId: producer.id, shouldFollow: !producer.isFollowing })}
          disabled={isFollowPending}
          variant={producer.isFollowing ? 'outline' : 'default'}
          size="sm"
          className={`flex-1 ${
            producer.isFollowing
              ? 'border-border/50 hover:border-red-500/30 hover:bg-red-500/10 hover:text-red-400'
              : ''
          }`}
        >
          {producer.isFollowing ? 'Following' : 'Follow'}
        </Button>
      </div>
    </div>
  )
}
