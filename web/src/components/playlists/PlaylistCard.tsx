import { useNavigate } from 'react-router-dom'
import type { Playlist } from '@/models/Playlist'
import { Button } from '@/components/ui/button'
import { usePlaylistStore } from '@/stores/usePlaylistStore'

interface PlaylistCardProps {
  playlist: Playlist
}

/**
 * PlaylistCard - Displays a single playlist with cover, metadata, and actions
 */
export function PlaylistCard({ playlist }: PlaylistCardProps) {
  const navigate = useNavigate()
  const { toggleFollowPlaylist } = usePlaylistStore()

  const handleFollow = async () => {
    await toggleFollowPlaylist(playlist.id, !playlist.isFollowing)
  }

  return (
    <div
      onClick={() => navigate(`/playlists/${playlist.id}`)}
      className="bg-card border border-border rounded-lg overflow-hidden hover:border-coral-pink/50 transition-colors cursor-pointer group"
    >
      {/* Cover Image */}
      <div className="relative w-full aspect-square bg-muted overflow-hidden">
        {playlist.coverUrl ? (
          <img
            src={playlist.coverUrl}
            alt={playlist.title}
            className="w-full h-full object-cover group-hover:opacity-80 transition-opacity"
          />
        ) : (
          <div className="w-full h-full flex items-center justify-center bg-gradient-to-br from-coral-pink/20 to-rose-pink/20">
            <div className="text-4xl text-coral-pink opacity-50">♫</div>
          </div>
        )}

        {/* Overlay Info */}
        <div className="absolute inset-0 bg-black/40 opacity-0 group-hover:opacity-100 transition-opacity flex flex-col justify-end p-3">
          <h3 className="font-bold text-white truncate">{playlist.title}</h3>
          <p className="text-xs text-gray-200">{playlist.trackCount} tracks</p>
        </div>
      </div>

      {/* Info Section */}
      <div className="p-3 space-y-2">
        {/* Title */}
        <div>
          <h3 className="font-semibold text-foreground truncate hover:text-coral-pink transition-colors">
            {playlist.title}
          </h3>
          <p className="text-xs text-muted-foreground truncate">by {playlist.displayName}</p>
        </div>

        {/* Metadata */}
        <div className="flex gap-2 text-xs text-muted-foreground">
          <span>🎵 {playlist.trackCount}</span>
          <span>👥 {playlist.followerCount}</span>
          {playlist.isPublic && <span>🌍 Public</span>}
        </div>

        {/* Actions */}
        {!playlist.isOwnPlaylist && (
          <Button
            onClick={(e) => {
              e.stopPropagation()
              handleFollow()
            }}
            variant={playlist.isFollowing ? 'outline' : 'default'}
            size="sm"
            className="w-full"
          >
            {playlist.isFollowing ? 'Following' : 'Follow'}
          </Button>
        )}
      </div>
    </div>
  )
}
