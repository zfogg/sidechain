import { useEffect, useRef } from 'react'
import { getWebSocketClient } from '@/api/websocket'
import { useUserStore } from '@/stores/useUserStore'
import { useFeedStore } from '@/stores/useFeedStore'

/**
 * useWebSocket - React hook for WebSocket connection and real-time updates
 *
 * Automatically:
 * - Connects when user has token
 * - Disconnects on unmount
 * - Updates stores based on incoming messages
 * - Handles reconnection
 *
 * Usage in component:
 * ```tsx
 * function Feed() {
 *   useWebSocket()
 *   // Component receives real-time updates automatically
 * }
 * ```
 */
export function useWebSocket() {
  const { token, isAuthenticated } = useUserStore()
  const feedStore = useFeedStore()
  const wsRef = useRef(getWebSocketClient())

  useEffect(() => {
    const ws = wsRef.current

    if (!isAuthenticated || !token) {
      ws.disconnect()
      return
    }

    // Connect if not already connected
    if (!ws.isConnected()) {
      ws.connect(token)
    }

    // Subscribe to real-time events
    const unsubscribeNewPost = ws.on('new_post', (payload: any) => {
      console.log('[RT] New post:', payload)
      // Prepend new post to current feed
      if (payload.post) {
        feedStore.addPostToFeed(payload.post)
      }
    })

    const unsubscribePostLiked = ws.on('post_liked', (payload: any) => {
      console.log('[RT] Post liked:', payload)
      // Update like count in feed
      if (payload.post_id && payload.new_like_count !== undefined) {
        feedStore.updatePost(payload.post_id, {
          likeCount: payload.new_like_count,
        })
      }
    })

    const unsubscribePostCommented = ws.on('post_commented', (payload: any) => {
      console.log('[RT] Post commented:', payload)
      // Increment comment count
      if (payload.post_id && payload.new_comment_count !== undefined) {
        feedStore.updatePost(payload.post_id, {
          commentCount: payload.new_comment_count,
        })
      }
    })

    const unsubscribePostSaved = ws.on('post_saved', (payload: any) => {
      console.log('[RT] Post saved:', payload)
      // Update save count
      if (payload.post_id && payload.new_save_count !== undefined) {
        feedStore.updatePost(payload.post_id, {
          saveCount: payload.new_save_count,
        })
      }
    })

    const unsubscribeUserFollowed = ws.on('user_followed', (payload: any) => {
      console.log('[RT] User followed:', payload)
      // Update is_following flag for posts
      if (payload.user_id && payload.is_following !== undefined) {
        feedStore.feeds[feedStore.currentFeedType]?.posts.forEach((post) => {
          if (post.userId === payload.user_id) {
            feedStore.updatePost(post.id, {
              isFollowing: payload.is_following,
            })
          }
        })
      }
    })

    const unsubscribeError = ws.on('error', (payload: any) => {
      console.error('[RT] Error:', payload)
    })

    // Cleanup
    return () => {
      unsubscribeNewPost()
      unsubscribePostLiked()
      unsubscribePostCommented()
      unsubscribePostSaved()
      unsubscribeUserFollowed()
      unsubscribeError()
      // Don't disconnect on unmount - WebSocket should persist across components
      // ws.disconnect()
    }
  }, [token, isAuthenticated, feedStore])
}

/**
 * useWebSocketStatus - Hook to get WebSocket connection status
 *
 * Usage:
 * ```tsx
 * const { isConnected } = useWebSocketStatus()
 * return <div>{isConnected ? 'Connected' : 'Reconnecting...'}</div>
 * ```
 */
export function useWebSocketStatus() {
  const ws = getWebSocketClient()

  return {
    isConnected: ws.isConnected(),
    listeners: ws.getListeners(),
  }
}

/**
 * useWebSocketEmit - Hook to send messages through WebSocket
 */
export function useWebSocketEmit() {
  const ws = getWebSocketClient()

  return (type: string, payload?: Record<string, any>) => {
    ws.send(type, payload || {})
  }
}
