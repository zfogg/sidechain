import { useState, useEffect, useRef } from 'react'
import type { FeedPost } from '@/models/FeedPost'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuSeparator,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'
import { useLikeMutation, useSaveMutation, usePlayTrackMutation } from '@/hooks/mutations/useFeedMutations'
import { createAudioPlayer, formatTime, formatGenres, type AudioPlayer } from '@/utils/audio'
import { CommentsPanel } from '@/components/comments/CommentsPanel'
import { ReportButton } from '@/components/report/ReportButton'
import { AddToPlaylistDialog } from '@/components/playlists/AddToPlaylistDialog'
import { WaveformDisplay } from '@/components/audio/WaveformDisplay'
import { RepostQuoteDialog } from '@/components/feed/RepostQuoteDialog'
import { DownloadButton } from '@/components/feed/DownloadButton'
import { CreateRemixDialog } from '@/components/remixes/CreateRemixDialog'
import { SocialClient } from '@/api/SocialClient'

interface PostCardProps {
  post: FeedPost
}

/**
 * PostCard - Displays a single feed post with audio player
 * Mirrors C++ PostCard component with real-time updates
 *
 * Features:
 * - Audio playback with Howler.js
 * - Like/save with optimistic updates
 * - Engagement metrics
 * - User profile display
 * - Genre and metadata display
 */
