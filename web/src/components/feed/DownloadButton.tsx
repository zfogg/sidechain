import { useState } from 'react'
import { DownloadClient, DownloadType } from '@/api/DownloadClient'
import { Button } from '@/components/ui/button'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'
import { Spinner } from '@/components/ui/spinner'

interface DownloadButtonProps {
  postId: string
  hasAudio?: boolean
  hasMidi?: boolean
  hasProject?: boolean
  variant?: 'default' | 'ghost' | 'outline'
  size?: 'default' | 'sm' | 'lg' | 'icon'
  className?: string
}

export function DownloadButton({
  postId,
  hasAudio = true,
  hasMidi = false,
  hasProject = false,
  variant = 'ghost',
  size = 'sm',
  className = '',
}: DownloadButtonProps) {
  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState('')
  const [loadingType, setLoadingType] = useState<DownloadType | null>(null)

  const downloadCount = [hasAudio, hasMidi, hasProject].filter(Boolean).length

  const handleDownload = async (type: DownloadType) => {
    setIsLoading(true)
    setLoadingType(type)
    setError('')

    try {
      let result
      switch (type) {
        case 'audio':
          result = await DownloadClient.getAudioDownloadUrl(postId)
          break
        case 'midi':
          result = await DownloadClient.getMidiDownloadUrl(postId)
          break
        case 'project':
          result = await DownloadClient.getProjectDownloadUrl(postId)
          break
      }

      if (result.isOk()) {
        const { url, filename } = result.getValue()
        DownloadClient.downloadFile(url, filename)
        // Track the download
        await DownloadClient.trackDownload(postId, type)
      } else {
        setError(result.getError())
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Download failed')
    } finally {
      setIsLoading(false)
      setLoadingType(null)
    }
  }

  // Show nothing if no downloads available
  if (downloadCount === 0) {
    return null
  }

  // Single download option
  if (downloadCount === 1) {
    const type = hasAudio ? 'audio' : hasMidi ? 'midi' : 'project'
    const label = hasAudio ? 'Audio' : hasMidi ? 'MIDI' : 'Project'

    return (
      <div className="flex flex-col items-center">
        <Button
          variant={variant}
          size={size}
          onClick={() => handleDownload(type as DownloadType)}
          disabled={isLoading}
          className={`gap-2 ${className}`}
          title={`Download ${label}`}
        >
          {isLoading && loadingType === type ? (
            <Spinner size="sm" />
          ) : (
            <span>⬇️</span>
          )}
          {size !== 'icon' && label}
        </Button>
        {error && <span className="text-xs text-red-400 mt-1">{error}</span>}
      </div>
    )
  }

  // Multiple download options
  return (
    <div className="flex flex-col items-center">
      <DropdownMenu>
        <DropdownMenuTrigger asChild>
          <Button
            variant={variant}
            size={size}
            disabled={isLoading}
            className={`gap-2 ${className}`}
            title="Download options"
          >
            {isLoading ? (
              <Spinner size="sm" />
            ) : (
              <span>⬇️</span>
            )}
            {size !== 'icon' && 'Download'}
          </Button>
        </DropdownMenuTrigger>

        <DropdownMenuContent align="end">
          {hasAudio && (
            <DropdownMenuItem
              onClick={() => handleDownload('audio')}
              disabled={isLoading}
            >
              <span className="flex items-center gap-2">
                {isLoading && loadingType === 'audio' ? <Spinner size="sm" /> : null}
                Audio File
              </span>
            </DropdownMenuItem>
          )}
          {hasMidi && (
            <DropdownMenuItem
              onClick={() => handleDownload('midi')}
              disabled={isLoading}
            >
              <span className="flex items-center gap-2">
                {isLoading && loadingType === 'midi' ? <Spinner size="sm" /> : null}
                MIDI File
              </span>
            </DropdownMenuItem>
          )}
          {hasProject && (
            <DropdownMenuItem
              onClick={() => handleDownload('project')}
              disabled={isLoading}
            >
              <span className="flex items-center gap-2">
                {isLoading && loadingType === 'project' ? <Spinner size="sm" /> : null}
                Project Files
              </span>
            </DropdownMenuItem>
          )}
        </DropdownMenuContent>
      </DropdownMenu>

      {error && <span className="text-xs text-red-400 mt-1">{error}</span>}
    </div>
  )
}
