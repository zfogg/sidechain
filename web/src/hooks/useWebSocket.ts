import { useEffect, useRef } from 'react'
import { getWebSocketClient } from '@/api/websocket'
import type {
  NewPostPayload,
  PostLikedPayload,
  PostUnlikedPayload,
  PostCommentedPayload,
  NewCommentPayload,
  PostSavedPayload,
  UserFollowedPayload,
  CommentLikedPayload,
  CommentUnlikedPayload,
  LikeCountUpdatePayload,
  CommentCountUpdatePayload,
  EngagementMetricsPayload,
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
        likeCount: likedPayload.like_count,
      })
    })

    const unsubscribePostUnliked = ws.on('post_unliked', (payload) => {
      const unlikedPayload = payload as PostUnlikedPayload
      console.log('[RT] Post unliked:', unlikedPayload)
      // Update like count in feed
      feedStore.updatePost(unlikedPayload.post_id, {
        likeCount: unlikedPayload.like_count,
      })
    })

    const unsubscribeNewComment = ws.on('new_comment', (payload) => {
      const commentPayload = payload as NewCommentPayload
      console.log('[RT] New comment:', commentPayload)
      // Increment comment count when new comment added
      feedStore.updatePost(commentPayload.post_id, {
        commentCount: (feedStore.feeds[feedStore.currentFeedType]?.posts.find(
          p => p.id === commentPayload.post_id
        )?.commentCount ?? 0) + 1,
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

    const unsubscribeCommentLiked = ws.on('comment_liked', (payload) => {
      const commentPayload = payload as CommentLikedPayload
      console.log('[RT] Comment liked:', commentPayload)
      // Update post to reflect comment like (would need comment-level state)
      // For now, just log - UI components can subscribe to this directly if needed
    })

    const unsubscribeCommentUnliked = ws.on('comment_unliked', (payload) => {
      const commentPayload = payload as CommentUnlikedPayload
      console.log('[RT] Comment unliked:', commentPayload)
      // Update post to reflect comment unlike
    })

    const unsubscribeLikeCountUpdate = ws.on('like_count_update', (payload) => {
      const countPayload = payload as LikeCountUpdatePayload
      console.log('[RT] Like count update:', countPayload)
      feedStore.updatePost(countPayload.post_id, {
        likeCount: countPayload.like_count,
      })
    })

    const unsubscribeCommentCountUpdate = ws.on('comment_count_update', (payload) => {
      const countPayload = payload as CommentCountUpdatePayload
      console.log('[RT] Comment count update:', countPayload)
      feedStore.updatePost(countPayload.post_id, {
        commentCount: countPayload.comment_count,
      })
    })

    const unsubscribeEngagementMetrics = ws.on('engagement_metrics', (payload) => {
      const metricsPayload = payload as EngagementMetricsPayload
      console.log('[RT] Engagement metrics:', metricsPayload)
      feedStore.updatePost(metricsPayload.post_id, {
        likeCount: metricsPayload.like_count,
        commentCount: metricsPayload.comment_count,
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
      unsubscribePostUnliked()
      unsubscribeNewComment()
      unsubscribePostCommented()
      unsubscribePostSaved()
      unsubscribeUserFollowed()
      unsubscribeCommentLiked()
      unsubscribeCommentUnliked()
      unsubscribeLikeCountUpdate()
      unsubscribeCommentCountUpdate()
      unsubscribeEngagementMetrics()
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
