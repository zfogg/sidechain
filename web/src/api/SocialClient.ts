import { apiClient } from './client'
import { Outcome } from './types'

/**
 * SocialClient - Advanced social interactions (follow, block, mute, archive, pin, repost, etc.)
 * Mirrors C++ SocialClient.cpp
 */

export class SocialClient {
  // ============== Follow/Unfollow ==============

  /**
   * Follow a user
   */
  static async followUser(userId: string): Promise<Outcome<void>> {
    const result = await apiClient.post('/social/follow', {
      user_id: userId,
    })

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Unfollow a user
   */
  static async unfollowUser(userId: string): Promise<Outcome<void>> {
    const result = await apiClient.delete(`/social/follow?user_id=${userId}`)

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Accept a follow request (for private accounts)
   */
  static async acceptFollowRequest(userId: string): Promise<Outcome<void>> {
    const result = await apiClient.post('/social/follow-request/accept', {
      user_id: userId,
    })

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Reject a follow request (for private accounts)
   */
  static async rejectFollowRequest(userId: string): Promise<Outcome<void>> {
    const result = await apiClient.post('/social/follow-request/reject', {
      user_id: userId,
    })

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get pending follow requests for current user (private account)
   */
  static async getPendingFollowRequests(
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Array<{ id: string; username: string; displayName: string; profilePictureUrl?: string }>>> {
    interface FollowRequest {
      id: string
      username: string
      display_name: string
      profile_picture_url?: string
    }

    interface FollowRequestsResponse {
      follow_requests: FollowRequest[]
    }

    const result = await apiClient.get<FollowRequestsResponse>('/social/follow-requests', {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const requests = result.getValue().follow_requests.map((r) => ({
          id: r.id,
          username: r.username,
          displayName: r.display_name,
          profilePictureUrl: r.profile_picture_url,
        }))
        return Outcome.ok(requests)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  // ============== Block/Mute ==============

  /**
   * Block a user (hide all content and prevent interaction)
   */
  static async blockUser(userId: string): Promise<Outcome<void>> {
    const result = await apiClient.post('/social/block', {
      user_id: userId,
    })

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Unblock a user
   */
  static async unblockUser(userId: string): Promise<Outcome<void>> {
    const result = await apiClient.delete(`/social/block?user_id=${userId}`)

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Mute a user (hide their posts without blocking)
   */
  static async muteUser(userId: string): Promise<Outcome<void>> {
    const result = await apiClient.post('/social/mute', {
      user_id: userId,
    })

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Unmute a user
   */
  static async unmuteUser(userId: string): Promise<Outcome<void>> {
    const result = await apiClient.delete(`/social/mute?user_id=${userId}`)

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  // ============== Post Interactions ==============

  /**
   * Archive a post (hide from profile and feeds)
   */
  static async archivePost(postId: string): Promise<Outcome<void>> {
    const result = await apiClient.post(`/posts/${postId}/archive`, {})

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Unarchive a post
   */
  static async unarchivePost(postId: string): Promise<Outcome<void>> {
    const result = await apiClient.delete(`/posts/${postId}/archive`)

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Pin a post to profile (max 3)
   */
  static async pinPost(postId: string): Promise<Outcome<void>> {
    const result = await apiClient.post(`/posts/${postId}/pin`, {})

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Unpin a post from profile
   */
  static async unpinPost(postId: string): Promise<Outcome<void>> {
    const result = await apiClient.delete(`/posts/${postId}/pin`)

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Repost a post
   */
  static async repostPost(postId: string): Promise<Outcome<void>> {
    const result = await apiClient.post(`/posts/${postId}/repost`, {})

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Repost with a quote/comment
   */
  static async repostWithQuote(postId: string, quote: string): Promise<Outcome<void>> {
    const result = await apiClient.post(`/posts/${postId}/repost`, {
      quote,
    })

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Remove a repost
   */
  static async removeRepost(postId: string): Promise<Outcome<void>> {
    const result = await apiClient.delete(`/posts/${postId}/repost`)

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  // ============== Saved/Collections ==============

  /**
   * Save a post (for later)
   */
  static async savePost(postId: string): Promise<Outcome<void>> {
    const result = await apiClient.post(`/posts/${postId}/save`, {})

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Unsave a post
   */
  static async unsavePost(postId: string): Promise<Outcome<void>> {
    const result = await apiClient.delete(`/posts/${postId}/save`)

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }

  // ============== Reporting ==============

  /**
   * Report a post or user
   */
  static async report(
    targetId: string,
    targetType: 'post' | 'user' | 'comment',
    reason: string,
    description?: string
  ): Promise<Outcome<void>> {
    const result = await apiClient.post('/reports', {
      target_id: targetId,
      target_type: targetType,
      reason,
      description,
    })

    if (result.isOk()) {
      return Outcome.ok(undefined)
    }

    return Outcome.error(result.getError())
  }
}
