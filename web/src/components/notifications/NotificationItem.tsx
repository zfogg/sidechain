import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Notification, NotificationModel } from '@/models/Notification'
import { NotificationClient } from '@/api/NotificationClient'
import { Button } from '@/components/ui/button'
import { formatDistanceToNow } from '@/utils/format'

interface NotificationItemProps {
  notification: Notification
  onUpdate: () => void
}

/**
 * NotificationItem - Single notification display with actions
 */
export function NotificationItem({ notification, onUpdate }: NotificationItemProps) {
  const navigate = useNavigate()
  const [isDeleting, setIsDeleting] = useState(false)
  const [isMarking, setIsMarking] = useState(false)

  const handleMarkAsRead = async () => {
    setIsMarking(true)
    try {
      await NotificationClient.markAsRead(notification.id)
      onUpdate()
    } finally {
      setIsMarking(false)
    }
  }

  const handleDelete = async () => {
    setIsDeleting(true)
    try {
      await NotificationClient.deleteNotification(notification.id)
      onUpdate()
    } finally {
      setIsDeleting(false)
    }
  }

  const handleNavigate = () => {
    // Navigate to relevant page based on notification type
    if (notification.targetType === 'post' && notification.targetId) {
      // Navigate to post (would need post detail page)
      console.log('Navigate to post:', notification.targetId)
    } else if (notification.targetType === 'user' && notification.userId) {
      navigate(`/profile/${notification.userUsername}`)
    } else if (notification.targetType === 'comment' && notification.targetId) {
      // Navigate to post with comment highlighted
      console.log('Navigate to comment:', notification.targetId)
    }
  }

  const icon = NotificationModel.getIcon(notification.type)
  const message = NotificationModel.getMessage(notification)
  const timeAgo = formatDistanceToNow(notification.createdAt)

  return (
    <div
      className={`
        p-4 rounded-lg border transition-all
        ${notification.isRead
          ? 'bg-bg-secondary/30 border-border/20 hover:bg-bg-secondary/50'
          : 'bg-bg-secondary/80 border-coral-pink/30 hover:bg-bg-secondary/90'}
      `}
    >
      <div className="flex gap-3">
        {/* Avatar */}
        <img
          src={
            notification.userAvatarUrl ||
            `https://api.dicebear.com/7.x/avataaars/svg?seed=${notification.userId}`
          }
          alt={notification.userDisplayName}
          className="w-10 h-10 rounded-full flex-shrink-0 cursor-pointer hover:opacity-80 transition-opacity"
          onClick={() => navigate(`/profile/${notification.userUsername}`)}
        />

        {/* Content */}
        <div className="flex-1 min-w-0">
          <div className="flex items-start justify-between gap-2">
            <div
              className="flex-1 cursor-pointer hover:text-coral-pink transition-colors"
              onClick={handleNavigate}
            >
              <div className="text-sm text-foreground">
                <span className="font-semibold">{notification.userDisplayName}</span>
                <span className="text-muted-foreground"> {message}</span>
              </div>
              <div className="text-xs text-muted-foreground mt-1">{timeAgo}</div>
            </div>

            {/* Icon and unread indicator */}
            <div className="flex items-center gap-2 flex-shrink-0">
              <span className="text-xl">{icon}</span>
              {!notification.isRead && <div className="w-2 h-2 bg-coral-pink rounded-full" />}
            </div>
          </div>

          {/* Actions */}
          <div className="flex gap-2 mt-2">
            {!notification.isRead && (
              <Button
                onClick={handleMarkAsRead}
                disabled={isMarking}
                size="sm"
                variant="ghost"
                className="h-7 text-xs text-muted-foreground hover:text-foreground"
              >
                Mark as read
              </Button>
            )}
            <Button
              onClick={handleDelete}
              disabled={isDeleting}
              size="sm"
              variant="ghost"
              className="h-7 text-xs text-muted-foreground hover:text-red-400"
            >
              Delete
            </Button>
          </div>
        </div>
      </div>
    </div>
  )
}
