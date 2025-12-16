import { useState, useCallback, useEffect, useRef } from 'react'
import { useWebSocketEmit } from '@/hooks/useWebSocket'
import { useUserStore } from '@/stores/useUserStore'

interface TypingUser {
  userId: string
  username: string
  displayName: string
}

/**
 * useCommentTyping - Manage typing indicators for comment composition
 *
 * Sends typing events to backend and displays other users typing
 */
export function useCommentTyping(postId: string) {
  const [typingUsers, setTypingUsers] = useState<TypingUser[]>([])
  const emit = useWebSocketEmit()
  const { user } = useUserStore()
  const typingTimeoutRef = useRef<NodeJS.Timeout>()
  const isTypingRef = useRef(false)

  // Send typing indicator to others
  const sendTypingIndicator = useCallback(() => {
    if (!user) return

    // Clear previous timeout
    if (typingTimeoutRef.current) {
      clearTimeout(typingTimeoutRef.current)
    }

    // Send "user_typing" event
    if (!isTypingRef.current) {
      isTypingRef.current = true
      emit('user_typing', {
        post_id: postId,
        user_id: user.id,
        username: user.username,
        display_name: user.displayName,
      })
    }

    // Stop typing after 3 seconds of inactivity
    typingTimeoutRef.current = setTimeout(() => {
      isTypingRef.current = false
      emit('user_stop_typing', {
        post_id: postId,
        user_id: user.id,
      })
    }, 3000)
  }, [postId, user, emit])

  // Add typing user
  const addTypingUser = useCallback((user: TypingUser) => {
    setTypingUsers((prev) => {
      // Don't add duplicate
      if (prev.some((u) => u.userId === user.userId)) {
        return prev
      }
      return [...prev, user]
    })

    // Auto-remove after 5 seconds
    setTimeout(() => {
      setTypingUsers((prev) => prev.filter((u) => u.userId !== user.userId))
    }, 5000)
  }, [])

  // Remove typing user
  const removeTypingUser = useCallback((userId: string) => {
    setTypingUsers((prev) => prev.filter((u) => u.userId !== userId))
  }, [])

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (typingTimeoutRef.current) {
        clearTimeout(typingTimeoutRef.current)
      }
      // Send stop typing when unmounting
      if (isTypingRef.current && user) {
        emit('user_stop_typing', {
          post_id: postId,
          user_id: user.id,
        })
      }
    }
  }, [postId, user, emit])

  return {
    typingUsers,
    sendTypingIndicator,
    addTypingUser,
    removeTypingUser,
  }
}
