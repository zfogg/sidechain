import { useState, useRef } from 'react'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription } from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Textarea } from '@/components/ui/textarea'
import { Avatar } from '@/components/ui/avatar'
import { UserProfileClient } from '@/api/UserProfileClient'
import { useUserStore } from '@/stores/useUserStore'

interface EditProfileDialogProps {
  isOpen: boolean
  onOpenChange: (open: boolean) => void
}

/**
 * EditProfileDialog - Dialog for editing user profile (name, bio, picture)
 */
export function EditProfileDialog({ isOpen, onOpenChange }: EditProfileDialogProps) {
  const { user } = useUserStore()
  const fileInputRef = useRef<HTMLInputElement>(null)

  const [displayName, setDisplayName] = useState(user?.displayName || '')
  const [bio, setBio] = useState(user?.bio || '')
  const [profilePictureUrl, setProfilePictureUrl] = useState(user?.profilePictureUrl || '')
  const [isUploading, setIsUploading] = useState(false)
  const [isSaving, setIsSaving] = useState(false)
  const [error, setError] = useState('')

  const handlePictureChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0]
    if (!file) return

    // Validate file type
    if (!file.type.startsWith('image/')) {
      setError('Please select an image file')
      return
    }

    // Validate file size (5MB max)
    if (file.size > 5 * 1024 * 1024) {
      setError('Image must be less than 5MB')
      return
    }

    setIsUploading(true)
    setError('')

    const result = await UserProfileClient.uploadProfilePicture(file)

    if (result.isOk()) {
      setProfilePictureUrl(result.getValue().url)
    } else {
      setError(result.getError())
    }

    setIsUploading(false)
  }

  const handleSave = async () => {
    if (!displayName.trim()) {
      setError('Display name is required')
      return
    }

    setIsSaving(true)
    setError('')

    const result = await UserProfileClient.updateProfile({
      displayName: displayName.trim(),
      bio: bio.trim(),
    })

    if (result.isOk()) {
      // Update local user store
      useUserStore.setState((state) => ({
        user: state.user
          ? {
              ...state.user,
              displayName: displayName.trim(),
              bio: bio.trim(),
              profilePictureUrl,
            }
          : null,
      }))

      onOpenChange(false)
    } else {
      setError(result.getError())
    }

    setIsSaving(false)
  }

  if (!user) return null

  return (
    <Dialog open={isOpen} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-md">
        <DialogHeader>
          <DialogTitle>Edit Profile</DialogTitle>
          <DialogDescription>Update your profile information and picture</DialogDescription>
        </DialogHeader>

        <div className="space-y-4">
          {/* Profile Picture */}
          <div className="flex flex-col items-center gap-3">
            <Avatar src={profilePictureUrl} alt={displayName} size="lg" />
            <input
              ref={fileInputRef}
              type="file"
              accept="image/*"
              onChange={handlePictureChange}
              className="hidden"
              disabled={isUploading}
            />
            <Button
              variant="outline"
              size="sm"
              onClick={() => fileInputRef.current?.click()}
              disabled={isUploading}
            >
              {isUploading ? 'Uploading...' : 'Change Picture'}
            </Button>
          </div>

          {/* Error Message */}
          {error && <div className="text-sm text-red-400 text-center">{error}</div>}

          {/* Display Name */}
          <div>
            <label className="text-sm font-medium text-foreground mb-2 block">Display Name</label>
            <Input
              placeholder="Your name"
              value={displayName}
              onChange={(e) => setDisplayName(e.target.value)}
              disabled={isSaving}
              maxLength={50}
            />
            <p className="text-xs text-muted-foreground mt-1">{displayName.length}/50</p>
          </div>

          {/* Bio */}
          <div>
            <label className="text-sm font-medium text-foreground mb-2 block">Bio</label>
            <Textarea
              placeholder="Tell others about yourself..."
              value={bio}
              onChange={(e) => setBio(e.target.value)}
              disabled={isSaving}
              maxLength={500}
              className="min-h-[100px] resize-none"
            />
            <p className="text-xs text-muted-foreground mt-1">{bio.length}/500</p>
          </div>

          {/* Actions */}
          <div className="flex gap-2 justify-end pt-4">
            <Button variant="outline" onClick={() => onOpenChange(false)} disabled={isSaving}>
              Cancel
            </Button>
            <Button onClick={handleSave} disabled={isSaving || !displayName.trim()}>
              {isSaving ? 'Saving...' : 'Save Changes'}
            </Button>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  )
}
