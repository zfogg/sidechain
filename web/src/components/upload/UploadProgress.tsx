import { Spinner } from '@/components/ui/spinner'

interface UploadProgressProps {
  progress: number // 0-100
}

/**
 * UploadProgress - Visual progress indicator during file upload
 */
export function UploadProgress({ progress }: UploadProgressProps) {
  return (
    <div className="bg-card border border-border/50 rounded-2xl p-8 space-y-4">
      <div className="flex items-center gap-3 mb-4">
        <Spinner size="md" />
        <h3 className="text-lg font-semibold text-foreground">Uploading Your Loop</h3>
      </div>

      {/* Progress Bar */}
      <div className="space-y-2">
        <div className="h-2 bg-bg-secondary/50 rounded-full overflow-hidden border border-border/30">
          <div
            className="h-full bg-gradient-to-r from-coral-pink to-rose-pink transition-all duration-300"
            style={{ width: `${progress}%` }}
          />
        </div>

        {/* Progress Text */}
        <div className="flex justify-between text-sm">
          <span className="text-muted-foreground">Processing...</span>
          <span className="font-semibold text-foreground">{Math.round(progress)}%</span>
        </div>
      </div>

      {/* Status Message */}
      <div className="text-sm text-muted-foreground text-center">
        {progress < 25 && 'Preparing upload...'}
        {progress >= 25 && progress < 50 && 'Uploading file...'}
        {progress >= 50 && progress < 75 && 'Processing audio...'}
        {progress >= 75 && progress < 100 && 'Finalizing...'}
        {progress === 100 && 'Complete!'}
      </div>
    </div>
  )
}
