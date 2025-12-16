import { create } from 'zustand'
import { Notification } from '@/models/Notification'
import { NotificationClient } from '@/api/NotificationClient'

interface NotificationStoreState {
  notifications: Notification[]
  unreadCount: number
  isLoading: boolean
  error: string | null
  filter: 'all' | 'unread'
}

interface NotificationStoreActions {
  setFilter: (filter: 'all' | 'unread') => void
  addNotification: (notification: Notification) => void
  updateNotification: (id: string, updates: Partial<Notification>) => void
  removeNotification: (id: string) => void
  setNotifications: (notifications: Notification[], unreadCount: number) => void
  setLoading: (loading: boolean) => void
  setError: (error: string | null) => void
  markAsRead: (id: string) => Promise<void>
  deleteNotification: (id: string) => Promise<void>
  markAllAsRead: () => Promise<void>
  clearAllNotifications: () => Promise<void>
}

export const useNotificationStore = create<NotificationStoreState & NotificationStoreActions>(
  (set, get) => ({
    notifications: [],
    unreadCount: 0,
    isLoading: false,
    error: null,
    filter: 'all',

    setFilter: (filter) => set({ filter }),

    addNotification: (notification) => {
      set((state) => {
        const newNotifications = [notification, ...state.notifications]
        return {
          notifications: newNotifications,
          unreadCount: state.unreadCount + (notification.isRead ? 0 : 1),
        }
      })
    },

    updateNotification: (id, updates) => {
      set((state) => {
        const notification = state.notifications.find((n) => n.id === id)
        if (!notification) return state

        const wasRead = notification.isRead
        const willBeRead = updates.isRead !== undefined ? updates.isRead : notification.isRead

        return {
          notifications: state.notifications.map((n) =>
            n.id === id ? { ...n, ...updates } : n
          ),
          unreadCount:
            state.unreadCount + (wasRead === willBeRead ? 0 : wasRead ? 1 : -1),
        }
      })
    },

    removeNotification: (id) => {
      set((state) => {
        const notification = state.notifications.find((n) => n.id === id)
        return {
          notifications: state.notifications.filter((n) => n.id !== id),
          unreadCount: state.unreadCount - (notification?.isRead ? 0 : 1),
        }
      })
    },

    setNotifications: (notifications, unreadCount) => {
      set({ notifications, unreadCount })
    },

    setLoading: (loading) => set({ isLoading: loading }),

    setError: (error) => set({ error }),

    markAsRead: async (id) => {
      const { updateNotification } = get()
      const notification = get().notifications.find((n) => n.id === id)

      if (!notification || notification.isRead) return

      // Optimistic update
      updateNotification(id, { isRead: true })

      const result = await NotificationClient.markAsRead(id)
      if (result.isError()) {
        // Rollback on error
        updateNotification(id, { isRead: false })
        set({ error: result.getError() })
      }
    },

    deleteNotification: async (id) => {
      const { removeNotification } = get()

      // Optimistic update
      removeNotification(id)

      const result = await NotificationClient.deleteNotification(id)
      if (result.isError()) {
        // TODO: Rollback by re-fetching notifications
        set({ error: result.getError() })
      }
    },

    markAllAsRead: async () => {
      const { notifications } = get()

      // Optimistic update
      set((state) => ({
        notifications: state.notifications.map((n) => ({ ...n, isRead: true })),
        unreadCount: 0,
      }))

      const result = await NotificationClient.markAllAsRead()
      if (result.isError()) {
        // TODO: Rollback by re-fetching notifications
        set({ error: result.getError() })
      }
    },

    clearAllNotifications: async () => {
      // Optimistic update
      set({ notifications: [], unreadCount: 0 })

      const result = await NotificationClient.clearAllNotifications()
      if (result.isError()) {
        // TODO: Rollback by re-fetching notifications
        set({ error: result.getError() })
      }
    },
  })
)
