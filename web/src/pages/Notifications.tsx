import { useState } from 'react'
import { useInfiniteQuery } from '@tanstack/react-query'
import { NotificationClient } from '@/api/NotificationClient'
import { NotificationItem } from '@/components/notifications/NotificationItem'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'

type NotificationFilter = 'all' | 'unread'

/**
 * Notifications - Notification center page
 *
 * Features:
 * - View all notifications with pagination
 * - Filter unread/all
 * - Mark notifications as read
 * - Delete notifications
 * - Mark all as read
 * - Real-time updates via WebSocket
 */
export function Notifications() {
  const [filter, setFilter] = useState<NotificationFilter>('all')

  const {
    data,
    isLoading,
    isFetchingNextPage,
    hasNextPage,
    fetchNextPage,
    error,
    refetch,
  } = useInfiniteQuery({
    queryKey: ['notifications', filter],
    queryFn: async ({ pageParam = 0 }) => {
      const result = await NotificationClient.getNotifications(20, pageParam, filter === 'unread')

      if (result.isError()) {
        throw new Error(result.getError())
      }

      return result.getValue()
    },
    getNextPageParam: (lastPage) => {
      const nextOffset = lastPage.offset + lastPage.limit
      if (nextOffset < lastPage.total) {
        return nextOffset
      }
      return undefined
    },
    initialPageParam: 0,
  })

  const notifications = data?.pages.flatMap((page) => page.notifications) || []
  const unreadCount = data?.pages[0]?.unreadCount || 0

  const handleMarkAllAsRead = async () => {
    await NotificationClient.markAllAsRead()
    refetch()
  }

  const handleClearAll = async () => {
    if (confirm('Are you sure? This action cannot be undone.')) {
      await NotificationClient.clearAllNotifications()
      refetch()
    }
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Header */}
      <div className="sticky top-0 z-40 bg-card/95 backdrop-blur border-b border-border">
        <div className="max-w-2xl mx-auto px-4 py-6">
          <div className="flex items-center justify-between mb-4">
            <h1 className="text-3xl font-bold text-foreground">Notifications</h1>
            {unreadCount > 0 && (
              <div className="bg-coral-pink text-white px-3 py-1 rounded-full text-sm font-semibold">
                {unreadCount} new
              </div>
            )}
          </div>

          {/* Filters and Actions */}
          <div className="flex gap-2 items-center justify-between">
            <div className="flex gap-2">
              <Button
                variant={filter === 'all' ? 'default' : 'outline'}
                onClick={() => setFilter('all')}
                size="sm"
                className="text-sm"
              >
                All
              </Button>
              <Button
                variant={filter === 'unread' ? 'default' : 'outline'}
                onClick={() => setFilter('unread')}
                size="sm"
                className="text-sm"
              >
                Unread ({unreadCount})
              </Button>
            </div>

            <div className="flex gap-2">
              {notifications.length > 0 && (
                <>
                  <Button
                    onClick={handleMarkAllAsRead}
                    variant="ghost"
                    size="sm"
                    className="text-xs text-muted-foreground hover:text-foreground"
                  >
                    Mark all as read
                  </Button>
                  <Button
                    onClick={handleClearAll}
                    variant="ghost"
                    size="sm"
                    className="text-xs text-red-400 hover:text-red-300"
                  >
                    Clear all
                  </Button>
                </>
              )}
            </div>
          </div>
        </div>
      </div>

      {/* Content */}
      <main className="max-w-2xl mx-auto px-4 py-8">
        {isLoading ? (
          <div className="flex justify-center py-12">
            <Spinner size="lg" />
          </div>
        ) : error ? (
          <div className="bg-card border border-border rounded-lg p-6 text-center space-y-4">
            <div className="text-destructive font-medium">Failed to load notifications</div>
            <Button onClick={() => refetch()}>Retry</Button>
          </div>
        ) : notifications.length === 0 ? (
          <div className="bg-card border border-border rounded-lg p-12 text-center space-y-4">
            <div className="text-4xl">🔔</div>
            <div className="text-muted-foreground text-lg">
              {filter === 'unread' ? "You're all caught up!" : 'No notifications yet'}
            </div>
            <div className="text-sm text-muted-foreground">
              {filter === 'unread'
                ? 'Check back later for new updates'
                : 'Follow creators and interact with posts to get notifications'}
            </div>
          </div>
        ) : (
          <div className="space-y-3">
            {notifications.map((notification) => (
              <NotificationItem key={notification.id} notification={notification} onUpdate={() => refetch()} />
            ))}

            {/* Load more */}
            {hasNextPage && (
              <div className="flex justify-center pt-4">
                <Button
                  onClick={() => fetchNextPage()}
                  disabled={isFetchingNextPage}
                  variant="outline"
                  className="border-border/50 text-foreground text-sm"
                >
                  {isFetchingNextPage ? 'Loading...' : 'Load More'}
                </Button>
              </div>
            )}
          </div>
        )}
      </main>
    </div>
  )
}
