import { create } from 'zustand'
import { persist } from 'zustand/middleware'

/**
 * Draft Store - Manages draft posts before they're published
 * Persists to localStorage so drafts aren't lost on page refresh
 */

export interface PostDraft {
  id: string
  filename: string
  audioUrl?: string // Local preview URL
  bpm?: number
  key?: string
  daw?: string
  genre: string[]
  description?: string
  isPublic: boolean
  createdAt: Date
  updatedAt: Date
}

interface DraftStoreState {
  drafts: PostDraft[]
  selectedDraftId: string | null
  isUploadingFile: boolean
  uploadProgress: number // 0-100
}

interface DraftStoreActions {
  // Drafts
  createDraft: () => string // Returns draft ID
  updateDraft: (id: string, updates: Partial<PostDraft>) => void
  deleteDraft: (id: string) => void
  getDraft: (id: string) => PostDraft | undefined

  // Selection
  selectDraft: (id: string | null) => void

  // Upload
  setUploadingFile: (uploading: boolean) => void
  setUploadProgress: (progress: number) => void

  // Cleanup
  clearAllDrafts: () => void
}

export const useDraftStore = create<DraftStoreState & DraftStoreActions>()(
  persist(
    (set, get) => ({
      // Initial state
      drafts: [],
      selectedDraftId: null,
      isUploadingFile: false,
      uploadProgress: 0,

      // Actions
      createDraft: () => {
        const id = `draft-${Date.now()}`
        const newDraft: PostDraft = {
          id,
          filename: 'Untitled Loop',
          genre: [],
          isPublic: true,
          createdAt: new Date(),
          updatedAt: new Date(),
        }

        set((state) => ({
          drafts: [...state.drafts, newDraft],
          selectedDraftId: id,
        }))

        return id
      },

      updateDraft: (id, updates) => {
        set((state) => ({
          drafts: state.drafts.map((d) =>
            d.id === id
              ? {
                  ...d,
                  ...updates,
                  updatedAt: new Date(),
                }
              : d
          ),
        }))
      },

      deleteDraft: (id) => {
        set((state) => ({
          drafts: state.drafts.filter((d) => d.id !== id),
          selectedDraftId:
            state.selectedDraftId === id ? null : state.selectedDraftId,
        }))
      },

      getDraft: (id) => {
        return get().drafts.find((d) => d.id === id)
      },

      selectDraft: (id) => {
        set({ selectedDraftId: id })
      },

      setUploadingFile: (uploading) => {
        set({
          isUploadingFile: uploading,
          uploadProgress: uploading ? 0 : 100,
        })
      },

      setUploadProgress: (progress) => {
        set({ uploadProgress: Math.min(Math.max(progress, 0), 100) })
      },

      clearAllDrafts: () => {
        set({
          drafts: [],
          selectedDraftId: null,
        })
      },
    }),
    {
      name: 'draft-store',
      // Only persist drafts, not selectedDraftId or upload state
      partialize: (state) => ({
        drafts: state.drafts,
      }),
    }
  )
)
