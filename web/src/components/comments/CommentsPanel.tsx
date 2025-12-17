import { useState, useEffect } from 'react'
import { useQuery } from '@tanstack/react-query'
import { CommentsClient } from '@/api/CommentsClient'
import { CommentRow } from './CommentRow'
import { CommentComposer } from './CommentComposer'
import { Spinner } from '@/components/ui/spinner'
import { Button } from '@/components/ui/button'
import { getWebSocketClient, type UserTypingPayload, type UserStopTypingPayload } from '@/api/websocket'
import { useUserStore } from '@/stores/useUserStore'
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
  const [typingUsers, setTypingUsers] = useState<Map<string, UserTypingPayload>>(new Map())
  const { user: currentUser } = useUserStore()

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

  // Subscribe to typing indicators
  useEffect(() => {
    if (!isOpen) return

    const ws = getWebSocketClient()

    const unsubscribeTyping = ws.on('user_typing', (payload) => {
      const typingPayload = payload as UserTypingPayload
      // Only show typing for this post and not for current user
      if (typingPayload.post_id === postId && typingPayload.user_id !== currentUser?.id) {
        setTypingUsers((prev) => new Map(prev).set(typingPayload.user_id, typingPayload))
      }
    })

    const unsubscribeStopTyping = ws.on('user_stop_typing', (payload) => {
      const stopPayload = payload as UserStopTypingPayload
      if (stopPayload.post_id === postId) {
        setTypingUsers((prev) => {
          const next = new Map(prev)
          next.delete(stopPayload.user_id)
          return next
        })
      }
    })

    return () => {
      unsubscribeTyping()
      unsubscribeStopTyping()
    }
  }, [isOpen, postId, currentUser?.id])

  const handleCommentCreated = () => {
    setReplyingTo(null)
    refetch()
  }

  // Separate comments into top-level and replies
  const commentsArray = comments || []
  const topLevelComments = commentsArray.filter((c) => !c.parentId)
  const replies = commentsArray.reduce(
    (acc, c) => {
      if (c.parentId) {
        if (!acc[c.parentId]) acc[c.parentId] = []
        acc[c.parentId]?.push(c)
      }
      return acc
    },
    {} as Record<string, typeof commentsArray>
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
          ) : topLevelComments.length === 0 && typingUsers.size === 0 ? (
            <div className="text-center py-8 text-muted-foreground text-sm">
              No comments yet. Be the first to comment!
            </div>
          ) : (
            <>
              {topLevelComments.map((comment) => (
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
              ))}

              {/* Typing Indicators */}
              {typingUsers.size > 0 && (
                <div className="flex items-center gap-2 py-2 text-xs text-muted-foreground italic">
                  <div className="flex gap-1">
                    <span className="w-1.5 h-1.5 bg-muted-foreground rounded-full animate-bounce" style={{ animationDelay: '0ms' }} />
                    <span className="w-1.5 h-1.5 bg-muted-foreground rounded-full animate-bounce" style={{ animationDelay: '150ms' }} />
                    <span className="w-1.5 h-1.5 bg-muted-foreground rounded-full animate-bounce" style={{ animationDelay: '300ms' }} />
                  </div>
                  <span>
                    {Array.from(typingUsers.values())
                      .map((u) => u.display_name || u.username)
                      .join(', ')}{' '}
                    {typingUsers.size === 1 ? 'is' : 'are'} typing...
                  </span>
                </div>
              )}
            </>
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
