import { create } from 'zustand'
import { Playlist } from '@/models/Playlist'
import { PlaylistClient } from '@/api/PlaylistClient'

interface PlaylistsState {
  userPlaylists: Playlist[]
  currentPlaylist: Playlist | null
  isLoading: boolean
  error: string
}

interface PlaylistsActions {
  loadUserPlaylists: () => Promise<void>
  loadPlaylist: (playlistId: string) => Promise<void>
  createPlaylist: (title: string, description?: string) => Promise<void>
  updatePlaylist: (playlistId: string, updates: any) => Promise<void>
  deletePlaylist: (playlistId: string) => Promise<void>
  addTrackToPlaylist: (playlistId: string, postId: string) => Promise<void>
  removeTrackFromPlaylist: (playlistId: string, postId: string) => Promise<void>
  toggleFollowPlaylist: (playlistId: string, shouldFollow: boolean) => Promise<void>
}

export const usePlaylistStore = create<PlaylistsState & PlaylistsActions>((set, get) => ({
  userPlaylists: [],
  currentPlaylist: null,
  isLoading: false,
  error: '',

  loadUserPlaylists: async () => {
    set({ isLoading: true, error: '' })

    const result = await PlaylistClient.getUserPlaylists()

    if (result.isOk()) {
      set({ userPlaylists: result.getValue(), isLoading: false })
    } else {
      set({ error: result.getError(), isLoading: false })
    }
  },

  loadPlaylist: async (playlistId) => {
    set({ isLoading: true, error: '' })

    const result = await PlaylistClient.getPlaylist(playlistId)

    if (result.isOk()) {
      set({ currentPlaylist: result.getValue(), isLoading: false })
    } else {
      set({ error: result.getError(), isLoading: false })
    }
  },

  createPlaylist: async (title, description = '') => {
    set({ isLoading: true, error: '' })

    const result = await PlaylistClient.createPlaylist(title, description)

    if (result.isOk()) {
      const newPlaylist = result.getValue()
      set((state) => ({
        userPlaylists: [...state.userPlaylists, newPlaylist],
        currentPlaylist: newPlaylist,
        isLoading: false,
      }))
    } else {
      set({ error: result.getError(), isLoading: false })
    }
  },

  updatePlaylist: async (playlistId, updates) => {
    set({ error: '' })

    const result = await PlaylistClient.updatePlaylist(playlistId, updates)

    if (result.isOk()) {
      const updated = result.getValue()

      set((state) => ({
        userPlaylists: state.userPlaylists.map((p) => (p.id === playlistId ? updated : p)),
        currentPlaylist: state.currentPlaylist?.id === playlistId ? updated : state.currentPlaylist,
      }))
    } else {
      set({ error: result.getError() })
    }
  },

  deletePlaylist: async (playlistId) => {
    set({ error: '' })

    const result = await PlaylistClient.deletePlaylist(playlistId)

    if (result.isOk()) {
      set((state) => ({
        userPlaylists: state.userPlaylists.filter((p) => p.id !== playlistId),
        currentPlaylist:
          state.currentPlaylist?.id === playlistId ? null : state.currentPlaylist,
      }))
    } else {
      set({ error: result.getError() })
    }
  },

  addTrackToPlaylist: async (playlistId, postId) => {
    const result = await PlaylistClient.addTrackToPlaylist(playlistId, postId)

    if (result.isOk()) {
      const updated = result.getValue()

      set((state) => ({
        userPlaylists: state.userPlaylists.map((p) => (p.id === playlistId ? updated : p)),
        currentPlaylist: state.currentPlaylist?.id === playlistId ? updated : state.currentPlaylist,
      }))
    } else {
      set({ error: result.getError() })
    }
  },

  removeTrackFromPlaylist: async (playlistId, postId) => {
    const result = await PlaylistClient.removeTrackFromPlaylist(playlistId, postId)

    if (result.isOk()) {
      const updated = result.getValue()

      set((state) => ({
        userPlaylists: state.userPlaylists.map((p) => (p.id === playlistId ? updated : p)),
        currentPlaylist: state.currentPlaylist?.id === playlistId ? updated : state.currentPlaylist,
      }))
    } else {
      set({ error: result.getError() })
    }
  },

  toggleFollowPlaylist: async (playlistId, shouldFollow) => {
    await PlaylistClient.toggleFollowPlaylist(playlistId, shouldFollow)

    set((state) => ({
      currentPlaylist: state.currentPlaylist
        ? {
            ...state.currentPlaylist,
            isFollowing: shouldFollow,
            followerCount: shouldFollow
              ? state.currentPlaylist.followerCount + 1
              : state.currentPlaylist.followerCount - 1,
          }
        : null,
    }))
  },
}))
