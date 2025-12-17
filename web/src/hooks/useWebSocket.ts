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
  FeedInvalidatePayload,
  TimelineUpdatePayload,
  ActivityUpdatePayload,
  NotificationCountUpdatePayload,
  UserTypingPayload,
  UserStopTypingPayload,
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

    const unsubscribeFeedInvalidate = ws.on('feed_invalidate', (payload) => {
      const invalidatePayload = payload as FeedInvalidatePayload
      console.log('[RT] Feed invalidate:', invalidatePayload)
      // Force refresh the specified feed type so next fetch gets fresh data from Stream.io
      const feedType = invalidatePayload.feed_type as any // Map to FeedType
      if (['timeline', 'global', 'trending', 'forYou'].includes(feedType)) {
        feedStore.loadFeed(feedType, true)
      }
    })

    const unsubscribeTimelineUpdate = ws.on('timeline_update', (payload) => {
      const timelinePayload = payload as TimelineUpdatePayload
      console.log('[RT] Timeline update:', timelinePayload)
      // Timeline was updated (e.g., followed user posted new activity)
      // Force refresh timeline feed to show updated activity
      feedStore.loadFeed('timeline', true)
    })

    const unsubscribeActivityUpdate = ws.on('activity_update', (payload) => {
      const activityPayload = payload as ActivityUpdatePayload
      console.log('[RT] Activity update:', activityPayload)
      // Handle activity updates (posts, likes, follows, comments)
      if (activityPayload.verb === 'posted' && activityPayload.object_type === 'loop_post') {
        // Create a FeedPost from the activity payload
        const newPost = FeedPostModel.fromJson({
          id: activityPayload.object,
          user_id: activityPayload.actor,
          username: activityPayload.actor_name || '',
          display_name: activityPayload.actor_name || '',
          avatar_url: activityPayload.actor_avatar,
          audio_url: activityPayload.audio_url || '',
          waveform_url: activityPayload.waveform_url || '',
          bpm: activityPayload.bpm || 0,
          key: activityPayload.key,
          daw: activityPayload.daw,
          genre: activityPayload.genre || [],
          like_count: 0,
          comment_count: 0,
          created_at: new Date(activityPayload.timestamp).toISOString(),
          is_liked_by_me: false,
          is_following: false,
        })
        // Prepend to affected feeds
        if (activityPayload.feed_types?.includes('global')) {
          const globalFeed = feedStore.feeds['global']
          if (globalFeed) {
            globalFeed.posts.unshift(newPost)
          }
        }
        if (activityPayload.feed_types?.includes('timeline')) {
          const timelineFeed = feedStore.feeds['timeline']
          if (timelineFeed) {
            timelineFeed.posts.unshift(newPost)
          }
        }
      } else if (activityPayload.verb === 'liked') {
        // Update like count for a post
        feedStore.updatePost(activityPayload.object, {
          likeCount: activityPayload.like_count || 0,
        })
      } else if (activityPayload.verb === 'commented') {
        // Update comment count for a post
        feedStore.updatePost(activityPayload.object, {
          commentCount: (activityPayload.comment_count || 0) + 1,
        })
      }
    })

    const unsubscribeUserTyping = ws.on('user_typing', (payload) => {
      const typingPayload = payload as UserTypingPayload
      console.log('[RT] User typing:', typingPayload)
      // Components listening for typing can subscribe directly to this event
      // CommentsPanel component will handle displaying the typing indicator
    })

    const unsubscribeUserStopTyping = ws.on('user_stop_typing', (payload) => {
      const stopPayload = payload as UserStopTypingPayload
      console.log('[RT] User stopped typing:', stopPayload)
      // Components listening for typing can subscribe directly to this event
    })

    const unsubscribeNotificationCountUpdate = ws.on('notification_count_update', (payload) => {
      const countPayload = payload as NotificationCountUpdatePayload
      console.log('[RT] Notification count update:', countPayload)
      // Update notification counts in user store
      // This would update unread and unseen notification badges
      // For now, just log - notification store can implement this
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
      unsubscribeFeedInvalidate()
      unsubscribeTimelineUpdate()
      unsubscribeActivityUpdate()
      unsubscribeNotificationCountUpdate()
      unsubscribeUserTyping()
      unsubscribeUserStopTyping()
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
