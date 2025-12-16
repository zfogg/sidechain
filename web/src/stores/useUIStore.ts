import { create } from 'zustand'

/**
 * UI Store - Manages modal states, sidebar visibility, notifications, etc.
 * Mirrors C++ plugin's UI state management with Zustand
 */

export type NotificationType = 'success' | 'error' | 'info' | 'warning'

export interface Notification {
  id: string
  type: NotificationType
  title: string
  message: string
  duration?: number // ms, omit for persistent
  action?: {
    label: string
    onClick: () => void
  }
}

interface UIStoreState {
  // Modals
  isLoginDialogOpen: boolean
  isComposerDialogOpen: boolean
  isSettingsDialogOpen: boolean
  isProfileDialogOpen: boolean

  // Sidebars
  isSidebarOpen: boolean
  sidebarView: 'profile' | 'notifications' | 'messages'

  // Notifications
  notifications: Notification[]

  // Theme
  isDarkMode: boolean

  // Search
  isSearchOpen: boolean
  searchQuery: string

  // Loading states
  isPageLoading: boolean
  isComposingPost: boolean
}

interface UIStoreActions {
  // Modal toggles
  setLoginDialogOpen: (open: boolean) => void
  setComposerDialogOpen: (open: boolean) => void
  setSettingsDialogOpen: (open: boolean) => void
  setProfileDialogOpen: (open: boolean) => void

  // Sidebar
  setSidebarOpen: (open: boolean) => void
  setSidebarView: (view: 'profile' | 'notifications' | 'messages') => void

  // Notifications
  addNotification: (notification: Omit<Notification, 'id'>) => void
  removeNotification: (id: string) => void
  clearNotifications: () => void

  // Theme
  setDarkMode: (dark: boolean) => void
  toggleDarkMode: () => void

  // Search
  setSearchOpen: (open: boolean) => void
  setSearchQuery: (query: string) => void

  // Loading states
  setPageLoading: (loading: boolean) => void
  setComposingPost: (composing: boolean) => void
}

export const useUIStore = create<UIStoreState & UIStoreActions>((set, get) => ({
  // Initial state
  isLoginDialogOpen: false,
  isComposerDialogOpen: false,
  isSettingsDialogOpen: false,
  isProfileDialogOpen: false,

  isSidebarOpen: false,
  sidebarView: 'profile',

  notifications: [],

  isDarkMode: localStorage.getItem('theme') === 'dark' || false,

  isSearchOpen: false,
  searchQuery: '',

  isPageLoading: false,
  isComposingPost: false,

  // Actions
  setLoginDialogOpen: (open) => {
    set({ isLoginDialogOpen: open })
  },

  setComposerDialogOpen: (open) => {
    set({ isComposerDialogOpen: open })
  },

  setSettingsDialogOpen: (open) => {
    set({ isSettingsDialogOpen: open })
  },

  setProfileDialogOpen: (open) => {
    set({ isProfileDialogOpen: open })
  },

  setSidebarOpen: (open) => {
    set({ isSidebarOpen: open })
  },

  setSidebarView: (view) => {
    set({ sidebarView: view })
  },

  addNotification: (notification) => {
    const id = `notif-${Date.now()}`
    const newNotification = { ...notification, id }

    set((state) => ({
      notifications: [...state.notifications, newNotification],
    }))

    // Auto-remove after duration if specified
    if (notification.duration) {
      setTimeout(() => {
        get().removeNotification(id)
      }, notification.duration)
    }
  },

  removeNotification: (id) => {
    set((state) => ({
      notifications: state.notifications.filter((n) => n.id !== id),
    }))
  },

  clearNotifications: () => {
    set({ notifications: [] })
  },

  setDarkMode: (dark) => {
    set({ isDarkMode: dark })
    if (dark) {
      document.documentElement.classList.add('dark')
      localStorage.setItem('theme', 'dark')
    } else {
      document.documentElement.classList.remove('dark')
      localStorage.setItem('theme', 'light')
    }
  },

  toggleDarkMode: () => {
    const current = get().isDarkMode
    get().setDarkMode(!current)
  },

  setSearchOpen: (open) => {
    set({ isSearchOpen: open })
  },

  setSearchQuery: (query) => {
    set({ searchQuery: query })
  },

  setPageLoading: (loading) => {
    set({ isPageLoading: loading })
  },

  setComposingPost: (composing) => {
    set({ isComposingPost: composing })
  },
}))
