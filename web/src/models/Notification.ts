export type NotificationType =
  | 'post_liked'
  | 'post_commented'
  | 'comment_replied'
  | 'comment_liked'
  | 'user_followed'
  | 'user_followed_back'
  | 'mention'
  | 'post_reposted'
  | 'playlist_shared'

export interface Notification {
  id: string
  type: NotificationType
  userId: string
  userDisplayName: string
  userUsername: string
  userAvatarUrl?: string
  targetId?: string // Post ID, comment ID, etc.
  targetType?: 'post' | 'comment' | 'user'
  message: string
  isRead: boolean
  createdAt: Date
  updatedAt: Date
}

export class NotificationModel {
  static fromJson(json: any): Notification {
    return {
      id: json.id || '',
      type: json.type || 'post_liked',
      userId: json.user_id || '',
      userDisplayName: json.user_display_name || '',
      userUsername: json.user_username || '',
      userAvatarUrl: json.user_avatar_url,
      targetId: json.target_id,
      targetType: json.target_type,
      message: json.message || '',
      isRead: json.is_read || false,
      createdAt: new Date(json.created_at || Date.now()),
      updatedAt: new Date(json.updated_at || Date.now()),
    }
  }

  static toJson(notification: Notification): any {
    return {
      id: notification.id,
      type: notification.type,
      user_id: notification.userId,
      user_display_name: notification.userDisplayName,
      user_username: notification.userUsername,
      user_avatar_url: notification.userAvatarUrl,
      target_id: notification.targetId,
      target_type: notification.targetType,
      message: notification.message,
      is_read: notification.isRead,
      created_at: notification.createdAt.toISOString(),
      updated_at: notification.updatedAt.toISOString(),
    }
  }

  static getIcon(type: NotificationType): string {
    switch (type) {
      case 'post_liked':
        return '❤️'
      case 'post_commented':
        return '💬'
      case 'comment_replied':
        return '↩️'
      case 'comment_liked':
        return '❤️'
      case 'user_followed':
        return '👤'
      case 'user_followed_back':
        return '👥'
      case 'mention':
        return '@'
      case 'post_reposted':
        return '🔄'
      case 'playlist_shared':
        return '📋'
      default:
        return '🔔'
    }
  }

  static getMessage(notification: Notification): string {
    switch (notification.type) {
      case 'post_liked':
        return `${notification.userDisplayName} liked your post`
      case 'post_commented':
        return `${notification.userDisplayName} commented on your post`
      case 'comment_replied':
        return `${notification.userDisplayName} replied to your comment`
      case 'comment_liked':
        return `${notification.userDisplayName} liked your comment`
      case 'user_followed':
        return `${notification.userDisplayName} started following you`
      case 'user_followed_back':
        return `${notification.userDisplayName} followed you back`
      case 'mention':
        return `${notification.userDisplayName} mentioned you`
      case 'post_reposted':
        return `${notification.userDisplayName} reposted your post`
      case 'playlist_shared':
        return `${notification.userDisplayName} shared a playlist with you`
      default:
        return notification.message
    }
  }
}
