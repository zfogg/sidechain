import { useEffect, useState, useRef } from 'react'
import { Ephemeral } from '@/models/Ephemeral'
import { StoryClient } from '@/api/StoryClient'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import './StoryViewer.css'

interface StoryViewerProps {
  stories: Ephemeral[]
  initialIndex?: number
  onClose: () => void
  onNext?: () => void
  onPrevious?: () => void
}

/**
 * StoryViewer - View and interact with user stories
 *
 * Features:
 * - Swipe navigation between stories
 * - Auto-advance after story duration
 * - Progress bar
 * - Viewer count and list
 * - Like functionality
 * - Responsive to touch and keyboard
 */
export function StoryViewer({
  stories,
  initialIndex = 0,
  onClose,
  onNext,
  onPrevious,
}: StoryViewerProps) {
  const [currentIndex, setCurrentIndex] = useState(initialIndex)
  const [progress, setProgress] = useState(0)
  const [isLiked, setIsLiked] = useState(false)
  const [viewers, setViewers] = useState<any[]>([])
  const [showViewers, setShowViewers] = useState(false)
  const [isLoading, setIsLoading] = useState(false)
  const [touchStart, setTouchStart] = useState<number | null>(null)
  const [touchEnd, setTouchEnd] = useState<number | null>(null)

  const progressInterval = useRef<NodeJS.Timeout | null>(null)
  const storyVideoRef = useRef<HTMLVideoElement | null>(null)

  const currentStory = stories[currentIndex]

  // Mark story as viewed on load
  useEffect(() => {
    const markViewed = async () => {
      await StoryClient.viewStory(currentStory.id)
    }
    markViewed()
  }, [currentStory.id])

  // Progress bar animation
  useEffect(() => {
    const duration = currentStory.duration || 5 // default 5 seconds
    const interval = setInterval(() => {
      setProgress((prev) => {
        if (prev >= 100) {
          handleNext()
          return 0
        }
        return prev + (100 / (duration * 10)) // 100ms ticks
      })
    }, 100)

    progressInterval.current = interval

    return () => clearInterval(interval)
  }, [currentStory.id, currentStory.duration])

  const handleNext = () => {
    if (currentIndex < stories.length - 1) {
      setCurrentIndex(currentIndex + 1)
      setProgress(0)
      onNext?.()
    } else {
      onClose()
    }
  }

  const handlePrevious = () => {
    if (currentIndex > 0) {
      setCurrentIndex(currentIndex - 1)
      setProgress(0)
      onPrevious?.()
    }
  }

  const handleLike = async () => {
    setIsLiked(!isLiked)
    await StoryClient.likeStory(currentStory.id, !isLiked)
  }

  const handleGetViewers = async () => {
    setIsLoading(true)
    const result = await StoryClient.getStoryViewers(currentStory.id)
    if (result.isOk()) {
      setViewers(result.getValue())
    }
    setIsLoading(false)
    setShowViewers(true)
  }

  // Swipe handling
  const handleTouchStart = (e: React.TouchEvent) => {
    setTouchStart(e.touches[0]?.clientX || 0)
  }

  const handleTouchEnd = (e: React.TouchEvent) => {
    setTouchEnd(e.changedTouches[0]?.clientX || 0)
  }

  useEffect(() => {
    if (!touchStart || !touchEnd) return

    const distance = touchStart - touchEnd
    const isLeftSwipe = distance > 50
    const isRightSwipe = distance < -50

    if (isLeftSwipe) {
      handleNext()
    } else if (isRightSwipe) {
      handlePrevious()
    }
  }, [touchStart, touchEnd])

  // Keyboard navigation
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'ArrowRight') handleNext()
      if (e.key === 'ArrowLeft') handlePrevious()
      if (e.key === 'Escape') onClose()
    }

    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [currentIndex])

  return (
    <div
      className="fixed inset-0 bg-black/95 flex items-center justify-center z-50 select-none"
      onTouchStart={handleTouchStart}
      onTouchEnd={handleTouchEnd}
    >
      {/* Close button */}
      <button
        onClick={onClose}
        className="absolute top-4 right-4 text-white hover:text-gray-300 z-10 text-2xl"
      >
        ✕
      </button>

      {/* Progress bars */}
      <div className="absolute top-0 left-0 right-0 flex gap-1 p-2">
        {stories.map((_, idx) => (
          <div
            key={idx}
            className="flex-1 h-1 bg-white/30 rounded overflow-hidden"
          >
            <div
              className="h-full bg-white transition-all duration-100"
              style={{
                width: idx === currentIndex ? `${progress}%` : idx < currentIndex ? '100%' : '0%',
              }}
            />
          </div>
        ))}
      </div>

      {/* Story content */}
      <div className="relative w-full max-w-md aspect-[9/16] bg-black rounded-lg overflow-hidden">
        {/* Media */}
        {currentStory.mediaType === 'image' && (
          <img
            src={currentStory.mediaUrl}
            alt={currentStory.caption}
            className="w-full h-full object-cover"
          />
        )}

        {currentStory.mediaType === 'video' && (
          <video
            ref={storyVideoRef}
            src={currentStory.mediaUrl}
            className="w-full h-full object-cover"
            autoPlay
            loop
          />
        )}

        {currentStory.mediaType === 'audio' && (
          <div className="w-full h-full flex flex-col items-center justify-center bg-gradient-to-br from-coral-pink to-lavender">
            <div className="text-6xl mb-4">🎵</div>
            <div className="text-center text-white">
              <p className="font-semibold text-lg">{currentStory.caption || 'Audio Story'}</p>
              <p className="text-sm opacity-75">
                {currentStory.duration}s
              </p>
            </div>
          </div>
        )}

        {/* User info header */}
        <div className="absolute top-12 left-4 right-4 flex items-center gap-3">
          <img
            src={
              currentStory.profilePictureUrl ||
              `https://api.dicebear.com/7.x/avataaars/svg?seed=${currentStory.userId}`
            }
            alt={currentStory.displayName}
            className="w-10 h-10 rounded-full border-2 border-white"
          />
          <div className="text-white">
            <p className="font-semibold text-sm">{currentStory.displayName}</p>
            <p className="text-xs opacity-75">@{currentStory.username}</p>
          </div>
        </div>

        {/* Story caption */}
        {currentStory.caption && (
          <div className="absolute bottom-16 left-4 right-4 text-white">
            <p className="text-sm">{currentStory.caption}</p>
          </div>
        )}

        {/* Actions footer */}
        <div className="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/80 to-transparent p-4 flex gap-2">
          <Button
            size="sm"
            onClick={handleLike}
            className="flex-1 gap-2"
            variant={isLiked ? 'default' : 'outline'}
          >
            <span>{isLiked ? '❤️' : '🤍'}</span>
            <span className="text-xs">{currentStory.likeCount}</span>
          </Button>

          <Button
            size="sm"
            onClick={handleGetViewers}
            disabled={isLoading}
            variant="outline"
            className="flex-1"
          >
            <span className="text-xs">
              {isLoading ? <Spinner size="sm" /> : `👁️ ${currentStory.viewCount}`}
            </span>
          </Button>
        </div>
      </div>

      {/* Navigation arrows */}
      <button
        onClick={handlePrevious}
        disabled={currentIndex === 0}
        className="absolute left-4 text-white hover:text-gray-300 disabled:opacity-30 text-3xl"
      >
        ‹
      </button>

      <button
        onClick={handleNext}
        disabled={currentIndex === stories.length - 1}
        className="absolute right-4 text-white hover:text-gray-300 disabled:opacity-30 text-3xl"
      >
        ›
      </button>

      {/* Viewers modal */}
      {showViewers && (
        <div className="absolute inset-0 bg-black/90 flex items-center justify-center">
          <div className="bg-card border border-border rounded-lg p-4 max-w-sm w-full mx-4 max-h-96 overflow-y-auto">
            <div className="flex justify-between items-center mb-4">
              <h3 className="font-semibold text-foreground">Story Viewers ({viewers.length})</h3>
              <button
                onClick={() => setShowViewers(false)}
                className="text-muted-foreground hover:text-foreground"
              >
                ✕
              </button>
            </div>

            <div className="space-y-2">
              {viewers.map((viewer) => (
                <div key={viewer.id} className="flex items-center gap-2 p-2 hover:bg-bg-secondary rounded">
                  <img
                    src={
                      viewer.profilePictureUrl ||
                      `https://api.dicebear.com/7.x/avataaars/svg?seed=${viewer.id}`
                    }
                    alt={viewer.displayName}
                    className="w-8 h-8 rounded-full"
                  />
                  <div className="flex-1 min-w-0">
                    <p className="text-sm font-medium text-foreground truncate">{viewer.displayName}</p>
                    <p className="text-xs text-muted-foreground">@{viewer.username}</p>
                  </div>
                  <p className="text-xs text-muted-foreground">
                    {new Date(viewer.viewedAt).toLocaleTimeString()}
                  </p>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
