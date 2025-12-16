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
import { Textarea } from '@/components/ui/textarea'
import { RemixClient } from '@/api/RemixClient'
import { Spinner } from '@/components/ui/spinner'

interface CreateRemixDialogProps {
  postId: string
  parentRemixId?: string
  trigger?: React.ReactNode
}

/**
 * CreateRemixDialog - Create new remix of a post
 *
 * Features:
 * - Title and description input
 * - Audio/MIDI upload
 * - Parent remix selection
 * - Real-time validation
 */
export function CreateRemixDialog({
  postId,
  parentRemixId,
  trigger,
}: CreateRemixDialogProps) {
  const [open, setOpen] = useState(false)
  const [title, setTitle] = useState('')
  const [description, setDescription] = useState('')
  const [audioUrl, setAudioUrl] = useState('')
  const [midiUrl, setMidiUrl] = useState('')
  const [errors, setErrors] = useState<Record<string, string>>({})

  const queryClient = useQueryClient()

  const createRemixMutation = useMutation({
    mutationFn: async () => {
      if (!title.trim()) {
        setErrors({ title: 'Title is required' })
        return
      }

      if (!audioUrl.trim()) {
        setErrors({ audioUrl: 'Audio file is required' })
        return
      }

      const result = await RemixClient.createRemix({
        parentPostId: postId,
        parentRemixId,
        title: title.trim(),
        description: description.trim() || undefined,
        audioUrl: audioUrl.trim(),
        midiUrl: midiUrl.trim() || undefined,
      })

      if (result.isError()) {
        throw new Error(result.getError())
      }

      return result.getValue()
    },
    onSuccess: () => {
      // Invalidate remix chain and post remixes queries
      queryClient.invalidateQueries({ queryKey: ['remixChain', postId] })
      queryClient.invalidateQueries({ queryKey: ['postRemixes', postId] })

      // Reset form
      setTitle('')
      setDescription('')
      setAudioUrl('')
      setMidiUrl('')
      setErrors({})
      setOpen(false)
    },
    onError: (error: Error) => {
      setErrors({ submit: error.message })
    },
  })

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        {trigger ? (
          trigger
        ) : (
          <Button className="bg-coral-pink hover:bg-coral-pink/90 text-white">
            + Remix This Post
          </Button>
        )}
      </DialogTrigger>

      <DialogContent className="max-w-md">
        <DialogHeader>
          <DialogTitle>Create Remix</DialogTitle>
          <DialogDescription>
            Add your unique twist to this post and share your version
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-4">
          {/* Title */}
          <div>
            <label className="block text-sm font-medium text-foreground mb-1">
              Remix Title
            </label>
            <Input
              value={title}
              onChange={(e) => {
                setTitle(e.target.value)
                if (errors.title) setErrors({ ...errors, title: '' })
              }}
              placeholder="Give your remix a name"
              className="w-full"
            />
            {errors.title && <p className="text-xs text-red-500 mt-1">{errors.title}</p>}
          </div>

          {/* Description */}
          <div>
            <label className="block text-sm font-medium text-foreground mb-1">
              Description (Optional)
            </label>
            <Textarea
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              placeholder="Tell us about your remix..."
              rows={3}
              className="w-full"
            />
          </div>

          {/* Audio Upload */}
          <div>
            <label className="block text-sm font-medium text-foreground mb-1">
              Audio File (MP3, WAV, etc.)
            </label>
            <Input
              type="url"
              value={audioUrl}
              onChange={(e) => {
                setAudioUrl(e.target.value)
                if (errors.audioUrl) setErrors({ ...errors, audioUrl: '' })
              }}
              placeholder="Paste audio URL or upload"
              className="w-full"
            />
            {errors.audioUrl && <p className="text-xs text-red-500 mt-1">{errors.audioUrl}</p>}
          </div>

          {/* MIDI Upload (Optional) */}
          <div>
            <label className="block text-sm font-medium text-foreground mb-1">
              MIDI File (Optional)
            </label>
            <Input
              type="url"
              value={midiUrl}
              onChange={(e) => setMidiUrl(e.target.value)}
              placeholder="Paste MIDI URL or upload"
              className="w-full"
            />
          </div>

          {/* Error Message */}
          {errors.submit && (
            <div className="p-3 bg-red-500/10 border border-red-500/20 rounded text-sm text-red-500">
              {errors.submit}
            </div>
          )}

          {/* Actions */}
          <div className="flex gap-2 justify-end">
            <Button
              onClick={() => setOpen(false)}
              variant="outline"
              disabled={createRemixMutation.isPending}
            >
              Cancel
            </Button>
            <Button
              onClick={() => createRemixMutation.mutate()}
              className="bg-coral-pink hover:bg-coral-pink/90 text-white"
              disabled={createRemixMutation.isPending}
            >
              {createRemixMutation.isPending ? <Spinner size="sm" /> : 'Create Remix'}
            </Button>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  )
}
