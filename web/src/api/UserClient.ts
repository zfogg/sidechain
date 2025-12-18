import { apiClient } from './client'
import { Outcome } from './types'

// Backend response uses snake_case
interface UserProfileResponse {
  id: string
  username: string
  display_name: string
  bio?: string
  profile_picture_url?: string
  avatar_url?: string
  follower_count: number
  following_count: number
  post_count: number
  is_following: boolean
  is_own_profile?: boolean
  created_at: string
  social_links?: {
    twitter?: string
    instagram?: string
    website?: string
  }
}

// Frontend uses camelCase
interface UserProfile {
  id: string
  username: string
  displayName: string
  bio?: string
  profilePictureUrl?: string
  followerCount: number
  followingCount: number
  postCount: number
  isFollowing: boolean
  isOwnProfile: boolean
  createdAt: string
  socialLinks?: {
    twitter?: string
    instagram?: string
    website?: string
  }
}

interface UserListResponse {
  users: Array<{
    id: string
    username: string
    displayName: string
    profilePictureUrl?: string
    isFollowing: boolean
  }>
  total: number
  offset: number
  limit: number
}

/**
 * UserClient - Handles user profile and social operations
 *
 * API Endpoints:
 * GET /users/:username/profile - Get user profile
 * POST /users/:userId/follow - Follow user
 * DELETE /users/:userId/follow - Unfollow user
 * GET /users/:userId/followers - Get followers list
 * GET /users/:userId/following - Get following list
 * PUT /users/me - Update own profile
 */
export class UserClient {
  /**
   * Get user profile by username
   */
  static async getProfile(username: string): Promise<Outcome<UserProfile>> {
    const result = await apiClient.get<UserProfileResponse>(`/users/${username}/profile`)

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const response = result.getValue()
    // Map snake_case to camelCase
    const profile: UserProfile = {
      id: response.id,
      username: response.username,
      displayName: response.display_name,
      bio: response.bio,
      profilePictureUrl: response.profile_picture_url,
      followerCount: response.follower_count,
      followingCount: response.following_count,
      postCount: response.post_count,
      isFollowing: response.is_following,
      isOwnProfile: response.is_own_profile ?? false,
      createdAt: response.created_at,
      socialLinks: response.social_links,
    }

    return Outcome.ok(profile)
  }

  /**
   * Follow a user
   */
  static async followUser(userId: string): Promise<Outcome<void>> {
    return apiClient.post(`/users/${userId}/follow`, {})
  }

  /**
   * Unfollow a user
   */
  static async unfollowUser(userId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/users/${userId}/follow`)
  }

  /**
   * Get user's followers
   */
  static async getFollowers(
    userId: string,
    limit: number = 50,
    offset: number = 0
  ): Promise<Outcome<UserListResponse>> {
    return apiClient.get<UserListResponse>(`/users/${userId}/followers`, {
      limit,
      offset,
    })
  }

  /**
   * Get user's following
   */
  static async getFollowing(
    userId: string,
    limit: number = 50,
    offset: number = 0
  ): Promise<Outcome<UserListResponse>> {
    return apiClient.get<UserListResponse>(`/users/${userId}/following`, {
      limit,
      offset,
    })
  }

  /**
   * Update own profile
   */
  static async updateProfile(data: {
    displayName?: string
    bio?: string
    profilePictureUrl?: string
    isPrivate?: boolean
    socialLinks?: {
      twitter?: string
      instagram?: string
      website?: string
    }
  }): Promise<Outcome<UserProfile>> {
    return apiClient.put<UserProfile>('/users/me', {
      display_name: data.displayName,
      bio: data.bio,
      profile_picture_url: data.profilePictureUrl,
      is_private: data.isPrivate,
      social_links: data.socialLinks,
    })
  }

  /**
   * Search users by username or display name
   */
  static async searchUsers(query: string, limit: number = 20): Promise<Outcome<UserListResponse>> {
    return apiClient.get<UserListResponse>('/users/search', {
      q: query,
      limit,
    })
  }

  /**
   * Get trending producers
   */
  static async getTrendingProducers(limit: number = 20): Promise<Outcome<UserListResponse>> {
    return apiClient.get<UserListResponse>('/users/trending', {
      limit,
    })
  }

  /**
   * Get suggested follows based on user behavior
   */
  static async getSuggestedFollows(limit: number = 10): Promise<Outcome<UserListResponse>> {
    return apiClient.get<UserListResponse>('/users/suggestions', {
      limit,
    })
  }
}
