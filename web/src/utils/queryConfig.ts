/**
 * React Query Configuration & Caching Strategies
 * Optimized for different data types and their update frequencies
 */

export const queryConfig = {
  // Frequently changing data (real-time)
  feed: {
    staleTime: 30000, // 30 seconds - refetch sooner for fresh feed
    gcTime: 300000, // 5 minutes
    retry: 2,
  },

  // Comment data (moderate changes)
  comments: {
    staleTime: 60000, // 1 minute
    gcTime: 600000, // 10 minutes
    retry: 2,
  },

  // User data (rarely changes)
  user: {
    staleTime: 300000, // 5 minutes
    gcTime: 1800000, // 30 minutes
    retry: 3,
  },

  // Profile data (rarely changes)
  profile: {
    staleTime: 600000, // 10 minutes
    gcTime: 3600000, // 1 hour
    retry: 3,
  },

  // Search results (one-time queries)
  search: {
    staleTime: 300000, // 5 minutes - fresh searches
    gcTime: 600000, // 10 minutes
    retry: 1,
  },

  // Recommendations (weekly updates)
  recommendations: {
    staleTime: 3600000, // 1 hour
    gcTime: 7200000, // 2 hours
    retry: 2,
  },

  // Static data (never changes)
  static: {
    staleTime: Infinity, // Never refetch
    gcTime: Infinity, // Keep forever
    retry: 1,
  },

  // Notifications (real-time)
  notifications: {
    staleTime: 10000, // 10 seconds
    gcTime: 300000, // 5 minutes
    retry: 2,
  },
}

/**
 * Query key factory for type-safe React Query keys
 * Prevents key collision and makes refetching easier
 */
export const queryKeys = {
  // Feed queries
  feed: {
    all: ['feed'] as const,
    timeline: () => [...queryKeys.feed.all, 'timeline'] as const,
    global: () => [...queryKeys.feed.all, 'global'] as const,
    trending: () => [...queryKeys.feed.all, 'trending'] as const,
    forYou: () => [...queryKeys.feed.all, 'forYou'] as const,
    detail: (id: string) => [...queryKeys.feed.all, 'detail', id] as const,
  },

  // Comment queries
  comments: {
    all: ['comments'] as const,
    forPost: (postId: string) => [...queryKeys.comments.all, 'post', postId] as const,
    detail: (commentId: string) => [...queryKeys.comments.all, 'detail', commentId] as const,
  },

  // User queries
  users: {
    all: ['users'] as const,
    me: () => [...queryKeys.users.all, 'me'] as const,
    profile: (username: string) => [...queryKeys.users.all, 'profile', username] as const,
    followers: (username: string) => [...queryKeys.users.all, 'followers', username] as const,
    following: (username: string) => [...queryKeys.users.all, 'following', username] as const,
  },

  // Search queries
  search: {
    all: ['search'] as const,
    posts: (query: string) => [...queryKeys.search.all, 'posts', query] as const,
    users: (query: string) => [...queryKeys.search.all, 'users', query] as const,
    global: (query: string) => [...queryKeys.search.all, 'global', query] as const,
  },

  // Discovery queries
  discovery: {
    all: ['discovery'] as const,
    trending: () => [...queryKeys.discovery.all, 'trending'] as const,
    recommendations: () => [...queryKeys.discovery.all, 'recommendations'] as const,
    trendingProducers: () => [...queryKeys.discovery.all, 'producers'] as const,
  },

  // Notification queries
  notifications: {
    all: ['notifications'] as const,
    list: () => [...queryKeys.notifications.all, 'list'] as const,
    count: () => [...queryKeys.notifications.all, 'count'] as const,
    unread: () => [...queryKeys.notifications.all, 'unread'] as const,
  },

  // Chat queries
  chat: {
    all: ['chat'] as const,
    channels: () => [...queryKeys.chat.all, 'channels'] as const,
    messages: (channelId: string) => [...queryKeys.chat.all, 'messages', channelId] as const,
  },
}
