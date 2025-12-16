import { apiClient } from './client'
import { Outcome } from './types'
import { FeedPostModel } from '../models/FeedPost'
import type { FeedPost } from '../models/FeedPost'

export type FeedType = 'timeline' | 'global' | 'trending' | 'forYou';

/**
 * FeedClient - Modular API client for feed operations
 * Mirrors C++ FeedClient.cpp pattern
 */

export class FeedClient {
  /**
   * Get feed posts for a specific feed type
   */
  static async getFeed(
    feedType: FeedType,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<FeedPost[]>> {
    const endpoints: Record<FeedType, string> = {
      timeline: '/feed/timeline/enriched',
      global: '/feed/global/enriched',
      trending: '/feed/trending',
      forYou: '/feed/unified',
    };

    const result = await apiClient.get<{ activities: any[] }>(endpoints[feedType], {
      limit,
      offset,
    });

    if (result.isOk()) {
      try {
        const activities = result.getValue().activities || [];
        const posts = activities.map(FeedPostModel.fromJson);
        return Outcome.ok(posts);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Like a post (optimistic update compatible)
   */
  static async toggleLike(postId: string, shouldLike: boolean): Promise<Outcome<void>> {
    if (shouldLike) {
      return apiClient.post('/social/like', { post_id: postId });
    } else {
      return apiClient.post('/social/unlike', { post_id: postId });
    }
  }

  /**
   * Save a post
   */
  static async toggleSave(postId: string, shouldSave: boolean): Promise<Outcome<void>> {
    if (shouldSave) {
      return apiClient.post(`/posts/${postId}/save`, {});
    } else {
      return apiClient.post(`/posts/${postId}/unsave`, {});
    }
  }

  /**
   * Add emoji reaction to a post
   */
  static async react(postId: string, emoji: string): Promise<Outcome<void>> {
    return apiClient.post('/social/react', {
      activity_id: postId,
      emoji,
    });
  }

  /**
   * Track play event
   */
  static async trackPlay(postId: string): Promise<Outcome<void>> {
    return apiClient.post(`/posts/${postId}/play`, {});
  }

  /**
   * Create a new post
   */
  static async createPost(data: {
    filename: string;
    audioUrl: string;
    bpm: number;
    key: string;
    daw: string;
    genre: string[];
  }): Promise<Outcome<FeedPost>> {
    const result = await apiClient.post<any>('/feed/post', data);

    if (result.isOk()) {
      try {
        const post = FeedPostModel.fromJson(result.getValue());
        return Outcome.ok(post);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Get single post by ID
   */
  static async getPost(postId: string): Promise<Outcome<FeedPost>> {
    const result = await apiClient.get<any>(`/posts/${postId}`);

    if (result.isOk()) {
      try {
        const post = FeedPostModel.fromJson(result.getValue());
        return Outcome.ok(post);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }
}
