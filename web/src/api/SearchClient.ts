import { apiClient } from './client'
import { Outcome } from './types'
import { FeedPost } from '@/models/FeedPost'

export interface SearchFilters {
  query?: string
  genre?: string[]
  bpm?: { min: number; max: number }
  key?: string
  daw?: string
  limit?: number
  offset?: number
}

export interface SearchResults {
  posts: FeedPost[]
  users: SearchUser[]
  total: number
  took: number // milliseconds
}

export interface SearchUser {
  id: string
  username: string
  displayName: string
  profilePictureUrl?: string
  followerCount: number
  bio?: string
}

/**
 * SearchClient - Global search across posts, users, and sounds
 *
 * API Endpoints:
 * GET /search - Global search with filters
 * GET /search/posts - Search posts by title, description, tags
 * GET /search/users - Search users by username, display name
 * GET /search/filters/genres - Get available genres
 * GET /search/filters/keys - Get musical keys
 * GET /search/filters/daws - Get DAW list
 */
export class SearchClient {
  /**
   * Global search across posts and users
   */
  static async search(filters: SearchFilters): Promise<Outcome<SearchResults>> {
    const params: any = {
      limit: filters.limit || 20,
      offset: filters.offset || 0,
    }

    if (filters.query) params.q = filters.query
    if (filters.genre?.length) params.genre = filters.genre.join(',')
    if (filters.bpm) {
      params.bpm_min = filters.bpm.min
      params.bpm_max = filters.bpm.max
    }
    if (filters.key) params.key = filters.key
    if (filters.daw) params.daw = filters.daw

    const result = await apiClient.get<any>('/search', params)

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok({
      posts: (data.posts || []).map(FeedPost.fromJson),
      users: (data.users || []).map((user: any) => ({
        id: user.id,
        username: user.username,
        displayName: user.display_name,
        profilePictureUrl: user.profile_picture_url,
        followerCount: user.follower_count || 0,
        bio: user.bio,
      })),
      total: data.total || 0,
      took: data.took || 0,
    })
  }

  /**
   * Search posts with text and metadata filters
   */
  static async searchPosts(filters: SearchFilters): Promise<Outcome<FeedPost[]>> {
    const params: any = {
      limit: filters.limit || 50,
      offset: filters.offset || 0,
    }

    if (filters.query) params.q = filters.query
    if (filters.genre?.length) params.genre = filters.genre.join(',')
    if (filters.bpm) {
      params.bpm_min = filters.bpm.min
      params.bpm_max = filters.bpm.max
    }
    if (filters.key) params.key = filters.key
    if (filters.daw) params.daw = filters.daw

    const result = await apiClient.get<any>('/search/posts', params)

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok((data.posts || []).map(FeedPost.fromJson))
  }

  /**
   * Search users by username or display name
   */
  static async searchUsers(
    query: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<SearchUser[]>> {
    const result = await apiClient.get<any>('/search/users', {
      q: query,
      limit,
      offset,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok(
      (data.users || []).map((user: any) => ({
        id: user.id,
        username: user.username,
        displayName: user.display_name,
        profilePictureUrl: user.profile_picture_url,
        followerCount: user.follower_count || 0,
        bio: user.bio,
      }))
    )
  }

  /**
   * Get available genres for filtering
   */
  static async getGenres(): Promise<Outcome<string[]>> {
    const result = await apiClient.get<{ genres: string[] }>('/search/filters/genres')

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    return Outcome.ok(result.getValue().genres)
  }

  /**
   * Get musical keys for filtering
   */
  static async getKeys(): Promise<Outcome<string[]>> {
    const result = await apiClient.get<{ keys: string[] }>('/search/filters/keys')

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    return Outcome.ok(result.getValue().keys)
  }

  /**
   * Get DAWs for filtering
   */
  static async getDAWs(): Promise<Outcome<string[]>> {
    const result = await apiClient.get<{ daws: string[] }>('/search/filters/daws')

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    return Outcome.ok(result.getValue().daws)
  }
}
