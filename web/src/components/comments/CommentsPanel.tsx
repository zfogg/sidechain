import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { CommentsClient } from '@/api/CommentsClient'
import { CommentRow } from './CommentRow'
import { CommentComposer } from './CommentComposer'
import { Spinner } from '@/components/ui/spinner'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'

interface CommentsPanelProps {
  postId: string
  isOpen: boolean
  onOpenChange: (open: boolean) => void
}

/**
 * CommentsPanel - Dialog showing comments for a post with reply capability
 *
 * Features:
 * - Fetch and display paginated comments
 * - Create new comments
 * - Reply to comments (nested)
 * - Like comments
 * - Real-time updates via WebSocket
 */
export function CommentsPanel({
  postId,
  isOpen,
  onOpenChange,
}: CommentsPanelProps) {
  const [replyingTo, setReplyingTo] = useState<string | null>(null)

  // Fetch comments for this post
  const {
    data: comments,
    isLoading,
    error,
    refetch,
  } = useQuery({
    queryKey: ['comments', postId],
    queryFn: async () => {
      const result = await CommentsClient.getComments(postId, 100, 0)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    enabled: isOpen, // Only fetch when panel is open
    staleTime: 30 * 1000, // 30 seconds
    gcTime: 5 * 60 * 1000, // 5 minutes
  })

  const handleCommentCreated = () => {
    setReplyingTo(null)
    refetch()
  }

  // Separate comments into top-level and replies
  const topLevelComments = (comments || []).filter((c) => !c.parentId)
  const replies = (comments || []).reduce(
    (acc, c) => {
      if (c.parentId) {
        if (!acc[c.parentId]) acc[c.parentId] = []
        acc[c.parentId].push(c)
      }
      return acc
    },
    {} as Record<string, typeof comments>
  )

  return (
    <Dialog open={isOpen} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-md max-h-[80vh] flex flex-col">
        <DialogHeader>
          <DialogTitle>Comments</DialogTitle>
          <DialogDescription>
            {comments?.length || 0} comment{comments?.length !== 1 ? 's' : ''}
          </DialogDescription>
        </DialogHeader>

        {/* Comments List */}
        <div className="flex-1 overflow-y-auto space-y-2 py-4 -mx-6 px-6">
          {isLoading ? (
            <div className="flex justify-center py-8">
              <Spinner size="md" />
            </div>
          ) : error ? (
            <div className="text-center py-8 space-y-3">
              <div className="text-sm text-muted-foreground">
                Failed to load comments
              </div>
              <Button
                size="sm"
                variant="outline"
                onClick={() => refetch()}
              >
                Retry
              </Button>
            </div>
          ) : topLevelComments.length === 0 ? (
            <div className="text-center py-8 text-muted-foreground text-sm">
              No comments yet. Be the first to comment!
            </div>
          ) : (
            topLevelComments.map((comment) => (
              <div key={comment.id}>
                {/* Top-level comment */}
                <CommentRow
                  comment={comment}
                  onReplyClick={(id) => setReplyingTo(id)}
                />

                {/* Reply composer */}
                {replyingTo === comment.id && (
                  <div className="pl-8">
                    <CommentComposer
                      postId={postId}
                      parentId={comment.id}
                      placeholder={`Reply to ${comment.username}...`}
                      autoFocus
                      onCommentCreated={handleCommentCreated}
                    />
                  </div>
                )}

                {/* Nested replies */}
                {replies[comment.id] && (
                  <div className="pl-8 border-l border-muted">
                    {replies[comment.id].map((reply) => (
                      <div key={reply.id}>
                        <CommentRow
                          comment={reply}
                          onReplyClick={() => setReplyingTo(reply.id)}
                        />
                        {replyingTo === reply.id && (
                          <CommentComposer
                            postId={postId}
                            parentId={reply.parentId}
                            placeholder={`Reply to ${reply.username}...`}
                            autoFocus
                            onCommentCreated={handleCommentCreated}
                          />
                        )}
                      </div>
                    ))}
                  </div>
                )}
              </div>
            ))
          )}
        </div>

        {/* Comment Composer */}
        <div className="border-t border-border pt-4 -mx-6 px-6">
          <CommentComposer
            postId={postId}
            onCommentCreated={handleCommentCreated}
          />
        </div>
      </DialogContent>
    </Dialog>
  )
}
