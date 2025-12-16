/**
 * Ephemeral model - Visual stories (like Instagram/TikTok stories)
 * Represents temporary media content (expires after 24 hours)
 */

export interface StoryViewer {
  id: string
  username: string
  displayName: string
  profilePictureUrl?: string
  viewedAt: Date
}

export interface StoryHighlight {
  id: string
  title: string
  coverImageUrl?: string
  storyIds: string[]
  createdAt: Date
  updatedAt: Date
}

export interface Ephemeral {
  id: string
  userId: string
  username: string
  displayName: string
  profilePictureUrl?: string

  // Media content
  mediaUrl: string
  mediaType: 'image' | 'video' | 'audio'
  duration: number // seconds
  thumbnail?: string

  // Story metadata
  caption?: string
  isAudioStory?: boolean // true for audio/music stories
  visualizationType?: 'circular' | 'piano-roll' | 'waveform' | 'note-waterfall'
  visualizationData?: any // data for visualization

  // Engagement
  viewCount: number
  likeCount: number
  viewerIds: string[]

  // Timestamps
  createdAt: Date
  expiresAt: Date
  updatedAt: Date
}

export class EphemeralModel {
  static fromJson(json: any): Ephemeral {
    if (!json) throw new Error('Cannot create Ephemeral from null/undefined')

    return {
      id: json.id || '',
      userId: json.user_id || json.actor?.split(':')[1] || '',
      username: json.username || '',
      displayName: json.display_name || json.username || '',
      profilePictureUrl: json.profile_picture_url || json.user_avatar_url || '',

      mediaUrl: json.media_url || '',
      mediaType: (json.media_type || 'image') as 'image' | 'video' | 'audio',
      duration: json.duration || 0,
      thumbnail: json.thumbnail_url,

      caption: json.caption,
      isAudioStory: json.is_audio_story || false,
      visualizationType: (json.visualization_type || 'circular') as any,
      visualizationData: json.visualization_data,

      viewCount: json.view_count || 0,
      likeCount: json.like_count || 0,
      viewerIds: json.viewer_ids || [],

      createdAt: new Date(json.created_at || json.time || Date.now()),
      expiresAt: new Date(json.expires_at || Date.now() + 24 * 60 * 60 * 1000),
      updatedAt: new Date(json.updated_at || json.created_at || json.time || Date.now()),
    }
  }

  static isValid(ephemeral: Ephemeral): boolean {
    return ephemeral.id !== '' && ephemeral.userId !== '' && ephemeral.mediaUrl !== ''
  }

  static isExpired(ephemeral: Ephemeral): boolean {
    return new Date() > ephemeral.expiresAt
  }

  static toJson(ephemeral: Ephemeral): any {
    return {
      id: ephemeral.id,
      user_id: ephemeral.userId,
      username: ephemeral.username,
      display_name: ephemeral.displayName,
      profile_picture_url: ephemeral.profilePictureUrl,
      media_url: ephemeral.mediaUrl,
      media_type: ephemeral.mediaType,
      duration: ephemeral.duration,
      thumbnail_url: ephemeral.thumbnail,
      caption: ephemeral.caption,
      is_audio_story: ephemeral.isAudioStory,
      visualization_type: ephemeral.visualizationType,
      visualization_data: ephemeral.visualizationData,
      view_count: ephemeral.viewCount,
      like_count: ephemeral.likeCount,
      viewer_ids: ephemeral.viewerIds,
      created_at: ephemeral.createdAt.toISOString(),
      expires_at: ephemeral.expiresAt.toISOString(),
      updated_at: ephemeral.updatedAt.toISOString(),
    }
  }
}
