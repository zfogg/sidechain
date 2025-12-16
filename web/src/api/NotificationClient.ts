import { apiClient } from './client'
import { Outcome } from './types'
import { Notification, NotificationModel } from '@/models/Notification'

interface NotificationsResponse {
  notifications: Notification[]
  unreadCount: number
  total: number
  offset: number
  limit: number
}

/**
 * NotificationClient - Handles notification operations
 *
 * API Endpoints:
 * GET /notifications - Get user's notifications
 * GET /notifications/count - Get unread count
 * POST /notifications/:id/read - Mark notification as read
 * POST /notifications/read-all - Mark all as read
 * DELETE /notifications/:id - Delete notification
 * DELETE /notifications/clear - Clear all notifications
 * PUT /notifications/preferences - Update notification preferences
 */
export class NotificationClient {
  /**
   * Get user's notifications with pagination
   */
  static async getNotifications(
    limit: number = 20,
    offset: number = 0,
    unreadOnly: boolean = false
  ): Promise<Outcome<NotificationsResponse>> {
    const result = await apiClient.get<any>('/notifications', {
      limit,
      offset,
      unread_only: unreadOnly,
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    const data = result.getValue()
    return Outcome.ok({
      notifications: (data.notifications || []).map(NotificationModel.fromJson),
      unreadCount: data.unread_count || 0,
      total: data.total || 0,
      offset: data.offset || 0,
      limit: data.limit || 20,
    })
  }

  /**
   * Get unread notification count
   */
  static async getUnreadCount(): Promise<Outcome<number>> {
    const result = await apiClient.get<{ count: number }>('/notifications/count')

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    return Outcome.ok(result.getValue().count)
  }

  /**
   * Mark a single notification as read
   */
  static async markAsRead(notificationId: string): Promise<Outcome<void>> {
    return apiClient.post(`/notifications/${notificationId}/read`, {})
  }

  /**
   * Mark all notifications as read
   */
  static async markAllAsRead(): Promise<Outcome<void>> {
    return apiClient.post('/notifications/read-all', {})
  }

  /**
   * Delete a single notification
   */
  static async deleteNotification(notificationId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/notifications/${notificationId}`)
  }

  /**
   * Clear all notifications
   */
  static async clearAllNotifications(): Promise<Outcome<void>> {
    return apiClient.delete('/notifications/clear')
  }

  /**
   * Update notification preferences
   */
  static async updatePreferences(preferences: {
    notifyLikes?: boolean
    notifyComments?: boolean
    notifyFollows?: boolean
    notifyReposts?: boolean
    notifyMentions?: boolean
    emailDigestFrequency?: 'never' | 'daily' | 'weekly'
  }): Promise<Outcome<void>> {
    const data: any = {}
    if (preferences.notifyLikes !== undefined) data.notify_likes = preferences.notifyLikes
    if (preferences.notifyComments !== undefined) data.notify_comments = preferences.notifyComments
    if (preferences.notifyFollows !== undefined) data.notify_follows = preferences.notifyFollows
    if (preferences.notifyReposts !== undefined) data.notify_reposts = preferences.notifyReposts
    if (preferences.notifyMentions !== undefined) data.notify_mentions = preferences.notifyMentions
    if (preferences.emailDigestFrequency !== undefined)
      data.email_digest_frequency = preferences.emailDigestFrequency

    return apiClient.put('/notifications/preferences', data)
  }
}
