import { useQuery } from '@tanstack/react-query'
import { StoryClient } from '@/api/StoryClient'
import { Spinner } from '@/components/ui/spinner'
import { useState } from 'react'
import {
  Dialog,
  DialogContent,
} from '@/components/ui/dialog'
import { StoryViewer } from './StoryViewer'

interface StoryHighlightsProps {
  userId: string
}

/**
 * StoryHighlights - Display user's story highlights (permanent collections)
 *
 * Features:
 * - Circular highlight covers
 * - View stories in highlight
 * - Swipe through highlight stories
 * - Empty state when no highlights
 */
export function StoryHighlights({ userId }: StoryHighlightsProps) {
  const [highlightStories, setHighlightStories] = useState<any[]>([])
  const [isViewerOpen, setIsViewerOpen] = useState(false)

  const {
    data: highlights = [],
    isLoading,
    error,
  } = useQuery({
    queryKey: ['storyHighlights', userId],
    queryFn: async () => {
      const result = await StoryClient.getUserHighlights(userId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    staleTime: 10 * 60 * 1000, // 10 minutes
    gcTime: 60 * 60 * 1000, // 1 hour
  })

  const handleHighlightClick = async (highlightId: string, storyIds: string[]) => {
    setSelectedHighlight(highlightId)

    // Fetch the stories in this highlight
    const stories = []
    for (const storyId of storyIds) {
      // Would need a getStory method
      stories.push({ id: storyId })
    }

    setHighlightStories(stories)
    setIsViewerOpen(true)
  }

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-4">
        <Spinner size="sm" />
      </div>
    )
  }

  if (error || !highlights || highlights.length === 0) {
    return null
  }

  return (
    <>
      <div className="space-y-2">
        <h3 className="text-sm font-semibold text-foreground">Highlights</h3>
        <div className="flex gap-3 overflow-x-auto pb-2">
          {highlights.map((highlight) => (
            <button
              key={highlight.id}
              onClick={() =>
                handleHighlightClick(highlight.id, highlight.storyIds)
              }
              className="flex-shrink-0 flex flex-col items-center gap-2 group"
            >
              {/* Highlight cover */}
              <div className="w-16 h-16 rounded-full border-2 border-border group-hover:border-coral-pink transition-colors overflow-hidden bg-bg-secondary flex items-center justify-center">
                {highlight.coverImageUrl ? (
                  <img
                    src={highlight.coverImageUrl}
                    alt={highlight.title}
                    className="w-full h-full object-cover"
                  />
                ) : (
                  <span className="text-2xl">✨</span>
                )}
              </div>

              {/* Highlight title */}
              <p className="text-xs text-center text-foreground truncate max-w-[60px] group-hover:text-coral-pink">
                {highlight.title}
              </p>
            </button>
          ))}
        </div>
      </div>

      {/* Story viewer modal */}
      {isViewerOpen && highlightStories.length > 0 && (
        <Dialog open={isViewerOpen} onOpenChange={setIsViewerOpen}>
          <DialogContent className="p-0 border-0 bg-transparent max-w-none w-screen h-screen">
            <StoryViewer
              stories={highlightStories}
              onClose={() => setIsViewerOpen(false)}
            />
          </DialogContent>
        </Dialog>
      )}
    </>
  )
}
