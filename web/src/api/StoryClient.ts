import { apiClient } from './client'
import { Outcome } from './types'
import { Ephemeral, EphemeralModel, StoryHighlight, StoryViewer } from '../models/Ephemeral'

/**
 * StoryClient - API client for Stories/Ephemerals (temporary media content)
 * Mirrors C++ StoryClient.cpp pattern
 */

interface CreateStoryPayload {
  mediaUrl: string
  mediaType: 'image' | 'video' | 'audio'
  duration: number
  caption?: string
  isAudioStory?: boolean
  visualizationType?: 'circular' | 'piano-roll' | 'waveform' | 'note-waterfall'
  visualizationData?: Record<string, unknown>
}

// API DTO Types
interface StoryDTO {
  id: string
  user_id: string
  username: string
  display_name: string
  profile_picture_url?: string
  user_avatar_url?: string
  actor?: string
  media_url: string
  media_type: 'image' | 'video' | 'audio'
  duration: number
  thumbnail_url?: string
  caption?: string
  is_audio_story: boolean
  visualization_type?: string
  visualization_data?: Record<string, unknown>
  view_count: number
  like_count: number
  viewer_ids: string[]
  created_at: string
  time?: string
  expires_at: string
  updated_at: string
}

interface ViewerDTO {
  id: string
  username: string
  display_name: string
  profile_picture_url: string
  viewed_at: string
}

interface HighlightDTO {
  id: string
  title: string
  cover_image_url?: string
  story_ids: string[]
  created_at: string
  updated_at: string
}

// API Response Interfaces
interface StoriesResponse {
  stories: StoryDTO[]
}

interface StoryResponse {
  story: StoryDTO
}

interface ViewersResponse {
  viewers: ViewerDTO[]
}

interface HighlightsResponse {
  highlights: HighlightDTO[]
}

interface HighlightResponse {
  highlight: HighlightDTO
}

interface UpdateHighlightPayload {
  title?: string
  story_ids?: string[]
}

export class StoryClient {
  /**
   * Get active stories from followed users
   */
  static async getActiveStories(): Promise<Outcome<Ephemeral[]>> {
    const result = await apiClient.get<StoriesResponse>('/stories/active')

    if (result.isOk()) {
      try {
        const stories = (result.getValue().stories || []).map(EphemeralModel.fromJson)
        return Outcome.ok(stories)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get stories from a specific user
   */
  static async getUserStories(userId: string): Promise<Outcome<Ephemeral[]>> {
    const result = await apiClient.get<StoriesResponse>(`/users/${userId}/stories`)

    if (result.isOk()) {
      try {
        const stories = (result.getValue().stories || []).map(EphemeralModel.fromJson)
        return Outcome.ok(stories)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Create a new story
   */
  static async createStory(payload: CreateStoryPayload): Promise<Outcome<Ephemeral>> {
    const data = {
      media_url: payload.mediaUrl,
      media_type: payload.mediaType,
      duration: payload.duration,
      caption: payload.caption,
      is_audio_story: payload.isAudioStory,
      visualization_type: payload.visualizationType,
      visualization_data: payload.visualizationData,
    }

    const result = await apiClient.post<StoryResponse>('/stories', data)

    if (result.isOk()) {
      try {
        const story = EphemeralModel.fromJson(result.getValue().story)
        return Outcome.ok(story)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Delete a story
   */
  static async deleteStory(storyId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/stories/${storyId}`)
  }

  /**
   * Mark story as viewed
   */
  static async viewStory(storyId: string): Promise<Outcome<void>> {
    return apiClient.post(`/stories/${storyId}/view`, {})
  }

  /**
   * Like a story
   */
  static async likeStory(storyId: string, shouldLike: boolean): Promise<Outcome<void>> {
    if (shouldLike) {
      return apiClient.post(`/stories/${storyId}/like`, {})
    } else {
      return apiClient.post(`/stories/${storyId}/unlike`, {})
    }
  }

  /**
   * Get story viewers
   */
  static async getStoryViewers(storyId: string): Promise<Outcome<StoryViewer[]>> {
    const result = await apiClient.get<ViewersResponse>(`/stories/${storyId}/viewers`)

    if (result.isOk()) {
      try {
        const viewers = (result.getValue().viewers || []).map((v: ViewerDTO) => ({
          id: v.id,
          username: v.username,
          displayName: v.display_name,
          profilePictureUrl: v.profile_picture_url,
          viewedAt: new Date(v.viewed_at),
        }))
        return Outcome.ok(viewers)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Create a story highlight (permanent collection)
   */
  static async createHighlight(title: string, storyIds: string[]): Promise<Outcome<StoryHighlight>> {
    const result = await apiClient.post<HighlightResponse>('/story-highlights', {
      title,
      story_ids: storyIds,
    })

    if (result.isOk()) {
      try {
        const h = result.getValue().highlight
        return Outcome.ok({
          id: h.id,
          title: h.title,
          coverImageUrl: h.cover_image_url,
          storyIds: h.story_ids || [],
          createdAt: new Date(h.created_at),
          updatedAt: new Date(h.updated_at),
        })
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get user's story highlights
   */
  static async getUserHighlights(userId: string): Promise<Outcome<StoryHighlight[]>> {
    const result = await apiClient.get<HighlightsResponse>(`/users/${userId}/story-highlights`)

    if (result.isOk()) {
      try {
        const highlights = (result.getValue().highlights || []).map((h: HighlightDTO) => ({
          id: h.id,
          title: h.title,
          coverImageUrl: h.cover_image_url,
          storyIds: h.story_ids || [],
          createdAt: new Date(h.created_at),
          updatedAt: new Date(h.updated_at),
        }))
        return Outcome.ok(highlights)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Update a story highlight
   */
  static async updateHighlight(
    highlightId: string,
    updates: { title?: string; storyIds?: string[] }
  ): Promise<Outcome<StoryHighlight>> {
    const data: UpdateHighlightPayload = {}
    if (updates.title) data.title = updates.title
    if (updates.storyIds) data.story_ids = updates.storyIds

    const result = await apiClient.put<HighlightResponse>(`/story-highlights/${highlightId}`, data)

    if (result.isOk()) {
      try {
        const h = result.getValue().highlight
        return Outcome.ok({
          id: h.id,
          title: h.title,
          coverImageUrl: h.cover_image_url,
          storyIds: h.story_ids || [],
          createdAt: new Date(h.created_at),
          updatedAt: new Date(h.updated_at),
        })
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Delete a story highlight
   */
  static async deleteHighlight(highlightId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/story-highlights/${highlightId}`)
  }

  /**
   * Get activity timeline from followed users
   */
  static async getFollowingActivityTimeline(
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Record<string, unknown>[]>> {
    const result = await apiClient.get<StoriesResponse>('/activity/following', {
      limit,
      offset,
    })

    if (result.isOk()) {
      return Outcome.ok(result.getValue().stories || [])
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get global activity timeline
   */
  static async getGlobalActivityTimeline(
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Record<string, unknown>[]>> {
    const result = await apiClient.get<StoriesResponse>('/activity/global', {
      limit,
      offset,
    })

    if (result.isOk()) {
      return Outcome.ok(result.getValue().stories || [])
    }

    return Outcome.error(result.getError())
  }
}
