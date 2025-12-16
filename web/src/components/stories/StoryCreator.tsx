import { useState } from 'react'
import { StoryClient } from '@/api/StoryClient'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { Spinner } from '@/components/ui/spinner'

interface StoryCreatorProps {
  isOpen: boolean
  onOpenChange: (open: boolean) => void
  onSuccess?: () => void
}

type MediaType = 'image' | 'video' | 'audio'

/**
 * StoryCreator - Create and upload ephemeral stories
 *
 * Features:
 * - Upload image, video, or audio
 * - Add captions
 * - Audio story with visualizations
 * - Progress upload
 * - Error handling
 */
export function StoryCreator({ isOpen, onOpenChange, onSuccess }: StoryCreatorProps) {
  const [step, setStep] = useState<'select' | 'upload' | 'details'>('select')
  const [mediaType, setMediaType] = useState<MediaType | null>(null)
  const [file, setFile] = useState<File | null>(null)
  const [preview, setPreview] = useState<string | null>(null)
  const [caption, setCaption] = useState('')
  const [duration, setDuration] = useState(5)
  const [isUploading, setIsUploading] = useState(false)
  const [error, setError] = useState('')
  const [visualizationType, setVisualizationType] = useState<
    'circular' | 'piano-roll' | 'waveform' | 'note-waterfall'
  >('circular')

  const handleFileSelect = (e: React.ChangeEvent<HTMLInputElement>) => {
    const selectedFile = e.target.files?.[0]
    if (!selectedFile) return

    setFile(selectedFile)
    setError('')

    // Create preview
    const reader = new FileReader()
    reader.onload = (event) => {
      setPreview(event.target?.result as string)
    }
    reader.readAsDataURL(selectedFile)

    // Auto-detect duration for videos
    if (mediaType === 'video') {
      const video = document.createElement('video')
      video.onloadedmetadata = () => {
        setDuration(Math.ceil(video.duration))
      }
      video.src = URL.createObjectURL(selectedFile)
    }

    setStep('details')
  }

  const handleCreate = async () => {
    if (!file || !mediaType) {
      setError('Please select a file')
      return
    }

    setIsUploading(true)
    setError('')

    try {
      // Upload file to get URL (simplified - would use actual upload service)
      // For now, create object URL
      const mediaUrl = URL.createObjectURL(file)

      const result = await StoryClient.createStory({
        mediaUrl,
        mediaType,
        duration,
        caption: caption || undefined,
        isAudioStory: mediaType === 'audio',
        visualizationType: mediaType === 'audio' ? visualizationType : undefined,
      })

      if (result.isOk()) {
        onSuccess?.()
        handleClose()
      } else {
        setError(result.getError())
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create story')
    } finally {
      setIsUploading(false)
    }
  }

  const handleClose = () => {
    setStep('select')
    setMediaType(null)
    setFile(null)
    setPreview(null)
    setCaption('')
    setDuration(5)
    setError('')
    onOpenChange(false)
  }

  return (
    <Dialog open={isOpen} onOpenChange={handleClose}>
      <DialogContent className="sm:max-w-md">
        <DialogHeader>
          <DialogTitle>Create a Story</DialogTitle>
          <DialogDescription>Share a temporary moment with your followers (expires in 24h)</DialogDescription>
        </DialogHeader>

        <div className="space-y-4 py-4">
          {step === 'select' && (
            <div className="grid grid-cols-3 gap-3">
              {(['image', 'video', 'audio'] as MediaType[]).map((type) => (
                <button
                  key={type}
                  onClick={() => {
                    setMediaType(type)
                    setStep('upload')
                  }}
                  className="p-4 rounded-lg border-2 border-border hover:border-coral-pink transition-colors"
                >
                  <div className="text-3xl mb-2">
                    {type === 'image' && '🖼️'}
                    {type === 'video' && '🎥'}
                    {type === 'audio' && '🎵'}
                  </div>
                  <p className="text-sm font-semibold capitalize text-foreground">{type}</p>
                </button>
              ))}
            </div>
          )}

          {step === 'upload' && (
            <div className="space-y-4">
              <div className="border-2 border-dashed border-border rounded-lg p-6 text-center hover:border-coral-pink transition-colors cursor-pointer">
                <input
                  type="file"
                  accept={
                    mediaType === 'image'
                      ? 'image/*'
                      : mediaType === 'video'
                        ? 'video/*'
                        : 'audio/*'
                  }
                  onChange={handleFileSelect}
                  className="hidden"
                  id="story-file"
                />
                <label htmlFor="story-file" className="cursor-pointer block">
                  <div className="text-3xl mb-2">
                    {mediaType === 'image' && '🖼️'}
                    {mediaType === 'video' && '🎥'}
                    {mediaType === 'audio' && '🎵'}
                  </div>
                  <p className="text-sm font-semibold text-foreground">
                    Click to upload {mediaType}
                  </p>
                  <p className="text-xs text-muted-foreground">or drag and drop</p>
                </label>
              </div>

              <Button
                onClick={() => setStep('select')}
                variant="outline"
                className="w-full"
              >
                Back
              </Button>
            </div>
          )}

          {step === 'details' && (
            <div className="space-y-4">
              {/* Preview */}
              <div className="relative w-full aspect-[9/16] bg-black rounded-lg overflow-hidden">
                {mediaType === 'image' && preview && (
                  <img src={preview} alt="Preview" className="w-full h-full object-cover" />
                )}
                {mediaType === 'video' && preview && (
                  <video src={preview} className="w-full h-full object-cover" />
                )}
                {mediaType === 'audio' && (
                  <div className="w-full h-full flex flex-col items-center justify-center bg-gradient-to-br from-coral-pink to-lavender">
                    <div className="text-6xl">🎵</div>
                  </div>
                )}
              </div>

              {/* Caption */}
              <textarea
                value={caption}
                onChange={(e) => setCaption(e.target.value)}
                placeholder="Add a caption... (optional)"
                rows={3}
                className="w-full px-3 py-2 rounded-lg bg-bg-secondary border border-border text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
              />

              {/* Duration */}
              {mediaType !== 'video' && (
                <div>
                  <label className="text-sm font-semibold text-foreground block mb-2">
                    Duration: {duration}s
                  </label>
                  <input
                    type="range"
                    min="1"
                    max="60"
                    value={duration}
                    onChange={(e) => setDuration(parseInt(e.target.value))}
                    className="w-full"
                  />
                </div>
              )}

              {/* Visualization for audio */}
              {mediaType === 'audio' && (
                <div>
                  <label className="text-sm font-semibold text-foreground block mb-2">
                    Visualization
                  </label>
                  <select
                    value={visualizationType}
                    onChange={(e) =>
                      setVisualizationType(
                        e.target.value as 'circular' | 'piano-roll' | 'waveform' | 'note-waterfall'
                      )
                    }
                    className="w-full px-3 py-2 rounded-lg bg-bg-secondary border border-border text-foreground text-sm"
                  >
                    <option value="circular">Circular</option>
                    <option value="piano-roll">Piano Roll</option>
                    <option value="waveform">Waveform</option>
                    <option value="note-waterfall">Note Waterfall</option>
                  </select>
                </div>
              )}

              {/* Error */}
              {error && (
                <div className="text-sm text-red-400 bg-red-500/10 p-3 rounded">
                  {error}
                </div>
              )}

              {/* Actions */}
              <div className="flex gap-2">
                <Button
                  onClick={handleCreate}
                  disabled={isUploading || !file}
                  className="flex-1 bg-coral-pink hover:bg-coral-pink/90 text-white"
                >
                  {isUploading ? <Spinner size="sm" /> : 'Share Story'}
                </Button>
                <Button
                  onClick={() => setStep('select')}
                  disabled={isUploading}
                  variant="outline"
                  className="flex-1"
                >
                  Back
                </Button>
              </div>
            </div>
          )}
        </div>
      </DialogContent>
    </Dialog>
  )
}
