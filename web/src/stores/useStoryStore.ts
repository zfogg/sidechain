import { create } from 'zustand'
import { StoryClient } from '@/api/StoryClient'
import type { Story } from '@/models/Story'

interface StoryStoreState {
  userActivities: Story[]
  followingActivities: Story[]
  globalActivities: Story[]
  currentUserId: string | null
  isLoading: boolean
  error: string
}

interface StoryStoreActions {
  loadUserActivities: (userId: string) => Promise<void>
  loadFollowingActivities: () => Promise<void>
  loadGlobalActivities: () => Promise<void>
  clearError: () => void
}

export const useStoryStore = create<StoryStoreState & StoryStoreActions>((set, get) => ({
  userActivities: [],
  followingActivities: [],
  globalActivities: [],
  currentUserId: null,
  isLoading: false,
  error: '',

  loadUserActivities: async (userId) => {
    set({ isLoading: true, error: '', currentUserId: userId })

    const result = await StoryClient.getUserActivityTimeline(userId)

    if (result.isOk()) {
      set({
        userActivities: result.getValue(),
        isLoading: false,
      })
    } else {
      set({
        error: result.getError(),
        isLoading: false,
      })
    }
  },

  loadFollowingActivities: async () => {
    set({ isLoading: true, error: '' })

    const result = await StoryClient.getFollowingActivityTimeline()

    if (result.isOk()) {
      set({
        followingActivities: result.getValue(),
        isLoading: false,
      })
    } else {
      set({
        error: result.getError(),
        isLoading: false,
      })
    }
  },

  loadGlobalActivities: async () => {
    set({ isLoading: true, error: '' })

    const result = await StoryClient.getGlobalActivityTimeline()

    if (result.isOk()) {
      set({
        globalActivities: result.getValue(),
        isLoading: false,
      })
    } else {
      set({
        error: result.getError(),
        isLoading: false,
      })
    }
  },

  clearError: () => {
    set({ error: '' })
  },
}))
