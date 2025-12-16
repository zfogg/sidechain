import { useEffect, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { usePlaylistStore } from '@/stores/usePlaylistStore'
import { PlaylistCard } from '@/components/playlists/PlaylistCard'
import { CreatePlaylistDialog } from '@/components/playlists/CreatePlaylistDialog'
import { Button } from '@/components/ui/button'

/**
 * Playlists Page
 * Browse and manage user playlists
 */
export function Playlists() {
  const navigate = useNavigate()
  const { userPlaylists, isLoading, loadUserPlaylists } = usePlaylistStore()
  const [isCreateDialogOpen, setIsCreateDialogOpen] = useState(false)

  useEffect(() => {
    loadUserPlaylists()
  }, [])

  return (
    <div className="min-h-screen bg-bg-primary">
      {/* Header */}
      <div className="sticky top-0 z-10 bg-bg-primary border-b border-border p-4">
        <div className="max-w-7xl mx-auto flex items-center justify-between">
          <div>
            <h1 className="text-2xl font-bold text-foreground">My Playlists</h1>
            <p className="text-sm text-muted-foreground">Organize and share your favorite loops</p>
          </div>

          <Button onClick={() => setIsCreateDialogOpen(true)} className="gap-2">
            <span>+</span>
            <span>Create Playlist</span>
          </Button>
        </div>
      </div>

      {/* Content */}
      <div className="max-w-7xl mx-auto p-4">
        {isLoading ? (
          <div className="flex items-center justify-center py-12">
            <div className="text-muted-foreground">Loading playlists...</div>
          </div>
        ) : userPlaylists.length === 0 ? (
          <div className="flex flex-col items-center justify-center py-12">
            <div className="text-4xl mb-4">🎵</div>
            <h2 className="text-lg font-semibold text-foreground mb-2">No playlists yet</h2>
            <p className="text-muted-foreground mb-4">Create your first playlist to get started</p>
            <Button onClick={() => setIsCreateDialogOpen(true)}>Create Playlist</Button>
          </div>
        ) : (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
            {userPlaylists.map((playlist) => (
              <PlaylistCard key={playlist.id} playlist={playlist} />
            ))}
          </div>
        )}
      </div>

      {/* Create Playlist Dialog */}
      <CreatePlaylistDialog isOpen={isCreateDialogOpen} onOpenChange={setIsCreateDialogOpen} />
    </div>
  )
}
