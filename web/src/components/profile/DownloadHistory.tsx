import { useQuery } from '@tanstack/react-query'
import { DownloadClient } from '@/api/DownloadClient'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { formatTime } from '@/utils/format'

/**
 * DownloadHistory - Display user's download history
 *
 * Features:
 * - Shows recent downloads with timestamps
 * - Displays file type (Audio, MIDI, Project)
 * - Paginated view with load more
 * - Empty state when no downloads
 * - Loading state
 */
export function DownloadHistory() {
  const {
    data: downloads = [],
    isLoading,
    error,
    hasNextPage,
    fetchNextPage,
    isFetchingNextPage,
  } = useQuery<any>({
    queryKey: ['downloadHistory'],
    queryFn: async () => {
      const result = await DownloadClient.getDownloadHistory(20, 0)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    staleTime: 5 * 60 * 1000, // 5 minutes
    gcTime: 30 * 60 * 1000, // 30 minutes
  })

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-8">
        <Spinner size="md" />
      </div>
    )
  }

  if (error) {
    return (
      <div className="text-center py-8 text-muted-foreground">
        Failed to load download history
      </div>
    )
  }

  if (!downloads || downloads.length === 0) {
    return (
      <div className="text-center py-8 text-muted-foreground">
        No downloads yet
      </div>
    )
  }

  const getFileTypeIcon = (type: string) => {
    switch (type) {
      case 'audio':
        return '🎵'
      case 'midi':
        return '🎹'
      case 'project':
        return '📦'
      default:
        return '📥'
    }
  }

  const getFileTypeLabel = (type: string) => {
    switch (type) {
      case 'audio':
        return 'Audio File'
      case 'midi':
        return 'MIDI File'
      case 'project':
        return 'Project Files'
      default:
        return 'File'
    }
  }

  return (
    <div className="space-y-3">
      <h3 className="text-lg font-semibold text-foreground mb-4">Download History</h3>

      {downloads.map((download: any, index: number) => (
        <div
          key={`${download.postId}-${download.downloadedAt}-${index}`}
          className="flex items-center gap-3 bg-card border border-border rounded-lg p-3 hover:border-coral-pink/50 transition-colors"
        >
          {/* File Type Icon */}
          <div className="text-2xl flex-shrink-0">
            {getFileTypeIcon(download.downloadType)}
          </div>

          {/* Download Info */}
          <div className="flex-1 min-w-0">
            <p className="font-medium text-sm text-foreground truncate">
              {download.filename}
            </p>
            <p className="text-xs text-muted-foreground">
              {getFileTypeLabel(download.downloadType)}
            </p>
          </div>

          {/* Download Date */}
          <div className="text-xs text-muted-foreground flex-shrink-0 text-right">
            <p>{new Date(download.downloadedAt).toLocaleDateString()}</p>
            <p>{new Date(download.downloadedAt).toLocaleTimeString()}</p>
          </div>
        </div>
      ))}

      {hasNextPage && (
        <Button
          onClick={() => fetchNextPage()}
          disabled={isFetchingNextPage}
          variant="outline"
          className="w-full"
        >
          {isFetchingNextPage ? 'Loading...' : 'Load More'}
        </Button>
      )}
    </div>
  )
}
