import { useParams, Navigate } from 'react-router-dom'
import { useQuery } from '@tanstack/react-query'
import { SoundClient } from '@/api/SoundClient'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { useState } from 'react'

/**
 * SoundPage - Dedicated page for viewing a sound and all posts using it
 * Similar to TikTok's sound pages
 */
export function SoundPage() {
  const { soundId } = useParams<{ soundId: string }>()
  const [isLiked, setIsLiked] = useState(false)

  if (!soundId) {
    return <Navigate to="/" replace />
  }

  // Fetch sound details
  const {
    data: sound,
    isLoading: isSoundLoading,
    error: soundError,
  } = useQuery({
    queryKey: ['sound', soundId],
    queryFn: async () => {
      const result = await SoundClient.getSound(soundId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
  })

  // Fetch posts using this sound
  const {
    data: posts = [],
    isLoading: isPostsLoading,
  } = useQuery({
    queryKey: ['soundPosts', soundId],
    queryFn: async () => {
      const result = await SoundClient.getPostsUsingSound(soundId, 50, 0)
      if (result.isError()) {
        return []
      }
      return result.getValue()
    },
  })

  const handleLike = async () => {
    if (!sound) return
    setIsLiked(!isLiked)
    await SoundClient.likeSound(soundId, !isLiked)
  }

  if (isSoundLoading) {
    return (
      <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary flex items-center justify-center">
        <Spinner size="lg" />
      </div>
    )
  }

  if (soundError || !sound) {
    return (
      <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary flex items-center justify-center">
        <div className="text-center">
          <div className="text-xl font-bold text-foreground mb-2">Sound not found</div>
          <div className="text-muted-foreground">This sound may have been deleted</div>
        </div>
      </div>
    )
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Sound header */}
      <div className="bg-card/50 border-b border-border">
        <div className="max-w-4xl mx-auto px-4 py-8">
          <div className="flex gap-6">
            {/* Waveform/Thumbnail */}
            <div className="flex-shrink-0 w-24 h-24 bg-gradient-to-br from-coral-pink to-lavender rounded-lg overflow-hidden">
              {sound.waveformUrl && (
                <img src={sound.waveformUrl} alt={sound.title} className="w-full h-full object-cover" />
              )}
            </div>

            {/* Sound info */}
            <div className="flex-1">
              <h1 className="text-3xl font-bold text-foreground mb-2">{sound.title}</h1>

              {sound.description && (
                <p className="text-muted-foreground mb-4">{sound.description}</p>
              )}

              {/* Creator info */}
              <div className="flex items-center gap-3 mb-4">
                <img
                  src={
                    sound.creatorAvatarUrl ||
                    `https://api.dicebear.com/7.x/avataaars/svg?seed=${sound.creatorId}`
                  }
                  alt={sound.creatorDisplayName}
                  className="w-10 h-10 rounded-full"
                />
                <div>
                  <p className="font-semibold text-foreground">{sound.creatorDisplayName}</p>
                  <p className="text-sm text-muted-foreground">@{sound.creatorUsername}</p>
                </div>
              </div>

              {/* Metadata tags */}
              <div className="flex flex-wrap gap-2 mb-4">
                {sound.isOfficial && (
                  <span className="px-3 py-1 bg-coral-pink/20 text-coral-pink rounded-full text-sm font-semibold">
                    Official
                  </span>
                )}
                {sound.isTrending && (
                  <span className="px-3 py-1 bg-rose-pink/20 text-rose-pink rounded-full text-sm font-semibold">
                    🔥 Trending
                  </span>
                )}
                {sound.bpm && (
                  <span className="px-3 py-1 bg-bg-secondary text-foreground rounded-full text-sm">
                    {sound.bpm} BPM
                  </span>
                )}
                {sound.key && (
                  <span className="px-3 py-1 bg-bg-secondary text-foreground rounded-full text-sm">
                    {sound.key}
                  </span>
                )}
                {sound.genre && sound.genre.length > 0 && (
                  <span className="px-3 py-1 bg-bg-secondary text-foreground rounded-full text-sm">
                    {sound.genre[0]}
                  </span>
                )}
              </div>

              {/* Stats and actions */}
              <div className="flex gap-3">
                <Button
                  onClick={handleLike}
                  variant={isLiked ? 'default' : 'outline'}
                  className="gap-2"
                >
                  <span>{isLiked ? '❤️' : '🤍'}</span>
                  <span>{sound.likeCount}</span>
                </Button>

                <div className="flex items-center gap-2 px-4 py-2 rounded-lg bg-bg-secondary border border-border">
                  <span className="text-foreground font-semibold">{sound.useCount}</span>
                  <span className="text-muted-foreground text-sm">posts</span>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Posts using this sound */}
      <main className="max-w-4xl mx-auto px-4 py-8">
        <h2 className="text-2xl font-bold text-foreground mb-6">
          Posts using this sound ({sound.useCount})
        </h2>

        {isPostsLoading ? (
          <div className="flex items-center justify-center py-12">
            <Spinner size="lg" />
          </div>
        ) : posts.length > 0 ? (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {posts.map((post) => (
              <PostUsingSound key={post.id} post={post} />
            ))}
          </div>
        ) : (
          <div className="text-center py-12 text-muted-foreground">
            <p>No posts are currently using this sound</p>
          </div>
        )}
      </main>
    </div>
  )
}

interface PostUsingSound {
  id: string
  filename: string
  username: string
  displayName: string
  userAvatarUrl?: string
}

function PostUsingSound({ post }: { post: PostUsingSound }) {
  return (
    <div className="bg-card border border-border rounded-lg p-4 hover:border-coral-pink/50 transition-colors cursor-pointer">
      <div className="flex items-start gap-3 mb-3">
        <img
          src={
            post.userAvatarUrl ||
            `https://api.dicebear.com/7.x/avataaars/svg?seed=${post.username}`
          }
          alt={post.displayName}
          className="w-10 h-10 rounded-full"
        />
        <div className="flex-1 min-w-0">
          <p className="font-semibold text-foreground truncate">{post.displayName}</p>
          <p className="text-sm text-muted-foreground">@{post.username}</p>
        </div>
      </div>

      <p className="text-foreground font-medium truncate">{post.filename}</p>
      <p className="text-sm text-muted-foreground mt-2">Click to view post</p>
    </div>
  )
}
