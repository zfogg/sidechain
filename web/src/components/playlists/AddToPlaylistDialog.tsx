import { useEffect } from 'react'
import { Dialog, DialogContent, DialogHeader, DialogTitle } from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { usePlaylistStore } from '@/stores/usePlaylistStore'
import { Spinner } from '@/components/ui/spinner'

interface AddToPlaylistDialogProps {
  postId: string
  isOpen: boolean
  onOpenChange: (open: boolean) => void
}

/**
 * AddToPlaylistDialog - Select a playlist to add track to
 */
export function AddToPlaylistDialog({ postId, isOpen, onOpenChange }: AddToPlaylistDialogProps) {
  const { userPlaylists, isLoading, loadUserPlaylists, addTrackToPlaylist } = usePlaylistStore()

  useEffect(() => {
    if (isOpen) {
      loadUserPlaylists()
    }
  }, [isOpen])

  const handleAddToPlaylist = async (playlistId: string) => {
    await addTrackToPlaylist(playlistId, postId)
    onOpenChange(false)
  }

  return (
    <Dialog open={isOpen} onOpenChange={onOpenChange}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Add to Playlist</DialogTitle>
        </DialogHeader>

        <div className="space-y-2 max-h-96 overflow-y-auto">
          {isLoading ? (
            <div className="flex items-center justify-center py-8">
              <Spinner />
            </div>
          ) : userPlaylists.length === 0 ? (
            <div className="text-center py-8 text-muted-foreground">
              <p>No playlists yet</p>
              <p className="text-sm">Create a playlist first to add tracks</p>
            </div>
          ) : (
            userPlaylists.map((playlist) => (
              <button
                key={playlist.id}
                onClick={() => handleAddToPlaylist(playlist.id)}
                className="w-full text-left p-3 rounded-lg border border-border hover:border-coral-pink/50 hover:bg-muted transition-colors flex items-center justify-between"
              >
                <div>
                  <p className="font-medium text-foreground">{playlist.title}</p>
                  <p className="text-xs text-muted-foreground">{playlist.trackCount} tracks</p>
                </div>
                <span className="text-muted-foreground">→</span>
              </button>
            ))
          )}
        </div>
      </DialogContent>
    </Dialog>
  )
}
