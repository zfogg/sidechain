import { useNavigate } from 'react-router-dom'
import { useQuery } from '@tanstack/react-query'
import { NotificationClient } from '@/api/NotificationClient'
import { useNotificationStore } from '@/stores/useNotificationStore'

/**
 * NotificationBell - Bell icon with unread count badge
 * Navigates to /notifications on click
 */
export function NotificationBell() {
  const navigate = useNavigate()
  const { unreadCount, setNotifications } = useNotificationStore()

  // Keep unread count updated via React Query
  const { data } = useQuery({
    queryKey: ['notification-count'],
    queryFn: async () => {
      const result = await NotificationClient.getUnreadCount()
      if (result.isOk()) {
        return result.getValue()
      }
      // Return 0 instead of throwing error - handle gracefully if endpoint doesn't exist
      console.debug('[NotificationBell] Failed to get unread count:', result.getError())
      return 0
    },
    refetchInterval: 30000, // Update every 30 seconds
    staleTime: 10000, // Consider fresh for 10 seconds
  })

  // Update store when count changes
  if (data !== undefined && data !== unreadCount) {
    setNotifications([], data)
  }

  const displayCount = data ?? unreadCount

  return (
    <button
      onClick={() => navigate('/notifications')}
      className="relative p-2 text-foreground hover:text-coral-pink transition-colors"
      title={displayCount > 0 ? `${displayCount} unread notifications` : 'No notifications'}
    >
      {/* Bell Icon */}
      <svg
        className="w-6 h-6"
        fill="none"
        stroke="currentColor"
        viewBox="0 0 24 24"
      >
        <path
          strokeLinecap="round"
          strokeLinejoin="round"
          strokeWidth={2}
          d="M15 17h5l-1.405-1.405A2.032 2.032 0 0118 14.158V11a6.002 6.002 0 00-4-5.659V5a2 2 0 10-4 0v.341C7.67 6.165 6 8.388 6 11v3.159c0 .538-.214 1.055-.595 1.436L4 17h5m6 0v1a3 3 0 11-6 0v-1m6 0H9"
        />
      </svg>

      {/* Unread Badge */}
      {displayCount > 0 && (
        <span className="absolute top-1 right-1 inline-flex items-center justify-center px-2 py-1 text-xs font-bold leading-none text-white transform translate-x-1/2 -translate-y-1/2 bg-coral-pink rounded-full">
          {displayCount > 99 ? '99+' : displayCount}
        </span>
      )}
    </button>
  )
}
