import { useState } from 'react'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription } from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Textarea } from '@/components/ui/textarea'
import { usePlaylistStore } from '@/stores/usePlaylistStore'

interface CreatePlaylistDialogProps {
  isOpen: boolean
  onOpenChange: (open: boolean) => void
}

/**
 * CreatePlaylistDialog - Form for creating new playlists
 */
export function CreatePlaylistDialog({ isOpen, onOpenChange }: CreatePlaylistDialogProps) {
  const [title, setTitle] = useState('')
  const [description, setDescription] = useState('')
  const [isCreating, setIsCreating] = useState(false)

  const { createPlaylist } = usePlaylistStore()

  const handleCreate = async () => {
    if (!title.trim()) return

    setIsCreating(true)
    await createPlaylist(title, description)
    setIsCreating(false)

    // Reset form
    setTitle('')
    setDescription('')
    onOpenChange(false)
  }

  return (
    <Dialog open={isOpen} onOpenChange={onOpenChange}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Create New Playlist</DialogTitle>
          <DialogDescription>Create a new playlist to organize and share your favorite loops</DialogDescription>
        </DialogHeader>

        <div className="space-y-4">
          {/* Title Input */}
          <div>
            <label className="text-sm font-medium text-foreground mb-2 block">Playlist Name</label>
            <Input
              placeholder="e.g., Summer Vibes"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              disabled={isCreating}
            />
          </div>

          {/* Description Input */}
          <div>
            <label className="text-sm font-medium text-foreground mb-2 block">Description (Optional)</label>
            <Textarea
              placeholder="Tell others what this playlist is about"
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              disabled={isCreating}
              className="min-h-[100px]"
            />
          </div>

          {/* Actions */}
          <div className="flex gap-2 justify-end">
            <Button variant="outline" onClick={() => onOpenChange(false)} disabled={isCreating}>
              Cancel
            </Button>
            <Button onClick={handleCreate} disabled={!title.trim() || isCreating}>
              {isCreating ? 'Creating...' : 'Create Playlist'}
            </Button>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  )
}
