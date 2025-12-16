import { useRef } from 'react'
import { Button } from '@/components/ui/button'

interface FileUploadZoneProps {
  onFileSelect: (file: File) => void
  selectedFile: File | null
  error?: string
}

/**
 * FileUploadZone - Drag-and-drop file upload with browser input fallback
 */
export function FileUploadZone({ onFileSelect, selectedFile, error }: FileUploadZoneProps) {
  const fileInputRef = useRef<HTMLInputElement>(null)
  const dragCounter = useRef(0)

  const handleDragEnter = (e: React.DragEvent) => {
    e.preventDefault()
    e.stopPropagation()
    dragCounter.current++
  }

  const handleDragLeave = (e: React.DragEvent) => {
    e.preventDefault()
    e.stopPropagation()
    dragCounter.current--
  }

  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault()
    e.stopPropagation()
  }

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault()
    e.stopPropagation()
    dragCounter.current = 0

    const files = e.dataTransfer.files
    if (files.length > 0) {
      onFileSelect(files[0])
    }
  }

  const handleFileInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.currentTarget.files
    if (files && files.length > 0) {
      onFileSelect(files[0])
    }
  }

  return (
    <div
      onDragEnter={handleDragEnter}
      onDragLeave={handleDragLeave}
      onDragOver={handleDragOver}
      onDrop={handleDrop}
      className={`
        relative border-2 border-dashed rounded-lg p-12 text-center transition-colors
        ${error ? 'border-red-500/50 bg-red-500/10' : 'border-border/50 bg-bg-secondary/50 hover:bg-bg-secondary/70'}
        ${dragCounter.current > 0 ? 'border-coral-pink bg-coral-pink/10' : ''}
      `}
    >
      <input
        ref={fileInputRef}
        type="file"
        accept="audio/*"
        onChange={handleFileInputChange}
        className="hidden"
      />

      {selectedFile ? (
        <div className="space-y-4">
          <div className="text-3xl">🎵</div>
          <div>
            <p className="font-semibold text-foreground text-lg">{selectedFile.name}</p>
            <p className="text-sm text-muted-foreground mt-1">
              {(selectedFile.size / 1024 / 1024).toFixed(2)} MB
            </p>
          </div>
          <Button
            type="button"
            variant="outline"
            size="sm"
            onClick={() => fileInputRef.current?.click()}
            className="border-border/50 text-foreground text-sm"
          >
            Change File
          </Button>
        </div>
      ) : (
        <div className="space-y-4">
          <div className="text-5xl">📁</div>
          <div>
            <p className="font-semibold text-foreground text-lg">Drag your loop here</p>
            <p className="text-sm text-muted-foreground mt-1">or</p>
          </div>
          <Button
            type="button"
            onClick={() => fileInputRef.current?.click()}
            className="bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white text-sm font-semibold"
          >
            Browse Files
          </Button>
          <p className="text-xs text-muted-foreground">
            Supports: MP3, WAV, OGG, FLAC, AAC • Max 50MB
          </p>
        </div>
      )}

      {error && (
        <div className="absolute -bottom-8 left-0 right-0 text-red-400 text-sm font-medium">
          {error}
        </div>
      )}
    </div>
  )
}
