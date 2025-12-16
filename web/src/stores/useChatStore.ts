import { create } from 'zustand'

/**
 * Chat Store - Manages Stream Chat state and active conversations
 * Integrates with getstream.io Chat SDK
 */

export interface ChatChannel {
  id: string
  name: string
  members: string[]
  lastMessage?: string
  lastMessageTime?: Date
  unreadCount: number
}

export interface ChatMessage {
  id: string
  channelId: string
  userId: string
  content: string
  createdAt: Date
  reactions: Record<string, number>
  isEdited: boolean
}

interface ChatStoreState {
  // Stream Chat token
  streamToken: string | null
  isStreamConnected: boolean

  // Channels
  channels: ChatChannel[]
  activeChannelId: string | null

  // Messages
  messages: Record<string, ChatMessage[]> // key: channelId

  // Typing indicators
  typingUsers: Record<string, string[]> // key: channelId, value: userIds

  // Loading states
  isLoadingChannels: boolean
  isLoadingMessages: boolean
}

interface ChatStoreActions {
  // Connection
  setStreamToken: (token: string) => void
  setStreamConnected: (connected: boolean) => void

  // Channels
  setChannels: (channels: ChatChannel[]) => void
  addChannel: (channel: ChatChannel) => void
  removeChannel: (channelId: string) => void
  setActiveChannelId: (channelId: string | null) => void
  updateChannelUnreadCount: (channelId: string, count: number) => void

  // Messages
  setMessages: (channelId: string, messages: ChatMessage[]) => void
  addMessage: (channelId: string, message: ChatMessage) => void
  updateMessage: (channelId: string, messageId: string, updates: Partial<ChatMessage>) => void
  deleteMessage: (channelId: string, messageId: string) => void
  setTypingUsers: (channelId: string, userIds: string[]) => void

  // Loading states
  setLoadingChannels: (loading: boolean) => void
  setLoadingMessages: (loading: boolean) => void

  // Cleanup
  clearChat: () => void
}

export const useChatStore = create<ChatStoreState & ChatStoreActions>((set, get) => ({
  // Initial state
  streamToken: null,
  isStreamConnected: false,

  channels: [],
  activeChannelId: null,

  messages: {},
  typingUsers: {},

  isLoadingChannels: false,
  isLoadingMessages: false,

  // Actions
  setStreamToken: (token) => {
    set({ streamToken: token })
  },

  setStreamConnected: (connected) => {
    set({ isStreamConnected: connected })
  },

  setChannels: (channels) => {
    set({ channels })
  },

  addChannel: (channel) => {
    set((state) => {
      // Check if channel already exists
      if (state.channels.find((c) => c.id === channel.id)) {
        return state
      }
      return {
        channels: [...state.channels, channel],
      }
    })
  },

  removeChannel: (channelId) => {
    set((state) => ({
      channels: state.channels.filter((c) => c.id !== channelId),
    }))
  },

  setActiveChannelId: (channelId) => {
    set({ activeChannelId: channelId })
  },

  updateChannelUnreadCount: (channelId, count) => {
    set((state) => ({
      channels: state.channels.map((c) =>
        c.id === channelId ? { ...c, unreadCount: count } : c
      ),
    }))
  },

  setMessages: (channelId, messages) => {
    set((state) => ({
      messages: {
        ...state.messages,
        [channelId]: messages,
      },
    }))
  },

  addMessage: (channelId, message) => {
    set((state) => ({
      messages: {
        ...state.messages,
        [channelId]: [...(state.messages[channelId] || []), message],
      },
    }))
  },

  updateMessage: (channelId, messageId, updates) => {
    set((state) => ({
      messages: {
        ...state.messages,
        [channelId]: (state.messages[channelId] || []).map((m) =>
          m.id === messageId ? { ...m, ...updates } : m
        ),
      },
    }))
  },

  deleteMessage: (channelId, messageId) => {
    set((state) => ({
      messages: {
        ...state.messages,
        [channelId]: (state.messages[channelId] || []).filter((m) => m.id !== messageId),
      },
    }))
  },

  setTypingUsers: (channelId, userIds) => {
    set((state) => ({
      typingUsers: {
        ...state.typingUsers,
        [channelId]: userIds,
      },
    }))
  },

  setLoadingChannels: (loading) => {
    set({ isLoadingChannels: loading })
  },

  setLoadingMessages: (loading) => {
    set({ isLoadingMessages: loading })
  },

  clearChat: () => {
    set({
      streamToken: null,
      isStreamConnected: false,
      channels: [],
      activeChannelId: null,
      messages: {},
      typingUsers: {},
    })
  },
}))
