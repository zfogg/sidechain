import { useEffect, useState } from 'react'
import { useParams, useNavigate } from 'react-router-dom'
import { usePlaylistStore } from '@/stores/usePlaylistStore'
import { PostCard } from '@/components/feed/PostCard'
import { Button } from '@/components/ui/button'
import { FeedClient } from '@/api/FeedClient'

/**
 * PlaylistDetail Page
 * View and manage individual playlist with all tracks
 */
export function PlaylistDetail() {
  const { playlistId } = useParams<{ playlistId: string }>()
  const navigate = useNavigate()
  const { currentPlaylist, isLoading, loadPlaylist, deletePlaylist, toggleFollowPlaylist } =
    usePlaylistStore()

  const [posts, setPosts] = useState<any[]>([])
  const [isLoadingTracks, setIsLoadingTracks] = useState(false)

  useEffect(() => {
    if (playlistId) {
      loadPlaylist(playlistId)
    }
  }, [playlistId])

  // Load tracks for the playlist
  useEffect(() => {
    if (currentPlaylist?.trackIds.length) {
      setIsLoadingTracks(true)

      // Fetch all posts that are in the playlist
      Promise.all(
        currentPlaylist.trackIds.map((postId) =>
          FeedClient.getPost?.(postId).catch(() => null)
        )
      )
        .then((results) => {
          setPosts(results.filter((p) => p !== null))
          setIsLoadingTracks(false)
        })
        .catch(() => setIsLoadingTracks(false))
    }
  }, [currentPlaylist?.trackIds])

  if (isLoading) {
    return (
      <div className="min-h-screen bg-bg-primary flex items-center justify-center">
        <div className="text-muted-foreground">Loading playlist...</div>
      </div>
    )
  }

  if (!currentPlaylist) {
    return (
      <div className="min-h-screen bg-bg-primary flex items-center justify-center">
        <div className="text-center">
          <h1 className="text-2xl font-bold text-foreground mb-2">Playlist not found</h1>
          <Button onClick={() => navigate('/playlists')} variant="outline">
            Back to Playlists
          </Button>
        </div>
      </div>
    )
  }

  const handleDeletePlaylist = async () => {
    if (window.confirm(`Delete "${currentPlaylist.title}"? This cannot be undone.`)) {
      await deletePlaylist(currentPlaylist.id)
      navigate('/playlists')
    }
  }

  const handleFollowPlaylist = async () => {
    await toggleFollowPlaylist(currentPlaylist.id, !currentPlaylist.isFollowing)
  }

  return (
    <div className="min-h-screen bg-bg-primary">
      {/* Header with Cover */}
      <div className="bg-gradient-to-b from-coral-pink/10 to-bg-primary border-b border-border">
        <div className="max-w-4xl mx-auto p-4 flex gap-6">
          {/* Cover Image */}
          <div className="w-32 h-32 rounded-lg overflow-hidden bg-muted flex-shrink-0">
            {currentPlaylist.coverUrl ? (
              <img
                src={currentPlaylist.coverUrl}
                alt={currentPlaylist.title}
                className="w-full h-full object-cover"
              />
            ) : (
              <div className="w-full h-full flex items-center justify-center bg-gradient-to-br from-coral-pink/20 to-rose-pink/20">
                <div className="text-5xl text-coral-pink opacity-50">♫</div>
              </div>
            )}
          </div>

          {/* Info */}
          <div className="flex-1 flex flex-col justify-between">
            <div>
              <h1 className="text-3xl font-bold text-foreground mb-2">{currentPlaylist.title}</h1>
              <p className="text-muted-foreground mb-2">{currentPlaylist.description}</p>
              <div className="flex gap-4 text-sm text-muted-foreground">
                <span>by {currentPlaylist.displayName}</span>
                <span>{currentPlaylist.trackCount} tracks</span>
                <span>{currentPlaylist.followerCount} followers</span>
                {currentPlaylist.isPublic && <span>🌍 Public</span>}
              </div>
            </div>

            {/* Actions */}
            <div className="flex gap-2">
              {currentPlaylist.isOwnPlaylist ? (
                <>
                  <Button onClick={() => navigate(`/playlists/${currentPlaylist.id}/edit`)} variant="outline">
                    Edit
                  </Button>
                  <Button onClick={handleDeletePlaylist} variant="outline" className="text-red-400">
                    Delete
                  </Button>
                </>
              ) : (
                <Button onClick={handleFollowPlaylist} variant={currentPlaylist.isFollowing ? 'outline' : 'default'}>
                  {currentPlaylist.isFollowing ? 'Following' : 'Follow'}
                </Button>
              )}
            </div>
          </div>
        </div>
      </div>

      {/* Tracks */}
      <div className="max-w-4xl mx-auto p-4">
        {isLoadingTracks ? (
          <div className="text-center py-8 text-muted-foreground">Loading tracks...</div>
        ) : posts.length === 0 ? (
          <div className="text-center py-12">
            <div className="text-4xl mb-4">🎵</div>
            <p className="text-muted-foreground">No tracks in this playlist yet</p>
          </div>
        ) : (
          <div className="space-y-4">
            <h2 className="text-lg font-semibold text-foreground">Tracks ({posts.length})</h2>
            {posts.map((post) => (
              <PostCard key={post.id} post={post} />
            ))}
          </div>
        )}
      </div>
    </div>
  )
}
