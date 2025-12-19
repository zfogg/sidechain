import { apiClient } from './client'
import { Outcome } from './types'
import { FeedPostModel } from '../models/FeedPost'
import type { FeedPost, FeedPostJSON } from '../models/FeedPost'

export type FeedType = 'timeline' | 'global' | 'trending' | 'forYou';

// API Response interfaces
interface FeedActivitiesResponse {
  activities: FeedPostJSON[]
}

// Handles both direct post return and wrapped post object
type FeedPostResponse = FeedPostJSON | { post: FeedPostJSON }

interface FeedPostsResponse {
  posts: FeedPostJSON[]
}

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
    offset: number = 0,
    retryOnCorruption: boolean = true
  ): Promise<Outcome<FeedPost[]>> {
    const endpoints: Record<FeedType, string> = {
      timeline: '/feed/timeline/enriched',
      global: '/feed/global/enriched',
      trending: '/feed/trending',
      forYou: '/feed/unified',
    };

    const result = await apiClient.get<FeedActivitiesResponse>(
      endpoints[feedType], 
      {
        limit,
        offset,
      },
      // Add cache-bypass header when retrying after corruption detection
      !retryOnCorruption ? { 'X-Cache-Bypass': 'true' } : undefined
    );

    if (result.isOk()) {
      try {
        let response = result.getValue();
        
        // Handle case where response might be a string (shouldn't happen, but defensive)
        if (typeof response === 'string') {
          console.warn(`[FeedClient] Response is a string, parsing JSON for ${feedType}`);
          try {
            response = JSON.parse(response);
          } catch (parseError) {
            console.error(`[FeedClient] Failed to parse string response for ${feedType}:`, parseError);
            // If this is the first attempt and we haven't retried yet, retry with cache-busting
            if (retryOnCorruption) {
              console.warn(`[FeedClient] Retrying request without cache for ${feedType}`);
              return this.getFeed(feedType, limit, offset, false);
            }
            return Outcome.error('Invalid response format: expected object, got string');
          }
        }
        
        // Ensure response is an object
        if (typeof response !== 'object' || response === null) {
          console.error(`[FeedClient] Invalid response type for ${feedType}:`, typeof response, response);
          // If this is the first attempt and we haven't retried yet, retry with cache-busting
          if (retryOnCorruption) {
            console.warn(`[FeedClient] Retrying request without cache for ${feedType} due to invalid response type`);
            return this.getFeed(feedType, limit, offset, false);
          }
          return Outcome.error(`Invalid response format: expected object, got ${typeof response}`);
        }
        
        // Check for suspicious response structure (corrupted cache)
        const responseKeys = Object.keys(response);
        if (responseKeys.length > 10 && !('activities' in response)) {
          console.warn(`[FeedClient] Suspicious response structure detected for ${feedType}: ${responseKeys.length} keys, retrying without cache`);
          // If this is the first attempt and we haven't retried yet, retry with cache-busting
          if (retryOnCorruption) {
            return this.getFeed(feedType, limit, offset, false);
          }
        }
        
        console.log(`[FeedClient] Raw response for ${feedType}:`, {
          hasActivities: 'activities' in response,
          activitiesType: typeof response.activities,
          activitiesLength: Array.isArray(response.activities) ? response.activities.length : 'not an array',
          responseKeys: Object.keys(response),
          responseSample: JSON.stringify(response).substring(0, 500)
        });
        
        const activities = response.activities || [];
        console.log(`[FeedClient] Fetched ${activities.length} activities for ${feedType}`);
        
        if (activities.length === 0) {
          console.warn(`[FeedClient] No activities returned for ${feedType}. Full response:`, response);
          return Outcome.ok([]);
        }
        
        const posts = activities.map((activity: any, index: number) => {
          try {
            const post = FeedPostModel.fromJson(activity);
            if (!FeedPostModel.isValid(post)) {
              console.warn(`[FeedClient] Post ${index} failed validation:`, {
                id: post.id,
                userId: post.userId,
                audioUrl: post.audioUrl,
                original: activity
              });
            }
            return post;
          } catch (err) {
            console.error(`[FeedClient] Failed to parse activity ${index}:`, err, activity);
            throw err;
          }
        });
        
        // Filter out invalid posts but log why
        const validPosts = posts.filter((post, index) => {
          const isValid = FeedPostModel.isValid(post);
          if (!isValid) {
            console.warn(`[FeedClient] Filtering out invalid post at index ${index}:`, {
              id: post.id,
              userId: post.userId,
              audioUrl: post.audioUrl,
              hasId: post.id !== '',
              hasUserId: post.userId !== '',
              hasAudioUrl: post.audioUrl !== ''
            });
          }
          return isValid;
        });
        
        if (validPosts.length !== posts.length) {
          console.warn(`[FeedClient] Filtered out ${posts.length - validPosts.length} invalid posts out of ${posts.length} total`);
        }
        
        console.log(`[FeedClient] Successfully parsed ${validPosts.length} valid posts for ${feedType}`);
        return Outcome.ok(validPosts);
      } catch (error) {
        console.error(`[FeedClient] Error parsing feed response for ${feedType}:`, error);
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
      return apiClient.post('/social/like', { activity_id: postId });
    } else {
      return apiClient.delete('/social/like', { activity_id: postId });
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
  static async trackPlay(postId: string, duration: number = 0, completed: boolean = false): Promise<Outcome<void>> {
    return apiClient.post(`/posts/${postId}/play`, {
      duration, // Seconds listened
      completed, // Whether user listened to the full track
    });
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
    const result = await apiClient.post<FeedPostResponse>('/feed/post', data);

    if (result.isOk()) {
      try {
        const response = result.getValue();
        const postJson = 'post' in response ? response.post : response;
        const post = FeedPostModel.fromJson(postJson);
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
    const result = await apiClient.get<FeedPostResponse>(`/posts/${postId}`);

    if (result.isOk()) {
      try {
        const response = result.getValue();
        const postJson = 'post' in response ? response.post : response;
        const post = FeedPostModel.fromJson(postJson);
        return Outcome.ok(post);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Get user's pinned posts (max 3)
   */
  static async getUserPinnedPosts(userId: string): Promise<Outcome<FeedPost[]>> {
    const result = await apiClient.get<FeedPostsResponse>(`/users/${userId}/pinned`);

    if (result.isOk()) {
      try {
        const posts = (result.getValue().posts || []).map(FeedPostModel.fromJson);
        return Outcome.ok(posts);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Get user's posts (all or paginated)
   */
  static async getUserPosts(
    userId: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<FeedPost[]>> {
    const result = await apiClient.get<FeedPostsResponse>(`/users/${userId}/posts`, {
      limit,
      offset,
    });

    if (result.isOk()) {
      try {
        const posts = (result.getValue().posts || []).map(FeedPostModel.fromJson);
        return Outcome.ok(posts);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }
}
