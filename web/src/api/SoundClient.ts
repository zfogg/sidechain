import { apiClient } from './client'
import { Outcome } from './types'
import { Sound, SoundModel } from '../models/Sound'

interface SoundsResponse {
  sounds: any[]
  total?: number
}

interface SoundResponse {
  sound: any
}

interface PostsUsingSound {
  posts: any[]
  total: number
}

/**
 * SoundClient - API client for shareable sounds/loops
 * Similar to TikTok's sound system
 */
export class SoundClient {
  /**
   * Get a specific sound by ID
   */
  static async getSound(soundId: string): Promise<Outcome<Sound>> {
    const result = await apiClient.get<SoundResponse>(`/sounds/${soundId}`)

    if (result.isOk()) {
      try {
        const sound = SoundModel.fromJson(result.getValue().sound)
        return Outcome.ok(sound)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get trending sounds
   */
  static async getTrendingSounds(limit: number = 20, offset: number = 0): Promise<Outcome<Sound[]>> {
    const result = await apiClient.get<SoundsResponse>('/sounds/trending', { limit, offset })

    if (result.isOk()) {
      try {
        const sounds = (result.getValue().sounds || []).map(SoundModel.fromJson)
        return Outcome.ok(sounds)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Search sounds by title or description
   */
  static async searchSounds(
    query: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Sound[]>> {
    const result = await apiClient.get<SoundsResponse>('/sounds/search', {
      q: query,
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const sounds = (result.getValue().sounds || []).map(SoundModel.fromJson)
        return Outcome.ok(sounds)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get posts that use this sound
   */
  static async getPostsUsingSound(
    soundId: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<
    Outcome<
      Array<{
        id: string
        filename: string
        username: string
        displayName: string
        userAvatarUrl?: string
      }>
    >
  > {
    const result = await apiClient.get<PostsUsingSound>(`/sounds/${soundId}/posts`, {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const posts = (result.getValue().posts || []).map((p: any) => ({
          id: p.id,
          filename: p.filename,
          username: p.username,
          displayName: p.display_name,
          userAvatarUrl: p.user_avatar_url,
        }))
        return Outcome.ok(posts)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Like a sound
   */
  static async likeSound(soundId: string, shouldLike: boolean): Promise<Outcome<void>> {
    if (shouldLike) {
      return apiClient.post(`/sounds/${soundId}/like`, {})
    } else {
      return apiClient.post(`/sounds/${soundId}/unlike`, {})
    }
  }

  /**
   * Get sounds by a specific creator
   */
  static async getCreatorSounds(
    userId: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Sound[]>> {
    const result = await apiClient.get<SoundsResponse>(`/users/${userId}/sounds`, {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const sounds = (result.getValue().sounds || []).map(SoundModel.fromJson)
        return Outcome.ok(sounds)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }
}
