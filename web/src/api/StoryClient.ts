/**
 * StoryClient - API client for user activity stories/timeline
 * Handles fetching user activity history and following activity
 */

import { apiClient } from './client'
import { Outcome } from './types'
import { StoryModel, type Story } from '../models/Story'

export class StoryClient {
  /**
   * Get user's activity timeline (their own actions)
   */
  static async getUserActivityTimeline(
    userId: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Story[]>> {
    const result = await apiClient.get<{ activities: any[] }>(`/users/${userId}/activities`, {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const stories = result.getValue().activities.map(StoryModel.fromJson)
        return Outcome.ok(stories)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get activity timeline of users being followed (activity feed)
   */
  static async getFollowingActivityTimeline(
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Story[]>> {
    const result = await apiClient.get<{ activities: any[] }>('/activities/following', {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const stories = result.getValue().activities.map(StoryModel.fromJson)
        return Outcome.ok(stories)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get global activity timeline (all users)
   */
  static async getGlobalActivityTimeline(
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Story[]>> {
    const result = await apiClient.get<{ activities: any[] }>('/activities/global', {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const stories = result.getValue().activities.map(StoryModel.fromJson)
        return Outcome.ok(stories)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get a single story/activity
   */
  static async getStory(storyId: string): Promise<Outcome<Story>> {
    const result = await apiClient.get<{ activity: any }>(`/activities/${storyId}`)

    if (result.isOk()) {
      try {
        const story = StoryModel.fromJson(result.getValue().activity)
        return Outcome.ok(story)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }
}
