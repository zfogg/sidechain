import { useMutation, useQueryClient } from '@tanstack/react-query'
import { CommentsClient } from '@/api/CommentsClient'
import type { Comment } from '@/models/Comment'

/**
 * useCreateCommentMutation - Mutation for creating new comments
 *
 * Usage:
 * ```tsx
 * const { mutate: createComment, isPending } = useCreateCommentMutation()
 * const handleComment = () => createComment({ postId: 'post-123', content: 'Great loop!' })
 * ```
 */
export function useCreateCommentMutation() {
  const queryClient = useQueryClient()

  return useMutation({
    mutationFn: async ({
      postId,
      content,
      parentId,
    }: {
      postId: string
      content: string
      parentId?: string
    }) => {
      const result = await CommentsClient.createComment(postId, content, parentId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    onMutate: async ({ postId, content }) => {
      // Cancel outgoing refetches
      await queryClient.cancelQueries({ queryKey: ['comments', postId] })

      // Snapshot previous data
      const previousComments = queryClient.getQueryData<Comment[]>([
        'comments',
        postId,
      ])

      // Create optimistic comment (temporary)
      const optimisticComment: Comment = {
        id: `optimistic-${Date.now()}`,
        postId,
        userId: '', // Will be filled by server
        content,
        parentId: undefined,
        likeCount: 0,
        isLiked: false,
        isOwnComment: true,
        isEdited: false,
        createdAt: new Date(),
        updatedAt: new Date(),
        username: '',
        displayName: '',
        userAvatarUrl: '',
      }

      // Optimistic update
      queryClient.setQueryData<Comment[]>(['comments', postId], (old = []) => [
        ...old,
        optimisticComment,
      ])

      return { previousComments }
    },
    onError: (err, variables, context: any) => {
      // Rollback on error
      if (context?.previousComments) {
        queryClient.setQueryData(['comments', variables.postId], context.previousComments)
      }
    },
    onSuccess: (newComment, { postId }) => {
      // Replace optimistic comments with real ones from server
      queryClient.invalidateQueries({ queryKey: ['comments', postId] })
    },
  })
}

/**
 * useEditCommentMutation - Mutation for editing existing comments
 */
export function useEditCommentMutation() {
  const queryClient = useQueryClient()

  return useMutation({
    mutationFn: async ({
      commentId,
      content,
    }: {
      commentId: string
      content: string
    }) => {
      const result = await CommentsClient.editComment(commentId, content)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    onSuccess: (updatedComment, { commentId }) => {
      // Invalidate comments queries to refresh
      queryClient.invalidateQueries({ queryKey: ['comments'] })
    },
  })
}

/**
 * useDeleteCommentMutation - Mutation for deleting comments
 */
export function useDeleteCommentMutation() {
  const queryClient = useQueryClient()

  return useMutation({
    mutationFn: async (commentId: string) => {
      const result = await CommentsClient.deleteComment(commentId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onSuccess: () => {
      // Invalidate all comments queries
      queryClient.invalidateQueries({ queryKey: ['comments'] })
    },
  })
}

/**
 * useToggleCommentLikeMutation - Mutation for liking/unliking comments
 */
export function useToggleCommentLikeMutation() {
  const queryClient = useQueryClient()

  return useMutation({
    mutationFn: async ({
      commentId,
      shouldLike,
    }: {
      commentId: string
      shouldLike: boolean
    }) => {
      const result = await CommentsClient.toggleCommentLike(commentId, shouldLike)
      if (result.isError()) {
        throw new Error(result.getError())
      }
    },
    onMutate: async ({ commentId, shouldLike }) => {
      // Cancel outgoing refetches
      await queryClient.cancelQueries({ queryKey: ['comments'] })

      // Snapshot previous data
      const previousComments = queryClient.getQueriesData<any>({ queryKey: ['comments'] })

      // Optimistic update
      queryClient.setQueriesData({ queryKey: ['comments'] }, (old: any) => {
        if (Array.isArray(old)) {
          return old.map((comment: Comment) =>
            comment.id === commentId
              ? {
                  ...comment,
                  isLiked: shouldLike,
                  likeCount: comment.likeCount + (shouldLike ? 1 : -1),
                }
              : comment
          )
        }
        return old
      })

      return { previousComments }
    },
    onError: (err, variables, context: any) => {
      if (context?.previousComments) {
        context.previousComments.forEach(([queryKey, data]: [any, any]) => {
          queryClient.setQueryData(queryKey, data)
        })
      }
    },
  })
}
