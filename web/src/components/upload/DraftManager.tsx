import { useState, useEffect } from 'react'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from '@/components/ui/dialog'

export interface Draft {
  id: string
  title: string
  description: string
  bpm: number | null
  key: string
  daw: string
  genre: string[]
  isPublic: boolean
  savedAt: Date
}

interface DraftManagerProps {
  onLoadDraft: (draft: Draft) => void
}

/**
 * DraftManager - Manage saved upload drafts
 *
 * Features:
 * - List all saved drafts
 * - Load draft into editor
 * - Delete drafts
 * - Show last saved time
 */
export function DraftManager({ onLoadDraft }: DraftManagerProps) {
  const [open, setOpen] = useState(false)
  const [drafts, setDrafts] = useState<Draft[]>([])

  useEffect(() => {
    loadDrafts()
  }, [open])

  const loadDrafts = () => {
    const storedDrafts = localStorage.getItem('uploadDrafts')
    if (storedDrafts) {
      try {
        const parsed = JSON.parse(storedDrafts)
        const withDates = parsed.map((d: any) => ({
          ...d,
          savedAt: new Date(d.savedAt),
        }))
        setDrafts(withDates)
      } catch {
        setDrafts([])
      }
    }
  }

  const handleLoadDraft = (draft: Draft) => {
    onLoadDraft(draft)
    setOpen(false)
  }

  const handleDeleteDraft = (id: string) => {
    const updated = drafts.filter((d) => d.id !== id)
    if (updated.length === 0) {
      localStorage.removeItem('uploadDrafts')
    } else {
      localStorage.setItem('uploadDrafts', JSON.stringify(updated))
    }
    setDrafts(updated)
  }

  const handleDeleteAll = () => {
    if (confirm('Delete all drafts? This cannot be undone.')) {
      localStorage.removeItem('uploadDrafts')
      setDrafts([])
    }
  }

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button variant="outline" className="gap-2">
          <span>📝</span> My Drafts ({drafts.length})
        </Button>
      </DialogTrigger>

      <DialogContent className="max-w-md max-h-96 overflow-y-auto">
        <DialogHeader>
          <DialogTitle>Saved Drafts</DialogTitle>
          <DialogDescription>
            Load a previous upload draft or start fresh
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-2">
          {drafts.length === 0 ? (
            <p className="text-sm text-muted-foreground text-center py-4">No drafts saved yet</p>
          ) : (
            <>
              {drafts.map((draft) => (
                <div
                  key={draft.id}
                  className="p-3 bg-bg-secondary rounded border border-border hover:border-coral-pink/30 transition-colors"
                >
                  <div className="flex items-start justify-between gap-2 mb-2">
                    <div className="flex-1 min-w-0">
                      <h3 className="font-semibold text-foreground truncate">{draft.title}</h3>
                      <p className="text-xs text-muted-foreground mt-1">
                        Saved {draft.savedAt.toLocaleDateString()}
                      </p>
                    </div>
                  </div>

                  <div className="text-xs text-muted-foreground mb-2 space-y-1">
                    {draft.bpm && <div>BPM: {draft.bpm}</div>}
                    {draft.daw && <div>DAW: {draft.daw}</div>}
                    {draft.genre.length > 0 && <div>Genre: {draft.genre.join(', ')}</div>}
                  </div>

                  <div className="flex gap-2">
                    <Button
                      onClick={() => handleLoadDraft(draft)}
                      className="flex-1 bg-coral-pink hover:bg-coral-pink/90 text-white text-sm h-8"
                    >
                      Load
                    </Button>
                    <Button
                      onClick={() => handleDeleteDraft(draft.id)}
                      variant="outline"
                      className="text-sm h-8 px-2"
                      title="Delete draft"
                    >
                      ✕
                    </Button>
                  </div>
                </div>
              ))}

              <Button
                onClick={handleDeleteAll}
                variant="outline"
                className="w-full text-red-500 hover:text-red-600"
              >
                Delete All
              </Button>
            </>
          )}
        </div>
      </DialogContent>
    </Dialog>
  )
}

/**
 * Save upload draft to localStorage
 */
export function saveDraft(data: Omit<Draft, 'id' | 'savedAt'>): Draft {
  const draft: Draft = {
    id: Math.random().toString(36).substring(7),
    ...data,
    savedAt: new Date(),
  }

  const existing = localStorage.getItem('uploadDrafts')
  const drafts = existing ? JSON.parse(existing) : []
  drafts.push(draft)
  localStorage.setItem('uploadDrafts', JSON.stringify(drafts))

  return draft
}
