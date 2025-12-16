import { useNavigate } from 'react-router-dom'
import { Avatar } from '@/components/ui/avatar'
import type { Story } from '@/models/Story'
import type { Ephemeral } from '@/models/Ephemeral'
import { formatDistanceToNow } from '@/utils/format'

interface StoryCardProps {
  story: Story | Ephemeral
  onStoryClick?: (story: Story | Ephemeral) => void
}

const verbEmoji: Record<string, string> = {
  post: '🎵',
  like: '❤️',
  comment: '💬',
  follow: '👥',
  repost: '🔄',
  save: '💾',
  share: '🔗',
  joined: '👋',
}

function isStory(story: Story | Ephemeral): story is Story {
  return 'verb' in story && 'description' in story
}

/**
 * StoryCard - Display a single activity/story in the timeline
 */
export function StoryCard({ story, onStoryClick }: StoryCardProps) {
  const navigate = useNavigate()

  if (!isStory(story)) {
    // Handle Ephemeral (visual story) differently
    return (
      <div
        className="bg-card border border-border rounded-lg p-4 hover:border-coral-pink/50 transition-colors cursor-pointer"
        onClick={() => onStoryClick?.(story)}
      >
        <div className="flex gap-3">
          <Avatar
            src={story.profilePictureUrl}
            alt={story.displayName}
            size="md"
            className="flex-shrink-0"
          />
          <div className="flex-1 min-w-0">
            <div className="flex items-start justify-between mb-2">
              <div>
                <p className="font-semibold text-foreground">{story.displayName}</p>
                <p className="text-xs text-muted-foreground">@{story.username}</p>
              </div>
              <span className="text-xs text-muted-foreground">
                {formatDistanceToNow(story.createdAt)}
              </span>
            </div>
            {story.caption && (
              <p className="text-sm text-foreground mb-2">{story.caption}</p>
            )}
            <p className="text-xs text-muted-foreground">
              {story.viewCount} views • {story.likeCount} likes
            </p>
          </div>
        </div>
      </div>
    )
  }

  const handleUserClick = (e: React.MouseEvent) => {
    e.stopPropagation()
    navigate(`/profile/${story.username}`)
  }

  const handleRelatedUserClick = (e: React.MouseEvent) => {
    if (!story.relatedUserName) return
    e.stopPropagation()
    navigate(`/profile/${story.relatedUserName}`)
  }

  const handleRelatedPostClick = (e: React.MouseEvent) => {
    if (!story.relatedPostId) return
    e.stopPropagation()
    // Could navigate to post detail view if available
  }

  return (
    <div
      className="bg-card border border-border rounded-lg p-4 hover:border-coral-pink/50 transition-colors cursor-pointer"
      onClick={() => onStoryClick?.(story)}
    >
      <div className="flex gap-3">
        {/* Avatar */}
        <Avatar
          src={story.userAvatarUrl}
          alt={story.displayName}
          size="md"
          className="flex-shrink-0"
        />

        {/* Content */}
        <div className="flex-1 min-w-0">
          <div className="flex items-start justify-between mb-2">
            <div className="flex items-center gap-2 min-w-0">
              <span className="text-base">{verbEmoji[story.verb] || '📝'}</span>
              <div className="min-w-0">
                <button
                  onClick={handleUserClick}
                  className="font-semibold text-foreground hover:text-coral-pink transition-colors truncate"
                >
                  {story.displayName}
                </button>
                <p className="text-xs text-muted-foreground truncate">@{story.username}</p>
              </div>
            </div>
            <span className="text-xs text-muted-foreground flex-shrink-0 ml-2">
              {formatDistanceToNow(story.createdAt)}
            </span>
          </div>

          {/* Description */}
          <p className="text-sm text-foreground mb-2">
            {story.description}
            {story.relatedUserName && (
              <>
                {' '}
                <button
                  onClick={handleRelatedUserClick}
                  className="text-coral-pink hover:text-rose-pink transition-colors"
                >
                  {story.relatedUserName}
                </button>
              </>
            )}
            {story.relatedPostTitle && (
              <>
                {' '}
                <button
                  onClick={handleRelatedPostClick}
                  className="text-coral-pink hover:text-rose-pink transition-colors truncate"
                >
                  "{story.relatedPostTitle}"
                </button>
              </>
            )}
          </p>

          {/* Tags/Metadata */}
          <div className="flex gap-2 flex-wrap">
            <span className="inline-block px-2 py-1 bg-muted rounded-full text-xs text-muted-foreground capitalize">
              {story.verb}
            </span>
          </div>
        </div>
      </div>
    </div>
  )
}
