/**
 * Sound model - Represents a shareable loop/sample that can be used in multiple posts
 * Similar to TikTok's sound system
 */

export interface Sound {
  id: string
  title: string
  description?: string
  creatorId: string
  creatorUsername: string
  creatorDisplayName: string
  creatorAvatarUrl?: string

  audioUrl: string
  waveformUrl?: string
  duration: number
  bpm?: number
  key?: string
  daw?: string
  genre?: string[]

  // Engagement
  useCount: number // how many posts use this sound
  likeCount: number
  isLiked?: boolean

  // Metadata
  isOfficial?: boolean // official Sidechain sound
  isTrending?: boolean
  createdAt: Date
  updatedAt: Date
}

export class SoundModel {
  static fromJson(json: any): Sound {
    if (!json) throw new Error('Cannot create Sound from null/undefined')

    return {
      id: json.id || '',
      title: json.title || '',
      description: json.description,
      creatorId: json.creator_id || json.user_id || '',
      creatorUsername: json.creator_username || json.username || '',
      creatorDisplayName: json.creator_display_name || json.display_name || '',
      creatorAvatarUrl: json.creator_avatar_url || json.profile_picture_url,

      audioUrl: json.audio_url || '',
      waveformUrl: json.waveform_url,
      duration: json.duration || 0,
      bpm: json.bpm,
      key: json.key,
      daw: json.daw,
      genre: json.genre || [],

      useCount: json.use_count || 0,
      likeCount: json.like_count || 0,
      isLiked: json.is_liked || false,

      isOfficial: json.is_official || false,
      isTrending: json.is_trending || false,
      createdAt: new Date(json.created_at || Date.now()),
      updatedAt: new Date(json.updated_at || json.created_at || Date.now()),
    }
  }

  static isValid(sound: Sound): boolean {
    return sound.id !== '' && sound.title !== '' && sound.audioUrl !== ''
  }

  static toJson(sound: Sound): any {
    return {
      id: sound.id,
      title: sound.title,
      description: sound.description,
      creator_id: sound.creatorId,
      creator_username: sound.creatorUsername,
      creator_display_name: sound.creatorDisplayName,
      creator_avatar_url: sound.creatorAvatarUrl,
      audio_url: sound.audioUrl,
      waveform_url: sound.waveformUrl,
      duration: sound.duration,
      bpm: sound.bpm,
      key: sound.key,
      daw: sound.daw,
      genre: sound.genre,
      use_count: sound.useCount,
      like_count: sound.likeCount,
      is_official: sound.isOfficial,
      created_at: sound.createdAt.toISOString(),
      updated_at: sound.updatedAt.toISOString(),
    }
  }
}