export function PostCard({ post }: PostCardProps) {
  const [isPlaying, setIsPlaying] = useState(false)
  const [progress, setProgress] = useState(0)
  const [duration, setDuration] = useState(0)
  const [isCommentsPanelOpen, setIsCommentsPanelOpen] = useState(false)
  const [isAddToPlaylistOpen, setIsAddToPlaylistOpen] = useState(false)
  const [isRepostQuoteOpen, setIsRepostQuoteOpen] = useState(false)
  const [socialActionLoading, setSocialActionLoading] = useState<string | null>(null)
  const [socialActionError, setSocialActionError] = useState('')
  const playerRef = useRef<AudioPlayer | null>(null)
  const animationRef = useRef<number | null>(null)

  const { mutate: toggleLike, isPending: isLikePending } = useLikeMutation()
  const { mutate: toggleSave, isPending: isSavePending } = useSaveMutation()
  const { mutate: trackPlay } = usePlayTrackMutation()

  // Initialize audio player
  useEffect(() => {
    if (!playerRef.current && post.audioUrl) {
      playerRef.current = createAudioPlayer(post.audioUrl, () => {
        setIsPlaying(false)
      })
      setDuration(playerRef.current.getDuration())
    }

    return () => {
      if (playerRef.current) {
        playerRef.current.destroy()
        playerRef.current = null
      }
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current)
      }
    }
  }, [post.audioUrl])

  // Update progress animation
  useEffect(() => {
    const updateProgress = () => {
      if (playerRef.current && isPlaying) {
        const newProgress = playerRef.current.getProgress()
        setProgress(newProgress)
        const newDuration = playerRef.current.getDuration()
        if (newDuration > 0 && newDuration !== duration) {
          setDuration(newDuration)
        }
        animationRef.current = requestAnimationFrame(updateProgress)
      }
    }

    if (isPlaying) {
      animationRef.current = requestAnimationFrame(updateProgress)
    }

    return () => {
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current)
      }
    }
  }, [isPlaying, duration])

  const handlePlayPause = () => {
    if (!playerRef.current) return

    if (isPlaying) {
      playerRef.current.pause()
      setIsPlaying(false)
    } else {
      playerRef.current.play()
      setIsPlaying(true)
      // Track play event with duration
      trackPlay(post.id, duration)
    }
  }

  const handleSeek = (e: React.MouseEvent<HTMLDivElement>) => {
    if (!playerRef.current) return

    const rect = e.currentTarget.getBoundingClientRect()
    const clickX = e.clientX - rect.left
    const percentage = clickX / rect.width
    const newTime = percentage * duration

    playerRef.current.seek(newTime)
    setProgress(percentage)
  }

  const handleLike = () => {
    toggleLike({ postId: post.id, shouldLike: !post.isLiked })
  }

  const handleSave = () => {
    toggleSave({ postId: post.id, shouldSave: !post.isSaved })
  }

  const handleComment = () => {
    setIsCommentsPanelOpen(true)
  }

  const handleArchive = async () => {
    setSocialActionLoading('archive')
    setSocialActionError('')
    const result = await SocialClient.archivePost(post.id)
    if (result.isError()) {
      setSocialActionError(result.getError())
    }
    setSocialActionLoading(null)
  }

  const handlePin = async () => {
    setSocialActionLoading('pin')
    setSocialActionError('')
    const result = await SocialClient.pinPost(post.id)
    if (result.isError()) {
      setSocialActionError(result.getError())
    }
    setSocialActionLoading(null)
  }

  const handleMuteUser = async () => {
    setSocialActionLoading('mute')
    setSocialActionError('')
    const result = await SocialClient.muteUser(post.userId)
    if (result.isError()) {
      setSocialActionError(result.getError())
    }
    setSocialActionLoading(null)
  }

  const handleBlockUser = async () => {
    setSocialActionLoading('block')
    setSocialActionError('')
    const result = await SocialClient.blockUser(post.userId)
    if (result.isError()) {
      setSocialActionError(result.getError())
    }
    setSocialActionLoading(null)
  }

  const timeDisplay = `${formatTime(progress * duration)} / ${formatTime(duration)}`

  return (
    <>
      <div className="bg-card border border-border rounded-lg overflow-hidden shadow-sm hover:shadow-md transition-shadow">
      {/* User Info Header */}
      <div className="p-4 flex items-center gap-3 border-b border-border">
        <img
          src={post.userAvatarUrl || 'https://api.dicebear.com/7.x/avataaars/svg?seed=' + post.userId}
          alt={post.displayName}
          className="w-10 h-10 rounded-full bg-muted"
        />
        <div className="flex-1 min-w-0">
          <div className="font-medium text-sm truncate">{post.displayName}</div>
          <div className="text-xs text-muted-foreground truncate">@{post.username}</div>
        </div>
        {!post.isOwnPost && (
          <Button variant="outline" size="sm">
            {post.isFollowing ? 'Following' : 'Follow'}
          </Button>
        )}
      </div>

      {/* Audio Player */}
      <div className="p-4 space-y-4">
        {/* Play Button and Title */}
        <div className="flex items-start gap-3">
          <button
            onClick={handlePlayPause}
            disabled={!post.audioUrl}
            className="flex-shrink-0 w-12 h-12 rounded-lg bg-primary hover:bg-primary/90 disabled:bg-muted disabled:cursor-not-allowed flex items-center justify-center transition-colors"
          >
            {isPlaying ? (
              <svg
                className="w-6 h-6 text-white"
                fill="currentColor"
                viewBox="0 0 24 24"
              >
                <path d="M6 4h4v16H6V4zm8 0h4v16h-4V4z" />
              </svg>
            ) : (
              <svg
                className="w-6 h-6 text-white ml-1"
                fill="currentColor"
                viewBox="0 0 24 24"
              >
                <path d="M8 5v14l11-7z" />
              </svg>
            )}
          </button>

          <div className="flex-1 min-w-0">
            <h3 className="font-semibold text-base truncate">{post.filename}</h3>
            <div className="text-xs text-muted-foreground space-y-1 mt-1">
              {post.bpm && (
                <div>
                  <span className="font-medium">{post.bpm}</span> BPM
                  {post.key && <span className="ml-2">• {post.key}</span>}
                </div>
              )}
              {post.daw && (
                <div>
                  <span className="font-medium">{post.daw}</span>
                </div>
              )}
              {post.genre && post.genre.length > 0 && (
                <div>{formatGenres(post.genre)}</div>
              )}
            </div>
          </div>
        </div>

        {/* Waveform Display */}
        {post.audioUrl && (
          <>
            <WaveformDisplay
              waveformUrl={post.waveformUrl}
              progress={progress}
              duration={duration}
              isPlaying={isPlaying}
              onClick={handleSeek}
            />

            <div className="flex justify-between items-center text-xs text-muted-foreground mt-2">
              <span>{timeDisplay}</span>
              <span>{post.playCount} plays</span>
            </div>
          </>
        )}
      </div>

      {/* Engagement Metrics */}
      <div className="px-4 py-2 flex gap-4 text-xs text-muted-foreground border-t border-border bg-muted/20">
        <div className="hover:text-foreground transition-colors cursor-pointer">
          <span className="font-medium text-foreground">{post.likeCount}</span> Likes
        </div>
        <div className="hover:text-foreground transition-colors cursor-pointer">
          <span className="font-medium text-foreground">{post.commentCount}</span> Comments
        </div>
        <div className="hover:text-foreground transition-colors cursor-pointer">
          <span className="font-medium text-foreground">{post.saveCount}</span> Saves
        </div>
      </div>

      {/* Action Buttons */}
      <div className="p-3 flex gap-2 border-t border-border flex-wrap">
        <Button
          variant="ghost"
          size="sm"
          onClick={handleLike}
          disabled={isLikePending}
          className="flex-1 min-w-0 gap-2"
        >
          {isLikePending ? (
            <Spinner size="sm" />
          ) : (
            <svg
              className={`w-4 h-4 ${post.isLiked ? 'text-coral-pink' : ''}`}
              fill={post.isLiked ? 'currentColor' : 'none'}
              stroke="currentColor"
              viewBox="0 0 24 24"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2}
                d="M4.318 6.318a4.5 4.5 0 000 6.364L12 20.364l7.682-7.682a4.5 4.5 0 00-6.364-6.364L12 7.636l-1.318-1.318a4.5 4.5 0 00-6.364 0z"
              />
            </svg>
          )}
          <span className="hidden sm:inline">Like</span>
        </Button>

        <Button
          variant="ghost"
          size="sm"
          onClick={handleComment}
          className="flex-1 min-w-0 gap-2"
        >
          <svg
            className="w-4 h-4"
            fill="none"
            stroke="currentColor"
            viewBox="0 0 24 24"
          >
            <path
              strokeLinecap="round"
              strokeLinejoin="round"
              strokeWidth={2}
              d="M8 12h.01M12 12h.01M16 12h.01M21 12c0 4.418-4.03 8-9 8a9.863 9.863 0 01-4.255-.949L3 20l1.395-3.72C3.512 15.042 3 13.574 3 12c0-4.418 4.03-8 9-8s9 3.582 9 8z"
            />
          </svg>
          <span className="hidden sm:inline">Comment</span>
        </Button>

        <Button
          variant="ghost"
          size="sm"
          onClick={handleSave}
          disabled={isSavePending}
          className="flex-1 min-w-0 gap-2"
        >
          {isSavePending ? (
            <Spinner size="sm" />
          ) : (
            <svg
              className={`w-4 h-4 ${post.isSaved ? 'text-amber-500' : ''}`}
              fill={post.isSaved ? 'currentColor' : 'none'}
              stroke="currentColor"
              viewBox="0 0 24 24"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2}
                d="M5 5a2 2 0 012-2h6a2 2 0 012 2v14l-5-2.5L5 19V5z"
              />
            </svg>
          )}
          <span className="hidden sm:inline">Save</span>
        </Button>

        <Button
          variant="ghost"
          size="sm"
          onClick={() => setIsRepostQuoteOpen(true)}
          className="flex-1 min-w-0 gap-2"
          disabled={socialActionLoading === 'repost'}
        >
          {socialActionLoading === 'repost' ? (
            <Spinner size="sm" />
          ) : (
            <span>↻</span>
          )}
          <span className="hidden sm:inline">Repost</span>
        </Button>

        <CreateRemixDialog
          postId={post.id}
          trigger={
            <Button
              variant="ghost"
              size="sm"
              className="flex-1 min-w-0 gap-2"
            >
              <span>🎵</span>
              <span className="hidden sm:inline">Remix</span>
            </Button>
          }
        />

        <DownloadButton
          postId={post.id}
          hasAudio={!!post.audioUrl}
          hasMidi={false}
          hasProject={false}
          variant="ghost"
          size="sm"
          className="flex-1 min-w-0"
        />

        {/* More Actions Dropdown */}
        <DropdownMenu>
          <DropdownMenuTrigger asChild>
            <Button
              variant="ghost"
              size="sm"
              className="px-2"
              title="More options"
            >
              ⋯
            </Button>
          </DropdownMenuTrigger>

          <DropdownMenuContent align="end">
            <DropdownMenuItem
              onClick={() => setIsAddToPlaylistOpen(true)}
            >
              <span>🎵 Add to Playlist</span>
            </DropdownMenuItem>

            {post.isOwnPost && (
              <>
                <DropdownMenuItem
                  onClick={handlePin}
                  disabled={socialActionLoading === 'pin'}
                >
                  <span>📌 {socialActionLoading === 'pin' ? 'Pinning...' : 'Pin to Profile'}</span>
                </DropdownMenuItem>

                <DropdownMenuItem
                  onClick={handleArchive}
                  disabled={socialActionLoading === 'archive'}
                >
                  <span>📂 {socialActionLoading === 'archive' ? 'Archiving...' : 'Archive'}</span>
                </DropdownMenuItem>
              </>
            )}

            {!post.isOwnPost && (
              <>
                <DropdownMenuItem
                  onClick={handleMuteUser}
                  disabled={socialActionLoading === 'mute'}
                >
                  <span>🔇 {socialActionLoading === 'mute' ? 'Muting...' : 'Mute User'}</span>
                </DropdownMenuItem>

                <DropdownMenuItem
                  onClick={handleBlockUser}
                  disabled={socialActionLoading === 'block'}
                  className="text-red-500"
                >
                  <span>{socialActionLoading === 'block' ? 'Blocking...' : '🚫 Block User'}</span>
                </DropdownMenuItem>

                <DropdownMenuSeparator />
              </>
            )}

            <ReportButton
              type="post"
              targetId={post.id}
              targetName={post.filename}
              variant="ghost"
              size="sm"
              showLabel={false}
            />
          </DropdownMenuContent>
        </DropdownMenu>
      </div>

      {/* Error Message */}
      {socialActionError && (
        <div className="px-3 py-2 text-xs text-red-400 bg-red-500/10 border-t border-red-500/20">
          {socialActionError}
        </div>
      )}
      </div>

      {/* Comments Panel Dialog */}
      <CommentsPanel
        postId={post.id}
        isOpen={isCommentsPanelOpen}
        onOpenChange={setIsCommentsPanelOpen}
      />

      {/* Add to Playlist Dialog */}
      <AddToPlaylistDialog
        postId={post.id}
        isOpen={isAddToPlaylistOpen}
        onOpenChange={setIsAddToPlaylistOpen}
      />

      {/* Repost with Quote Dialog */}
      <RepostQuoteDialog
        postId={post.id}
        isOpen={isRepostQuoteOpen}
        onClose={() => setIsRepostQuoteOpen(false)}
        onSuccess={() => {
          setSocialActionLoading(null)
          setSocialActionError('')
        }}
      />
    </>
  )
}
