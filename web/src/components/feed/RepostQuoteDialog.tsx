import { useState } from 'react'
import { SocialClient } from '@/api/SocialClient'
import { Button } from '@/components/ui/button'
import { Dialog, DialogContent, DialogDescription, DialogHeader, DialogTitle } from '@/components/ui/dialog'
import { Textarea } from '@/components/ui/textarea'
import { Spinner } from '@/components/ui/spinner'

interface RepostQuoteDialogProps {
  postId: string
  isOpen: boolean
  onClose: () => void
  onSuccess?: () => void
}

export function RepostQuoteDialog({ postId, isOpen, onClose, onSuccess }: RepostQuoteDialogProps) {
  const [quote, setQuote] = useState('')
  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState('')

  const handleRepost = async () => {
    if (quote.trim().length === 0) {
      setError('Quote cannot be empty')
      return
    }

    setIsLoading(true)
    setError('')

    const result = await SocialClient.repostWithQuote(postId, quote.trim())

    if (result.isOk()) {
      setQuote('')
      onSuccess?.()
      onClose()
    } else {
      setError(result.getError())
    }

    setIsLoading(false)
  }

  const handleClose = () => {
    setQuote('')
    setError('')
    onClose()
  }

  return (
    <Dialog open={isOpen} onOpenChange={handleClose}>
      <DialogContent className="sm:max-w-md">
        <DialogHeader>
          <DialogTitle>Repost with Quote</DialogTitle>
          <DialogDescription>Add your thoughts to this post</DialogDescription>
        </DialogHeader>

        <div className="space-y-4 py-4">
          <Textarea
            placeholder="What do you think about this post?"
            value={quote}
            onChange={(e) => {
              setQuote(e.target.value)
              setError('')
            }}
            className="min-h-24 resize-none"
            disabled={isLoading}
            maxLength={500}
          />

          <div className="text-xs text-muted-foreground">
            {quote.length}/500
          </div>

          {error && (
            <div className="text-sm text-red-400 bg-red-500/10 p-3 rounded">{error}</div>
          )}

          <div className="flex gap-2">
            <Button
              onClick={handleRepost}
              disabled={isLoading || quote.trim().length === 0}
              className="flex-1"
            >
              {isLoading ? <Spinner size="sm" className="mr-2" /> : null}
              Repost
            </Button>

            <Button variant="outline" onClick={handleClose} disabled={isLoading} className="flex-1">
              Cancel
            </Button>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  )
}
