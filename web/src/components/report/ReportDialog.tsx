import { useState } from 'react'
import { ReportClient, ReportType, ReportReason } from '@/api/ReportClient'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { Textarea } from '@/components/ui/textarea'
import { Spinner } from '@/components/ui/spinner'

interface ReportDialogProps {
  type: ReportType
  targetId: string
  targetName?: string
  isOpen: boolean
  onOpenChange: (open: boolean) => void
}

/**
 * ReportDialog - Modal for reporting posts, comments, or users
 */
export function ReportDialog({
  type,
  targetId,
  targetName,
  isOpen,
  onOpenChange,
}: ReportDialogProps) {
  const [selectedReason, setSelectedReason] = useState<ReportReason | null>(null)
  const [description, setDescription] = useState('')
  const [isSubmitting, setIsSubmitting] = useState(false)
  const [submitted, setSubmitted] = useState(false)
  const [error, setError] = useState<string | null>(null)

  const handleSubmit = async () => {
    if (!selectedReason) {
      setError('Please select a reason')
      return
    }

    setIsSubmitting(true)
    setError(null)

    try {
      const result = await ReportClient.submitReport(
        type,
        targetId,
        selectedReason,
        description
      )

      if (result.isOk()) {
        setSubmitted(true)
        setTimeout(() => {
          onOpenChange(false)
          // Reset state
          setSelectedReason(null)
          setDescription('')
          setSubmitted(false)
        }, 2000)
      } else {
        setError(result.getError())
      }
    } finally {
      setIsSubmitting(false)
    }
  }

  const reasons = ReportClient.getReportReasons()
  const typeLabel = type === 'post' ? 'Post' : type === 'comment' ? 'Comment' : 'User'

  return (
    <Dialog open={isOpen} onOpenChange={onOpenChange}>
      <DialogContent className="sm:max-w-md">
        <DialogHeader>
          <DialogTitle>Report {typeLabel}</DialogTitle>
          <DialogDescription>
            Help us improve Sidechain by reporting content that violates our community guidelines
            {targetName && ` (${targetName})`}
          </DialogDescription>
        </DialogHeader>

        {submitted ? (
          <div className="space-y-4 py-6">
            <div className="text-center space-y-2">
              <div className="text-4xl">✅</div>
              <div className="font-semibold text-foreground">Report Submitted</div>
              <p className="text-sm text-muted-foreground">
                Thank you for helping us maintain a safe community. Our team will review your
                report.
              </p>
            </div>
          </div>
        ) : (
          <div className="space-y-4">
            {/* Reason Selection */}
            <div className="space-y-2">
              <label className="text-sm font-medium text-foreground">Why are you reporting this?</label>
              <div className="space-y-2 max-h-64 overflow-y-auto">
                {reasons.map((reason) => (
                  <label
                    key={reason.value}
                    className="flex items-start gap-3 p-3 rounded-lg border border-border cursor-pointer hover:bg-bg-secondary transition-colors"
                  >
                    <input
                      type="radio"
                      name="reason"
                      value={reason.value}
                      checked={selectedReason === reason.value}
                      onChange={(e) => setSelectedReason(e.target.value as ReportReason)}
                      className="w-4 h-4 mt-1 flex-shrink-0 accent-coral-pink"
                    />
                    <div className="flex-1 min-w-0">
                      <div className="font-medium text-foreground text-sm">{reason.label}</div>
                      <div className="text-xs text-muted-foreground">{reason.description}</div>
                    </div>
                  </label>
                ))}
              </div>
            </div>

            {/* Additional Details */}
            {selectedReason && (
              <div className="space-y-2">
                <label className="text-sm font-medium text-foreground">
                  Additional details (optional)
                </label>
                <Textarea
                  placeholder="Please provide more information about why you're reporting this..."
                  value={description}
                  onChange={(e) => setDescription(e.target.value)}
                  className="min-h-20 resize-none"
                  maxLength={500}
                  disabled={isSubmitting}
                />
                <div className="text-xs text-muted-foreground">
                  {description.length}/500 characters
                </div>
              </div>
            )}

            {/* Error Message */}
            {error && (
              <div className="bg-destructive/10 border border-destructive/30 rounded-lg p-3">
                <div className="text-sm text-destructive">{error}</div>
              </div>
            )}

            {/* Actions */}
            <div className="flex gap-2 justify-end pt-2">
              <Button
                variant="outline"
                onClick={() => onOpenChange(false)}
                disabled={isSubmitting}
              >
                Cancel
              </Button>
              <Button
                onClick={handleSubmit}
                disabled={!selectedReason || isSubmitting}
                className="gap-2"
              >
                {isSubmitting && <Spinner size="sm" />}
                Submit Report
              </Button>
            </div>
          </div>
        )}
      </DialogContent>
    </Dialog>
  )
}
