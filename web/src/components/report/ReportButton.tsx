import { useState } from 'react'
import { ReportDialog } from './ReportDialog'
import { ReportType } from '@/api/ReportClient'
import { Button } from '@/components/ui/button'

interface ReportButtonProps {
  type: ReportType
  targetId: string
  targetName?: string
  variant?: 'default' | 'ghost' | 'outline'
  size?: 'sm' | 'lg' | 'default'
  showLabel?: boolean
}

/**
 * ReportButton - Button that opens report dialog
 * Used in posts, comments, and user cards
 */
export function ReportButton({
  type,
  targetId,
  targetName,
  variant = 'ghost',
  size = 'sm',
  showLabel = false,
}: ReportButtonProps) {
  const [isDialogOpen, setIsDialogOpen] = useState(false)

  return (
    <>
      <Button
        variant={variant}
        size={size}
        onClick={() => setIsDialogOpen(true)}
        className="gap-1 text-muted-foreground hover:text-red-400"
        title={`Report this ${type}`}
      >
        <span>🚩</span>
        {showLabel && <span className="text-xs">Report</span>}
      </Button>

      <ReportDialog
        type={type}
        targetId={targetId}
        targetName={targetName}
        isOpen={isDialogOpen}
        onOpenChange={setIsDialogOpen}
      />
    </>
  )
}
