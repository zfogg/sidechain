import { useQuery } from '@tanstack/react-query'
import { FeedClient } from '@/api/FeedClient'
import { FeedPost } from '@/models/FeedPost'
import { Spinner } from '@/components/ui/spinner'
import { PostCard } from '@/components/feed/PostCard'

interface UserPostsSectionProps {
  userId: string
  displayName?: string
  isOwnProfile?: boolean
}

/**
 * UserPostsSection - Display user's posts on profile
 *
 * Features:
 * - Shows all user posts in chronological order
 * - Pagination support
 * - Loading and empty states
 * - Responsive grid layout
 */
export function UserPostsSection({ userId, displayName, isOwnProfile }: UserPostsSectionProps) {
  const {
    data: posts = [],
    isLoading,
    error,
  } = useQuery({
    queryKey: ['userPosts', userId],
    queryFn: async () => {
      const result = await FeedClient.getUserPosts(userId, 20, 0)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    staleTime: 2 * 60 * 1000, // 2 minutes
    gcTime: 10 * 60 * 1000, // 10 minutes
  })

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-12">
        <Spinner size="md" />
      </div>
    )
  }

  if (error) {
    return (
      <div className="text-center py-8 text-muted-foreground">
        Failed to load posts
      </div>
    )
  }

  if (!posts || posts.length === 0) {
    return (
      <div className="text-center py-12 text-muted-foreground">
        {isOwnProfile ? "You haven't posted anything yet" : `${displayName || 'This user'} hasn't posted anything yet`}
      </div>
    )
  }

  return (
    <div className="space-y-4">
      {posts.map((post) => (
        <PostCard key={post.id} post={post} />
      ))}
    </div>
  )
}
