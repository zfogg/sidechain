/**
 * FeedPost model
 * Represents an audio post/loop in the feed
 * Mirrors C++ FeedPost.h structure
 */

// API DTO Type
export interface FeedPostJSON {
  id: string
  foreign_id?: string
  actor?: string
  verb?: string
  object?: string
  user_id?: string
  username?: string
  display_name?: string
  user_avatar_url?: string
  profile_picture_url?: string
  audio_url: string
  waveform_url?: string
  filename: string
  duration_seconds?: number
  duration?: number
  bpm?: number
  key?: string
  daw?: string
  genre?: string[]
  reaction_counts?: Record<string, number>
  like_count?: number
  play_count?: number
  playback_count?: number
  comment_count?: number
  comments_count?: number
  save_count?: number
  saved_count?: number
  repost_count?: number
  reposts_count?: number
  own_reactions?: Record<string, string[]>
  is_liked?: boolean
  is_saved?: boolean
  saved?: boolean
  is_reposted?: boolean
  reposted?: boolean
  is_following?: boolean
  is_own_post?: boolean
  own_post?: boolean
  time?: string
  created_at?: string
  updated_at?: string
}

export interface FeedPost {
  // Identifiers
  id: string;
  foreignId: string;
  actor: string;
  verb: string;
  object: string;

  // User info
  userId: string;
  username: string;
  displayName: string;
  userAvatarUrl: string;

  // Audio metadata
  audioUrl: string;
  waveformUrl: string;
  filename: string;
  durationSeconds: number;
  bpm: number;
  key: string;
  daw: string;
  genre: string[];

  // Social metrics
  likeCount: number;
  playCount: number;
  commentCount: number;
  saveCount: number;
  repostCount: number;

  // User engagement status
  isLiked: boolean;
  isSaved: boolean;
  isReposted: boolean;
  isFollowing: boolean;
  isOwnPost: boolean;

  // Emoji reactions
  reactionCounts: Record<string, number>;
  userReaction: string;

  // Timestamps
  createdAt: Date;
  updatedAt: Date;
}

/**
 * FeedPostModel - Static factory and utility methods
 * Mirrors C++ FeedPost::fromJson, toJson, isValid patterns
 */

export class FeedPostModel {
  /**
   * Create FeedPost from API response JSON
   */
  static fromJson(json: FeedPostJSON): FeedPost {
    if (!json) {
      throw new Error('Cannot create FeedPost from null/undefined');
    }

    return {
      id: json.id || '',
      foreignId: json.foreign_id || '',
      actor: json.actor || '',
      verb: json.verb || 'posted',
      object: json.object || '',

      userId: json.user_id || json.actor?.split(':')[1] || '',
      username: json.username || '',
      displayName: json.display_name || json.username || '',
      userAvatarUrl: json.user_avatar_url || json.profile_picture_url || '',

      audioUrl: json.audio_url || '',
      waveformUrl: json.waveform_url || '',
      filename: json.filename || 'Untitled Loop',
      durationSeconds: parseFloat(String(json.duration_seconds || json.duration || '0')),
      bpm: parseInt(String(json.bpm || '0'), 10),
      key: json.key || '',
      daw: json.daw || '',
      genre: Array.isArray(json.genre) ? json.genre : [],

      likeCount: parseInt(String(json.reaction_counts?.like || json.like_count || '0'), 10),
      playCount: parseInt(String(json.play_count || json.playback_count || '0'), 10),
      commentCount: parseInt(String(json.comment_count || json.comments_count || '0'), 10),
      saveCount: parseInt(String(json.save_count || json.saved_count || '0'), 10),
      repostCount: parseInt(String(json.repost_count || json.reposts_count || '0'), 10),

      isLiked: (json.own_reactions?.like?.length ?? 0) > 0 || json.is_liked || false,
      isSaved: json.is_saved || json.saved || false,
      isReposted: json.is_reposted || json.reposted || false,
      isFollowing: json.is_following || false,
      isOwnPost: json.is_own_post || json.own_post || false,

      reactionCounts: json.reaction_counts || {},
      userReaction: json.own_reactions ? Object.keys(json.own_reactions)[0] : '',

      createdAt: new Date(json.time || json.created_at || Date.now()),
      updatedAt: new Date(json.updated_at || json.time || Date.now()),
    };
  }

  /**
   * Validate FeedPost has required fields
   */
  static isValid(post: FeedPost): boolean {
    return (
      post.id !== '' &&
      post.userId !== '' &&
      post.audioUrl !== ''
    );
  }

  /**
   * Convert FeedPost to JSON for API requests
   */
  static toJson(post: FeedPost): Partial<FeedPostJSON> {
    return {
      id: post.id,
      foreign_id: post.foreignId,
      actor: post.actor,
      verb: post.verb,
      object: post.object,
      user_id: post.userId,
      username: post.username,
      display_name: post.displayName,
      audio_url: post.audioUrl,
      waveform_url: post.waveformUrl,
      filename: post.filename,
      duration_seconds: post.durationSeconds,
      bpm: post.bpm,
      key: post.key,
      daw: post.daw,
      genre: post.genre,
      created_at: post.createdAt.toISOString(),
      updated_at: post.updatedAt.toISOString(),
    };
  }

  /**
   * Create shallow copy with updates
   */
  static update(post: FeedPost, updates: Partial<FeedPost>): FeedPost {
    return {
      ...post,
      ...updates,
    };
  }
}
