import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Button } from '@/components/ui/button'
import { FileUploadZone } from '@/components/upload/FileUploadZone'
import { MetadataForm } from '@/components/upload/MetadataForm'
import { UploadProgress } from '@/components/upload/UploadProgress'
import { useUploadMutation } from '@/hooks/mutations/useUploadMutation'

interface UploadFormData {
  file: File | null
  title: string
  description: string
  bpm: number | null
  key: string
  daw: string
  genre: string[]
  isPublic: boolean
}

const initialFormData: UploadFormData = {
  file: null,
  title: '',
  description: '',
  bpm: null,
  key: '',
  daw: '',
  genre: [],
  isPublic: true,
}

/**
 * Upload - Page for uploading and sharing loops
 *
 * Features:
 * - Drag-and-drop file upload
 * - Metadata form (title, BPM, key, DAW, genre)
 * - Upload progress tracking
 * - Waveform generation
 * - Draft saving to localStorage
 */
export function Upload() {
  const navigate = useNavigate()
  const [formData, setFormData] = useState<UploadFormData>(initialFormData)
  const [preview, setPreview] = useState<string>('')
  const [errors, setErrors] = useState<Record<string, string>>({})

  const { mutate: uploadPost, isPending, progress } = useUploadMutation()

  const handleFileSelect = (file: File) => {
    // Validate file type
    const validTypes = ['audio/mpeg', 'audio/wav', 'audio/ogg', 'audio/flac', 'audio/aac']
    if (!validTypes.includes(file.type)) {
      setErrors({ file: 'Invalid file type. Please upload MP3, WAV, OGG, FLAC, or AAC.' })
      return
    }

    // Validate file size (max 50MB)
    if (file.size > 50 * 1024 * 1024) {
      setErrors({ file: 'File is too large. Max size is 50MB.' })
      return
    }

    setFormData((prev) => ({
      ...prev,
      file,
      title: file.name.replace(/\.[^/.]+$/, ''), // Remove extension
    }))
    setErrors({})

    // Create preview (for potential waveform visualization)
    const url = URL.createObjectURL(file)
    setPreview(url)

    // Save draft to localStorage
    saveDraft({
      ...formData,
      file: null, // Don't store file in localStorage
    })
  }

  const handleMetadataChange = (
    field: keyof Omit<UploadFormData, 'file'>,
    value: any
  ) => {
    setFormData((prev) => ({
      ...prev,
      [field]: value,
    }))
    saveDraft(formData)
  }

  const saveDraft = (data: UploadFormData) => {
    const draft = {
      title: data.title,
      description: data.description,
      bpm: data.bpm,
      key: data.key,
      daw: data.daw,
      genre: data.genre,
      isPublic: data.isPublic,
      savedAt: new Date().toISOString(),
    }
    localStorage.setItem('uploadDraft', JSON.stringify(draft))
  }

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()

    // Validation
    const newErrors: Record<string, string> = {}
    if (!formData.file) newErrors.file = 'Please select a file'
    if (!formData.title.trim()) newErrors.title = 'Title is required'
    if (formData.genre.length === 0) newErrors.genre = 'Please select at least one genre'

    if (Object.keys(newErrors).length > 0) {
      setErrors(newErrors)
      return
    }

    uploadPost(formData, {
      onSuccess: () => {
        // Clear draft
        localStorage.removeItem('uploadDraft')
        // Redirect to feed
        navigate('/feed')
      },
      onError: (error: any) => {
        setErrors({ submit: error.message || 'Upload failed. Please try again.' })
      },
    })
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary p-8">
      <div className="max-w-2xl mx-auto">
        {/* Header */}
        <div className="mb-8">
          <h1 className="text-4xl font-bold text-foreground mb-2">Upload Your Loop</h1>
          <p className="text-muted-foreground">Share your sounds with the community</p>
        </div>

        <form onSubmit={handleSubmit} className="space-y-8">
          {/* File Upload */}
          <div>
            <h2 className="text-xl font-bold text-foreground mb-4">Choose Your File</h2>
            <FileUploadZone
              onFileSelect={handleFileSelect}
              selectedFile={formData.file}
              error={errors.file}
            />
          </div>

          {/* Metadata Form */}
          {formData.file && (
            <div>
              <h2 className="text-xl font-bold text-foreground mb-4">Loop Details</h2>
              <MetadataForm
                formData={formData}
                onChange={handleMetadataChange}
                errors={errors}
              />
            </div>
          )}

          {/* Upload Progress */}
          {isPending && <UploadProgress progress={progress} />}

          {/* Error Message */}
          {errors.submit && (
            <div className="p-4 rounded-lg bg-red-500/10 border border-red-500/30 text-red-400 text-sm">
              {errors.submit}
            </div>
          )}

          {/* Submit Buttons */}
          <div className="flex gap-4 pt-4">
            <Button
              type="button"
              variant="outline"
              onClick={() => {
                setFormData(initialFormData)
                setPreview('')
                setErrors({})
              }}
              className="flex-1 h-11 border-border/50 text-foreground text-sm font-semibold"
              disabled={isPending}
            >
              Cancel
            </Button>
            <Button
              type="submit"
              disabled={!formData.file || isPending}
              className="flex-1 h-11 bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white text-sm font-semibold transition-all duration-200"
            >
              {isPending ? `Uploading... ${Math.round(progress)}%` : 'Upload & Share'}
            </Button>
          </div>
        </form>
      </div>
    </div>
  )
}
