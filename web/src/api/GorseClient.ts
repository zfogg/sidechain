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
    const result = await apiClient.get<any>('/recommendations', {
      limit,
      offset,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok(
      (data.recommendations || []).map((rec: any) => ({
        post: FeedPostModel.fromJson(rec.post),
        score: rec.score || 0,
        reason: rec.reason,
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
    const result = await apiClient.get<any>('/recommendations/producers', {
      limit,
      offset,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok(
      (data.producers || []).map((producer: any) => ({
        id: producer.id,
        username: producer.username,
        displayName: producer.display_name,
        profilePictureUrl: producer.profile_picture_url,
        bio: producer.bio,
        followerCount: producer.follower_count || 0,
        postCount: producer.post_count || 0,
        isFollowing: producer.is_following || false,
        trendingScore: producer.trending_score || 0,
      }))
    )
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
