import { create } from 'zustand'
import { CollectionClient } from '@/api/CollectionClient'
import type { Collection } from '@/models/Collection'

interface CollectionStoreState {
  collections: Collection[]
  currentCollection: Collection | null
  isLoading: boolean
  error: string
}

interface CollectionStoreActions {
  loadCollections: () => Promise<void>
  loadCollection: (collectionId: string) => Promise<void>
  createCollection: (
    title: string,
    description?: string,
    icon?: string,
    isPublic?: boolean
  ) => Promise<void>
  updateCollection: (
    collectionId: string,
    updates: Partial<Collection>
  ) => Promise<void>
  deleteCollection: (collectionId: string) => Promise<void>
  addPostToCollection: (collectionId: string, postId: string) => Promise<void>
  removePostFromCollection: (collectionId: string, postId: string) => Promise<void>
  setCurrentCollection: (collection: Collection | null) => void
  clearError: () => void
}

export const useCollectionStore = create<CollectionStoreState & CollectionStoreActions>(
  (set) => ({
    collections: [],
    currentCollection: null,
    isLoading: false,
    error: '',

    loadCollections: async () => {
      set({ isLoading: true, error: '' })

      const result = await CollectionClient.getUserCollections()

      if (result.isOk()) {
        set({
          collections: result.getValue(),
          isLoading: false,
        })
      } else {
        set({
          error: result.getError(),
          isLoading: false,
        })
      }
    },

    loadCollection: async (collectionId) => {
      set({ isLoading: true, error: '' })

      const result = await CollectionClient.getCollection(collectionId)

      if (result.isOk()) {
        const collection = result.getValue()
        set({
          currentCollection: collection,
          isLoading: false,
        })
      } else {
        set({
          error: result.getError(),
          isLoading: false,
        })
      }
    },

    createCollection: async (title, description = '', icon = '📁', isPublic = false) => {
      set({ isLoading: true, error: '' })

      const result = await CollectionClient.createCollection(title, description, icon, isPublic)

      if (result.isOk()) {
        const newCollection = result.getValue()
        set((state) => ({
          collections: [...state.collections, newCollection],
          isLoading: false,
        }))
      } else {
        set({
          error: result.getError(),
          isLoading: false,
        })
      }
    },

    updateCollection: async (collectionId, updates) => {
      set({ isLoading: true, error: '' })

      const result = await CollectionClient.updateCollection(collectionId, {
        title: updates.title,
        description: updates.description,
        icon: updates.icon,
        isPublic: updates.isPublic,
      })

      if (result.isOk()) {
        const updated = result.getValue()
        set((state) => ({
          collections: state.collections.map((c) => (c.id === collectionId ? updated : c)),
          currentCollection:
            state.currentCollection?.id === collectionId ? updated : state.currentCollection,
          isLoading: false,
        }))
      } else {
        set({
          error: result.getError(),
          isLoading: false,
        })
      }
    },

    deleteCollection: async (collectionId) => {
      set({ isLoading: true, error: '' })

      const result = await CollectionClient.deleteCollection(collectionId)

      if (result.isOk()) {
        set((state) => ({
          collections: state.collections.filter((c) => c.id !== collectionId),
          currentCollection:
            state.currentCollection?.id === collectionId ? null : state.currentCollection,
          isLoading: false,
        }))
      } else {
        set({
          error: result.getError(),
          isLoading: false,
        })
      }
    },

    addPostToCollection: async (collectionId, postId) => {
      set({ error: '' })

      const result = await CollectionClient.addPostToCollection(collectionId, postId)

      if (result.isOk()) {
        // Update local collections
        set((state) => ({
          collections: state.collections.map((c) =>
            c.id === collectionId
              ? {
                  ...c,
                  postIds: [...c.postIds, postId],
                  postCount: c.postCount + 1,
                }
              : c
          ),
          currentCollection:
            state.currentCollection?.id === collectionId
              ? {
                  ...state.currentCollection,
                  postIds: [...state.currentCollection.postIds, postId],
                  postCount: state.currentCollection.postCount + 1,
                }
              : state.currentCollection,
        }))
      } else {
        set({
          error: result.getError(),
        })
      }
    },

    removePostFromCollection: async (collectionId, postId) => {
      set({ error: '' })

      const result = await CollectionClient.removePostFromCollection(collectionId, postId)

      if (result.isOk()) {
        set((state) => ({
          collections: state.collections.map((c) =>
            c.id === collectionId
              ? {
                  ...c,
                  postIds: c.postIds.filter((id) => id !== postId),
                  postCount: Math.max(0, c.postCount - 1),
                }
              : c
          ),
          currentCollection:
            state.currentCollection?.id === collectionId
              ? {
                  ...state.currentCollection,
                  postIds: state.currentCollection.postIds.filter((id) => id !== postId),
                  postCount: Math.max(0, state.currentCollection.postCount - 1),
                }
              : state.currentCollection,
        }))
      } else {
        set({
          error: result.getError(),
        })
      }
    },

    setCurrentCollection: (collection) => {
      set({ currentCollection: collection })
    },

    clearError: () => {
      set({ error: '' })
    },
  })
)
