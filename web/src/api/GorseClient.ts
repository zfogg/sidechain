import { apiClient } from './client'
import { Outcome } from './types'
import { FeedPost, FeedPostModel } from '@/models/FeedPost'

export interface RecommendedPost {
  post: FeedPost
  score: number
  reason?: string // Why this post was recommended
}

export interface TrendingProducer {
  id: string
  username: string
  displayName: string
  profilePictureUrl?: string
  bio?: string
  followerCount: number
  postCount: number
  isFollowing: boolean
  trendingScore: number
}

/**
 * GorseClient - Integrates with Gorse recommendation engine
 * Provides personalized recommendations and discovery features
 *
 * Gorse API Endpoints:
 * GET /recommendations - Get personalized post recommendations
 * GET /recommendations/trending - Get trending posts
 * GET /recommendations/producers - Get trending producers
 * GET /recommendations/similar/:postId - Get posts similar to a given post
 * POST /feedback/:postId/:action - Send feedback (like, view, comment)
 */
export class GorseClient {
  /**
   * Get personalized post recommendations
   */
  static async getRecommendations(
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<RecommendedPost[]>> {
    // Use the backend's /recommendations/for-you endpoint which provides personalized recommendations
    const result = await apiClient.get<any>('/recommendations/for-you', {
      limit,
      offset,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    // Handle both direct posts array and wrapped response
    const posts = Array.isArray(data) ? data : (data.posts || data.activities || [])
    return Outcome.ok(
      posts.map((item: any) => ({
        post: FeedPostModel.fromJson(item.post || item),
        score: item.score || 0,
        reason: item.reason,
      }))
    )
  }

  /**
   * Get trending posts across the platform
   */
  static async getTrendingPosts(
    limit: number = 20,
    offset: number = 0,
    timeWindow: 'day' | 'week' | 'month' = 'week'
  ): Promise<Outcome<FeedPost[]>> {
    const result = await apiClient.get<any>('/recommendations/trending', {
      limit,
      offset,
      time_window: timeWindow,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok((data.posts || []).map(FeedPostModel.fromJson))
  }

  /**
   * Get trending producers
   */
  static async getTrendingProducers(
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<TrendingProducer[]>> {
    // Get trending posts to show trending producers
    // Extract unique users from trending posts
    const result = await apiClient.get<any>('/recommendations/popular', {
      limit: limit * 3, // Get more posts to extract unique producers
      offset,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    const posts = Array.isArray(data) ? data : (data.posts || data.activities || [])

    // Extract trending producers by aggregating posts by user
    const producerMap = new Map<string, { user_id: string; play_count: number; post_count: number }>()
    posts.forEach((item: any) => {
      const post = item.post || item
      if (post.actor) {
        const userId = post.actor.id || post.user_id
        if (!producerMap.has(userId)) {
          producerMap.set(userId, { user_id: userId, play_count: 0, post_count: 0 })
        }
        const producer = producerMap.get(userId)!
        producer.post_count += 1
        producer.play_count += post.play_count || 0
      }
    })

    // Convert to array and sort by play count
    const trendingProducers = Array.from(producerMap.values())
      .sort((a, b) => b.play_count - a.play_count)
      .slice(0, limit)

    return Outcome.ok(trendingProducers as TrendingProducer[])
  }

  /**
   * Get posts similar to a given post
   */
  static async getSimilarPosts(
    postId: string,
    limit: number = 10
  ): Promise<Outcome<FeedPost[]>> {
    const result = await apiClient.get<any>(`/recommendations/similar/${postId}`, {
      limit,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok((data.posts || []).map(FeedPostModel.fromJson))
  }

  /**
   * Send user feedback to Gorse for better recommendations
   * Actions: view, like, comment, download, share, dislike
   */
  static async sendFeedback(
    postId: string,
    action: 'view' | 'like' | 'comment' | 'download' | 'share' | 'dislike'
  ): Promise<Outcome<void>> {
    return apiClient.post(`/feedback/${postId}/${action}`, {})
  }

  /**
   * Get recommendations for a specific genre
   */
  static async getGenreRecommendations(
    genre: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<FeedPost[]>> {
    const result = await apiClient.get<any>('/recommendations/genre', {
      genre,
      limit,
      offset,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok((data.posts || []).map(FeedPostModel.fromJson))
  }

  /**
   * Get recommendations for a specific BPM range
   */
  static async getBpmRecommendations(
    minBpm: number,
    maxBpm: number,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<FeedPost[]>> {
    const result = await apiClient.get<any>('/recommendations/bpm', {
      min_bpm: minBpm,
      max_bpm: maxBpm,
      limit,
      offset,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok((data.posts || []).map(FeedPostModel.fromJson))
  }
}
