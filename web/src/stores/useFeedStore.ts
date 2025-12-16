import { create } from 'zustand';
import { FeedPost } from '../models/FeedPost';
import { FeedClient, FeedType } from '../api/FeedClient';

/**
 * Single feed state
 */
interface SingleFeedState {
  posts: FeedPost[];
  isLoading: boolean;
  hasMore: boolean;
  offset: number;
  error: string;
  lastUpdated: number;
}

/**
 * Feed store state
 * Mirrors C++ FeedStore pattern with optimistic updates
 */
interface FeedStoreState {
  feeds: Record<FeedType, SingleFeedState>;
  currentFeedType: FeedType;
}

interface FeedStoreActions {
  loadFeed: (feedType: FeedType, forceRefresh?: boolean) => Promise<void>;
  loadMore: () => Promise<void>;
  switchFeedType: (feedType: FeedType) => void;
  toggleLike: (postId: string) => Promise<void>;
  toggleSave: (postId: string) => Promise<void>;
  updatePost: (postId: string, updates: Partial<FeedPost>) => void;
  incrementPlayCount: (postId: string) => void;
  addPostToFeed: (post: FeedPost) => void;
}

const emptyFeedState: SingleFeedState = {
  posts: [],
  isLoading: false,
  hasMore: true,
  offset: 0,
  error: '',
  lastUpdated: 0,
};

export const useFeedStore = create<FeedStoreState & FeedStoreActions>((set, get) => ({
  feeds: {},
  currentFeedType: 'timeline',

  loadFeed: async (feedType, forceRefresh = false) => {
    const existingFeed = get().feeds[feedType] || emptyFeedState;

    // Skip if already loaded and not forcing refresh
    if (existingFeed.posts.length > 0 && !forceRefresh) {
      set({ currentFeedType: feedType });
      return;
    }

    // Set loading state
    set((state) => ({
      feeds: {
        ...state.feeds,
        [feedType]: {
          ...existingFeed,
          isLoading: true,
          error: '',
        },
      },
      currentFeedType: feedType,
    }));

    // Fetch from API
    const result = await FeedClient.getFeed(feedType, 20, 0);

    if (result.isOk()) {
      const posts = result.getValue();
      set((state) => ({
        feeds: {
          ...state.feeds,
          [feedType]: {
            posts,
            isLoading: false,
            hasMore: posts.length >= 20,
            offset: posts.length,
            error: '',
            lastUpdated: Date.now(),
          },
        },
      }));
    } else {
      set((state) => ({
        feeds: {
          ...state.feeds,
          [feedType]: {
            ...existingFeed,
            isLoading: false,
            error: result.getError(),
          },
        },
      }));
    }
  },

  loadMore: async () => {
    const { currentFeedType, feeds } = get();
    const currentFeed = feeds[currentFeedType] || emptyFeedState;

    if (currentFeed.isLoading || !currentFeed.hasMore) return;

    set((state) => ({
      feeds: {
        ...state.feeds,
        [currentFeedType]: {
          ...currentFeed,
          isLoading: true,
        },
      },
    }));

    const result = await FeedClient.getFeed(
      currentFeedType,
      20,
      currentFeed.offset
    );

    if (result.isOk()) {
      const newPosts = result.getValue();
      set((state) => ({
        feeds: {
          ...state.feeds,
          [currentFeedType]: {
            ...currentFeed,
            posts: [...currentFeed.posts, ...newPosts],
            isLoading: false,
            hasMore: newPosts.length >= 20,
            offset: currentFeed.offset + newPosts.length,
          },
        },
      }));
    } else {
      set((state) => ({
        feeds: {
          ...state.feeds,
          [currentFeedType]: {
            ...currentFeed,
            isLoading: false,
            error: result.getError(),
          },
        },
      }));
    }
  },

  switchFeedType: (feedType) => {
    set({ currentFeedType: feedType });

    // Load feed if not cached
    const feed = get().feeds[feedType];
    if (!feed || feed.posts.length === 0) {
      get().loadFeed(feedType);
    }
  },

  toggleLike: async (postId) => {
    const { currentFeedType, feeds } = get();
    const currentFeed = feeds[currentFeedType] || emptyFeedState;

    const postIndex = currentFeed.posts.findIndex((p) => p.id === postId);
    if (postIndex === -1) return;

    const post = currentFeed.posts[postIndex];
    const shouldLike = !post.isLiked;

    // OPTIMISTIC UPDATE (like C++ pattern)
    const prevState = get().feeds;
    set((state) => ({
      feeds: {
        ...state.feeds,
        [currentFeedType]: {
          ...currentFeed,
          posts: currentFeed.posts.map((p) =>
            p.id === postId
              ? {
                  ...p,
                  isLiked: shouldLike,
                  likeCount: p.likeCount + (shouldLike ? 1 : -1),
                }
              : p
          ),
        },
      },
    }));

    // Sync with server
    const result = await FeedClient.toggleLike(postId, shouldLike);

    if (result.isError()) {
      // ROLLBACK on error (like C++ pattern)
      set({ feeds: prevState });
    }
  },

  toggleSave: async (postId) => {
    const { currentFeedType, feeds } = get();
    const currentFeed = feeds[currentFeedType] || emptyFeedState;

    const postIndex = currentFeed.posts.findIndex((p) => p.id === postId);
    if (postIndex === -1) return;

    const post = currentFeed.posts[postIndex];
    const shouldSave = !post.isSaved;

    // Optimistic update
    const prevState = get().feeds;
    set((state) => ({
      feeds: {
        ...state.feeds,
        [currentFeedType]: {
          ...currentFeed,
          posts: currentFeed.posts.map((p) =>
            p.id === postId
              ? {
                  ...p,
                  isSaved: shouldSave,
                  saveCount: p.saveCount + (shouldSave ? 1 : -1),
                }
              : p
          ),
        },
      },
    }));

    const result = await FeedClient.toggleSave(postId, shouldSave);

    if (result.isError()) {
      set({ feeds: prevState });
    }
  },

  updatePost: (postId, updates) => {
    set((state) => ({
      feeds: Object.fromEntries(
        Object.entries(state.feeds).map(([type, feed]) => [
          type,
          {
            ...feed,
            posts: feed.posts.map((post) =>
              post.id === postId ? { ...post, ...updates } : post
            ),
          },
        ])
      ),
    }));
  },

  incrementPlayCount: (postId) => {
    const { currentFeedType, feeds } = get();
    const post = feeds[currentFeedType]?.posts.find((p) => p.id === postId);

    if (post) {
      get().updatePost(postId, {
        playCount: post.playCount + 1,
      });

      // Track on server
      FeedClient.trackPlay(postId);
    }
  },

  addPostToFeed: (post) => {
    const { currentFeedType, feeds } = get();
    const currentFeed = feeds[currentFeedType] || emptyFeedState;

    set((state) => ({
      feeds: {
        ...state.feeds,
        [currentFeedType]: {
          ...currentFeed,
          posts: [post, ...currentFeed.posts],
        },
      },
    }));
  },
}));
