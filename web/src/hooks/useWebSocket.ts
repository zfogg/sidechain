import { useEffect, useRef } from 'react'
import { getWebSocketClient } from '@/api/websocket'
import type {
  NewPostPayload,
  PostLikedPayload,
  PostCommentedPayload,
  PostSavedPayload,
  UserFollowedPayload,
  ErrorPayload,
  EventPayload,
} from '@/api/websocket'
import { FeedPostModel } from '@/models/FeedPost'
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
    const unsubscribeNewPost = ws.on('new_post', (payload) => {
      const newPostPayload = payload as NewPostPayload
      console.log('[RT] New post:', newPostPayload)
      // Prepend new post to current feed
      if (newPostPayload.post) {
        const post = FeedPostModel.fromJson(newPostPayload.post)
        feedStore.addPostToFeed(post)
      }
    })

    const unsubscribePostLiked = ws.on('post_liked', (payload) => {
      const likedPayload = payload as PostLikedPayload
      console.log('[RT] Post liked:', likedPayload)
      // Update like count in feed
      feedStore.updatePost(likedPayload.post_id, {
        likeCount: likedPayload.new_like_count,
      })
    })

    const unsubscribePostCommented = ws.on('post_commented', (payload) => {
      const commentedPayload = payload as PostCommentedPayload
      console.log('[RT] Post commented:', commentedPayload)
      // Increment comment count
      feedStore.updatePost(commentedPayload.post_id, {
        commentCount: commentedPayload.new_comment_count,
      })
    })

    const unsubscribePostSaved = ws.on('post_saved', (payload) => {
      const savedPayload = payload as PostSavedPayload
      console.log('[RT] Post saved:', savedPayload)
      // Update save count
      feedStore.updatePost(savedPayload.post_id, {
        saveCount: savedPayload.new_save_count,
      })
    })

    const unsubscribeUserFollowed = ws.on('user_followed', (payload) => {
      const followedPayload = payload as UserFollowedPayload
      console.log('[RT] User followed:', followedPayload)
      // Update is_following flag for posts
      feedStore.feeds[feedStore.currentFeedType]?.posts.forEach((post) => {
        if (post.userId === followedPayload.user_id) {
          feedStore.updatePost(post.id, {
            isFollowing: followedPayload.is_following,
          })
        }
      })
    })

    const unsubscribeError = ws.on('error', (payload) => {
      const errorPayload = payload as ErrorPayload
      console.error('[RT] Error:', errorPayload)
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

  return (type: string, payload?: Partial<EventPayload>) => {
    ws.send(type, payload || {})
  }
}
