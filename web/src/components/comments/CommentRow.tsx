import { useState } from 'react'
import type { Comment } from '@/models/Comment'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { useToggleCommentLikeMutation } from '@/hooks/mutations/useCommentMutations'

interface CommentRowProps {
  comment: Comment
  onReplyClick?: (commentId: string) => void
}

/**
 * CommentRow - Displays a single comment with like functionality
 */
export function CommentRow({ comment, onReplyClick }: CommentRowProps) {
  const { mutate: toggleLike, isPending } = useToggleCommentLikeMutation()

  const handleLike = () => {
    toggleLike({ commentId: comment.id, shouldLike: !comment.isLiked })
  }

  return (
    <div className="flex gap-3 py-3">
      {/* Avatar */}
      <img
        src={comment.userAvatarUrl || 'https://api.dicebear.com/7.x/avataaars/svg?seed=' + comment.userId}
        alt={comment.displayName}
        className="w-8 h-8 rounded-full bg-muted flex-shrink-0"
      />

      {/* Comment Content */}
      <div className="flex-1 min-w-0">
        {/* Header */}
        <div className="flex items-baseline gap-2 text-sm">
          <span className="font-medium">{comment.displayName}</span>
          <span className="text-muted-foreground text-xs">@{comment.username}</span>
          <span className="text-muted-foreground text-xs">
            {new Date(comment.createdAt).toLocaleDateString()}
          </span>
        </div>

        {/* Comment Text */}
        <p className="text-sm mt-1 text-foreground">{comment.content}</p>

        {/* Actions */}
        <div className="flex gap-4 mt-2 text-xs text-muted-foreground">
          <button
            onClick={handleLike}
            disabled={isPending}
            className="hover:text-foreground transition-colors flex items-center gap-1 disabled:opacity-50"
          >
            {isPending ? (
              <Spinner size="sm" />
            ) : (
              <svg
                className={`w-3 h-3 ${comment.isLiked ? 'text-coral-pink' : ''}`}
                fill={comment.isLiked ? 'currentColor' : 'none'}
                stroke="currentColor"
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M4.318 6.318a4.5 4.5 0 000 6.364L12 20.364l7.682-7.682a4.5 4.5 0 00-6.364-6.364L12 7.636l-1.318-1.318a4.5 4.5 0 00-6.364 0z"
                />
              </svg>
            )}
            <span>{comment.likeCount}</span>
          </button>

          <button
            onClick={() => onReplyClick?.(comment.id)}
            className="hover:text-foreground transition-colors"
          >
            Reply
          </button>
        </div>
      </div>
    </div>
  )
}
