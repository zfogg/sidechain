import { useState } from 'react'
import { useMutation, useQueryClient } from '@tanstack/react-query'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { PlaylistClient } from '@/api/PlaylistClient'
import { PlaylistCollaborator, PlaylistCollaboratorRole } from '@/models/Playlist'
import { Spinner } from '@/components/ui/spinner'

interface CollaboratorManagerProps {
  playlistId: string
  collaborators: PlaylistCollaborator[]
  isOwner: boolean
}

/**
 * CollaboratorManager - Manage playlist collaborators with roles
 *
 * Features:
 * - Add collaborators by username
 * - Assign roles (editor/viewer)
 * - Remove collaborators
 * - Display collaborator list
 */
export function CollaboratorManager({
  playlistId,
  collaborators,
  isOwner,
}: CollaboratorManagerProps) {
  const [open, setOpen] = useState(false)
  const [username, setUsername] = useState('')
  const [role, setRole] = useState<PlaylistCollaboratorRole>('viewer')
  const [errors, setErrors] = useState<Record<string, string>>({})

  const queryClient = useQueryClient()

  const addCollaboratorMutation = useMutation({
    mutationFn: async () => {
      if (!username.trim()) {
        setErrors({ username: 'Username is required' })
        return
      }

      const result = await PlaylistClient.addCollaborator(
        playlistId,
        username.trim(),
        role
      )

      if (result.isError()) {
        throw new Error(result.getError())
      }

      return result.getValue()
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['playlist', playlistId] })
      setUsername('')
      setRole('viewer')
      setErrors({})
    },
    onError: (error: Error) => {
      setErrors({ submit: error.message })
    },
  })

  const removeCollaboratorMutation = useMutation({
    mutationFn: async (userId: string) => {
      const result = await PlaylistClient.removeCollaborator(playlistId, userId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['playlist', playlistId] })
    },
  })

  const updateRoleMutation = useMutation({
    mutationFn: async (data: { userId: string; role: PlaylistCollaboratorRole }) => {
      const result = await PlaylistClient.updateCollaboratorRole(
        playlistId,
        data.userId,
        data.role
      )
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['playlist', playlistId] })
    },
  })

  if (!isOwner) {
    return null
  }

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button variant="outline" className="gap-2">
          <span>👥</span> Manage Collaborators
        </Button>
      </DialogTrigger>

      <DialogContent className="max-w-md max-h-96 overflow-y-auto">
        <DialogHeader>
          <DialogTitle>Manage Collaborators</DialogTitle>
          <DialogDescription>
            Add collaborators and manage their access level
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-4">
          {/* Add Collaborator Section */}
          <div className="p-3 bg-bg-secondary rounded-lg space-y-3">
            <p className="text-sm font-semibold text-foreground">Add New Collaborator</p>

            <div>
              <label className="block text-xs font-medium text-muted-foreground mb-1">
                Username
              </label>
              <Input
                value={username}
                onChange={(e) => {
                  setUsername(e.target.value)
                  if (errors.username) setErrors({ ...errors, username: '' })
                }}
                placeholder="Enter username"
                className="w-full text-sm"
              />
              {errors.username && (
                <p className="text-xs text-red-500 mt-1">{errors.username}</p>
              )}
            </div>

            <div>
              <label className="block text-xs font-medium text-muted-foreground mb-1">
                Role
              </label>
              <Select value={role} onValueChange={(v) => setRole(v as PlaylistCollaboratorRole)}>
                <SelectTrigger className="w-full text-sm">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="viewer">Viewer (read-only)</SelectItem>
                  <SelectItem value="editor">Editor (can add/remove tracks)</SelectItem>
                </SelectContent>
              </Select>
            </div>

            {errors.submit && (
              <div className="p-2 bg-red-500/10 border border-red-500/20 rounded text-xs text-red-500">
                {errors.submit}
              </div>
            )}

            <Button
              onClick={() => addCollaboratorMutation.mutate()}
              disabled={addCollaboratorMutation.isPending || !username.trim()}
              className="w-full bg-coral-pink hover:bg-coral-pink/90 text-white text-sm"
            >
              {addCollaboratorMutation.isPending ? <Spinner size="sm" /> : 'Add Collaborator'}
            </Button>
          </div>

          {/* Collaborators List */}
          <div className="space-y-2">
            <p className="text-sm font-semibold text-foreground">
              Collaborators ({collaborators.length})
            </p>

            {collaborators.length === 0 ? (
              <p className="text-xs text-muted-foreground py-2">No collaborators yet</p>
            ) : (
              <div className="space-y-2">
                {collaborators.map((collab) => (
                  <div
                    key={collab.userId}
                    className="flex items-center gap-2 p-2 bg-bg-secondary rounded text-sm"
                  >
                    <div className="flex-1">
                      <p className="font-medium text-foreground">{collab.displayName}</p>
                      <p className="text-xs text-muted-foreground">@{collab.username}</p>
                    </div>

                    <Select
                      value={collab.role}
                      onValueChange={(newRole) =>
                        updateRoleMutation.mutate({
                          userId: collab.userId,
                          role: newRole as PlaylistCollaboratorRole,
                        })
                      }
                      disabled={updateRoleMutation.isPending}
                    >
                      <SelectTrigger className="w-24 h-8 text-xs">
                        <SelectValue />
                      </SelectTrigger>
                      <SelectContent>
                        <SelectItem value="viewer">Viewer</SelectItem>
                        <SelectItem value="editor">Editor</SelectItem>
                      </SelectContent>
                    </Select>

                    <Button
                      onClick={() => removeCollaboratorMutation.mutate(collab.userId)}
                      disabled={removeCollaboratorMutation.isPending}
                      variant="ghost"
                      size="sm"
                      className="px-2 h-8"
                      title="Remove collaborator"
                    >
                      ✕
                    </Button>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      </DialogContent>
    </Dialog>
  )
}
