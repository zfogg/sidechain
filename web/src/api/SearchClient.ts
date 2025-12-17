import { apiClient } from './client'
import { Outcome } from './types'
import { FeedPost, FeedPostModel } from '../models/FeedPost'

export interface SearchFilters {
  query?: string
  bpmMin?: number
  bpmMax?: number
  key?: string
  genres?: string[]
  daw?: string
  limit?: number
  offset?: number
}

interface SearchResponse {
  posts: any[]
  total: number
  limit: number
  offset: number
}

interface GenresResponse {
  genres: string[]
}

interface KeysResponse {
  keys: string[]
}

/**
 * SearchClient - Advanced search with filters for music discovery
 * Mirrors C++ SearchClient.cpp pattern
 */
export class SearchClient {
  /**
   * Search posts with advanced filters
   */
  static async searchPosts(filters: SearchFilters): Promise<Outcome<{ posts: FeedPost[]; total: number }>> {
    const params: any = {}

    if (filters.query) params.q = filters.query
    if (filters.bpmMin) params.bpm_min = filters.bpmMin
    if (filters.bpmMax) params.bpm_max = filters.bpmMax
    if (filters.key) params.key = filters.key
    if (filters.genres && filters.genres.length > 0) {
      params.genres = filters.genres.join(',')
    }
    if (filters.daw) params.daw = filters.daw
    if (filters.limit) params.limit = filters.limit
    if (filters.offset) params.offset = filters.offset

    const result = await apiClient.get<SearchResponse>('/search/posts', params)

    if (result.isOk()) {
      try {
        const data = result.getValue()
        const posts = (data.posts || []).map(FeedPostModel.fromJson)
        return Outcome.ok({
          posts,
          total: data.total || 0,
        })
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Search users by username or display name
   */
  static async searchUsers(
    query: string,
    limit: number = 20
  ): Promise<
    Outcome<
      Array<{
        id: string
        username: string
        displayName: string
        profilePictureUrl?: string
      }>
    >
  > {
    const result = await apiClient.get<{
      users: any[]
    }>('/search/users', { q: query, limit })

    if (result.isOk()) {
      try {
        const users = (result.getValue().users || []).map((u: any) => ({
          id: u.id,
          username: u.username,
          displayName: u.display_name || u.username,
          profilePictureUrl: u.avatar_url,
        }))
        return Outcome.ok(users)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get all available genres
   */
  static async getGenres(): Promise<Outcome<string[]>> {
    const result = await apiClient.get<GenresResponse>('/search/genres')

    if (result.isOk()) {
      return Outcome.ok(result.getValue().genres || [])
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get all available musical keys
   */
  static async getKeys(): Promise<Outcome<string[]>> {
    const result = await apiClient.get<KeysResponse>('/search/keys')

    if (result.isOk()) {
      return Outcome.ok(result.getValue().keys || [])
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get all available DAWs
   */
  static async getDAWs(): Promise<Outcome<string[]>> {
    const result = await apiClient.get<{ daws: string[] }>('/search/daws')

    if (result.isOk()) {
      return Outcome.ok(result.getValue().daws || [])
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get popular search queries (trending)
   */
  static async getTrendingSearches(): Promise<Outcome<Array<{ query: string; count: number }>>> {
    const result = await apiClient.get<{
      trending: Array<{ query: string; count: number }>
    }>('/search/trending')

    if (result.isOk()) {
      return Outcome.ok(result.getValue().trending || [])
    }

    return Outcome.error(result.getError())
  }

  /**
   * Autocomplete usernames
   */
  static async autocompleteUsers(query: string, limit: number = 10): Promise<Outcome<string[]>> {
    const result = await apiClient.get<{
      suggestions: string[]
    }>('/search/autocomplete/users', { q: query, limit })

    if (result.isOk()) {
      return Outcome.ok(result.getValue().suggestions || [])
    }

    return Outcome.error(result.getError())
  }

  /**
   * Autocomplete genres
   */
  static async autocompleteGenres(query: string, limit: number = 10): Promise<Outcome<string[]>> {
    const result = await apiClient.get<{
      suggestions: string[]
    }>('/search/autocomplete/genres', { q: query, limit })

    if (result.isOk()) {
      return Outcome.ok(result.getValue().suggestions || [])
    }

    return Outcome.error(result.getError())
  }
}
