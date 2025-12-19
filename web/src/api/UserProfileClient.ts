/**
 * UserProfileClient - API client for user profile operations
 * Handles profile updates, picture uploads, and bio management
 */

import { apiClient } from './client'
import { Outcome } from './types'

export interface ProfileUpdatePayload {
  username?: string
  displayName?: string
  bio?: string
  website?: string
  profilePictureUrl?: string
}

export interface ProfilePictureUpdatePayload {
  profilePictureUrl?: string
}

export class UserProfileClient {
  /**
   * Update user profile metadata
   */
  static async updateProfile(updates: ProfileUpdatePayload): Promise<Outcome<any>> {
    const payload: any = {}

    // Always send fields that are explicitly provided (even if empty string)
    // This allows clearing fields and ensures backend receives all intended updates
    if (updates.username !== undefined) payload.username = updates.username
    if (updates.displayName !== undefined) payload.display_name = updates.displayName
    if (updates.bio !== undefined) payload.bio = updates.bio
    if (updates.website !== undefined) payload.website = updates.website
    if (updates.profilePictureUrl !== undefined) payload.profile_picture_url = updates.profilePictureUrl

    return apiClient.put('/users/me', payload)
  }

  /**
   * Upload profile picture
   * Returns URL of uploaded image
   */
  static async uploadProfilePicture(file: File): Promise<Outcome<{ url: string }>> {
    const formData = new FormData()
    formData.append('file', file)
    formData.append('type', 'profile_picture')

    try {
      const response = await fetch(`${import.meta.env.VITE_API_URL}/api/v1/users/upload-profile-picture`, {
        method: 'POST',
        headers: {
          Authorization: `Bearer ${localStorage.getItem('auth_token')}`,
        },
        body: formData,
      })

      if (!response.ok) {
        return Outcome.error('Failed to upload profile picture')
      }

      const data = await response.json()
      return Outcome.ok({ url: data.url })
    } catch (error) {
      return Outcome.error(`Upload error: ${error instanceof Error ? error.message : 'Unknown error'}`)
    }
  }

  /**
   * Delete profile picture
   */
  static async deleteProfilePicture(): Promise<Outcome<void>> {
    return apiClient.delete('/users/profile/picture')
  }

  /**
   * Update profile privacy settings
   */
  static async updatePrivacySettings(settings: {
    isPrivate?: boolean
    allowMessages?: boolean
    allowFollowRequests?: boolean
  }): Promise<Outcome<any>> {
    const payload: any = {}

    if (settings.isPrivate !== undefined) payload.is_private = settings.isPrivate
    if (settings.allowMessages !== undefined) payload.allow_messages = settings.allowMessages
    if (settings.allowFollowRequests !== undefined)
      payload.allow_follow_requests = settings.allowFollowRequests

    return apiClient.put('/users/profile/privacy', payload)
  }

  /**
   * Block a user
   */
  static async blockUser(userId: string): Promise<Outcome<void>> {
    return apiClient.post(`/users/${userId}/block`, {})
  }

  /**
   * Unblock a user
   */
  static async unblockUser(userId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/users/${userId}/block`)
  }

  /**
   * Get blocked users list
   */
  static async getBlockedUsers(limit: number = 20, offset: number = 0): Promise<Outcome<any[]>> {
    const result = await apiClient.get<{ blocked_users: any[] }>('/users/blocked', { limit, offset })

    if (result.isOk()) {
      return Outcome.ok(result.getValue().blocked_users)
    }

    return Outcome.error(result.getError())
  }
}
