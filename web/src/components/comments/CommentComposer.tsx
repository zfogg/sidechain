import { useState } from 'react'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { useUserStore } from '@/stores/useUserStore'
import { useCreateCommentMutation } from '@/hooks/mutations/useCommentMutations'

interface CommentComposerProps {
  postId: string
  parentId?: string
  onCommentCreated?: () => void
  autoFocus?: boolean
  placeholder?: string
}

/**
 * CommentComposer - Form for creating new comments or replies
 */
export function CommentComposer({
  postId,
  parentId,
  onCommentCreated,
  autoFocus = false,
  placeholder = 'Add a comment...',
}: CommentComposerProps) {
  const [content, setContent] = useState('')
  const { user } = useUserStore()
  const { mutate: createComment, isPending } = useCreateCommentMutation()

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()

    if (!content.trim()) return

    createComment(
      { postId, content: content.trim(), parentId },
      {
        onSuccess: () => {
          setContent('')
          onCommentCreated?.()
        },
      }
    )
  }

  if (!user) {
    return (
      <div className="text-center py-4 text-muted-foreground text-sm">
        Sign in to comment
      </div>
    )
  }

  return (
    <form onSubmit={handleSubmit} className="flex gap-3 py-3">
      {/* User Avatar */}
      <img
        src={user.profilePictureUrl || 'https://api.dicebear.com/7.x/avataaars/svg?seed=' + user.id}
        alt={user.displayName}
        className="w-8 h-8 rounded-full bg-muted flex-shrink-0"
      />

      {/* Comment Input */}
      <div className="flex-1 flex gap-2">
        <input
          type="text"
          value={content}
          onChange={(e) => setContent(e.target.value)}
          placeholder={placeholder}
          autoFocus={autoFocus}
          className="flex-1 px-3 py-2 rounded-lg bg-muted border border-border focus:border-primary focus:outline-none text-sm transition-colors"
          disabled={isPending}
        />
        <Button
          type="submit"
          size="sm"
          disabled={!content.trim() || isPending}
          className="gap-2"
        >
          {isPending ? (
            <>
              <Spinner size="sm" />
              <span className="hidden sm:inline">Posting...</span>
            </>
          ) : (
            <>
              <svg
                className="w-4 h-4"
                fill="currentColor"
                viewBox="0 0 24 24"
              >
                <path d="M16.6915026,12.4744748 L3.50612381,13.2599618 C3.19218622,13.2599618 3.03521743,13.4170592 3.03521743,13.5741566 L1.15159189,20.0151496 C0.8376543,20.8006365 0.99,21.89 1.77946707,22.52 C2.41,22.99 3.50612381,23.1 4.13399899,22.99 L21.714504,14.0454487 C22.6563168,13.5741566 23.1272231,12.6315722 22.9702544,11.6889879 L4.13399899,1.01036745 C3.34915502,0.9 2.40734225,1.00636533 1.77946707,1.4776575 C0.994623095,2.10604706 0.837654326,3.0486314 1.15159189,3.98 L3.03521743,10.4210 C3.03521743,10.5780974 3.19218622,10.7351948 3.50612381,10.7351948 L16.6915026,11.5206817 C16.6915026,11.5206817 17.1624089,11.5206817 17.1624089,12 C17.1624089,12.4744748 16.6915026,12.4744748 16.6915026,12.4744748 Z" />
              </svg>
              <span className="hidden sm:inline">Reply</span>
            </>
          )}
        </Button>
      </div>
    </form>
  )
}
