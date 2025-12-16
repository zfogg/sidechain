import { apiClient } from './client'
import { Outcome } from './types'
import { Challenge, ChallengeModel } from '../models/Challenge'

interface ChallengesResponse {
  challenges: any[]
  total?: number
}

interface ChallengeResponse {
  challenge: any
}

interface SubmitEntryPayload {
  audioUrl: string
  midiUrl?: string
}

/**
 * ChallengesClient - API client for MIDI challenges (battle royale)
 * Mirrors C++ ChallengesClient.cpp pattern
 */
export class ChallengesClient {
  /**
   * Get active challenges
   */
  static async getActiveChallenges(limit: number = 20, offset: number = 0): Promise<Outcome<Challenge[]>> {
    const result = await apiClient.get<ChallengesResponse>('/challenges/active', { limit, offset })

    if (result.isOk()) {
      try {
        const challenges = (result.getValue().challenges || []).map(ChallengeModel.fromJson)
        return Outcome.ok(challenges)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get past/completed challenges
   */
  static async getPastChallenges(limit: number = 20, offset: number = 0): Promise<Outcome<Challenge[]>> {
    const result = await apiClient.get<ChallengesResponse>('/challenges/past', { limit, offset })

    if (result.isOk()) {
      try {
        const challenges = (result.getValue().challenges || []).map(ChallengeModel.fromJson)
        return Outcome.ok(challenges)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get upcoming challenges
   */
  static async getUpcomingChallenges(limit: number = 20, offset: number = 0): Promise<Outcome<Challenge[]>> {
    const result = await apiClient.get<ChallengesResponse>('/challenges/upcoming', { limit, offset })

    if (result.isOk()) {
      try {
        const challenges = (result.getValue().challenges || []).map(ChallengeModel.fromJson)
        return Outcome.ok(challenges)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get a specific challenge
   */
  static async getChallenge(challengeId: string): Promise<Outcome<Challenge>> {
    const result = await apiClient.get<ChallengeResponse>(`/challenges/${challengeId}`)

    if (result.isOk()) {
      try {
        const challenge = ChallengeModel.fromJson(result.getValue().challenge)
        return Outcome.ok(challenge)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Submit an entry to a challenge
   */
  static async submitEntry(
    challengeId: string,
    payload: SubmitEntryPayload
  ): Promise<Outcome<void>> {
    const data = {
      audio_url: payload.audioUrl,
      midi_url: payload.midiUrl,
    }

    return apiClient.post(`/challenges/${challengeId}/submit`, data)
  }

  /**
   * Vote for a submission
   */
  static async voteForSubmission(
    challengeId: string,
    submissionId: string
  ): Promise<Outcome<void>> {
    return apiClient.post(`/challenges/${challengeId}/submissions/${submissionId}/vote`, {})
  }

  /**
   * Remove vote from a submission
   */
  static async removeVote(
    challengeId: string,
    submissionId: string
  ): Promise<Outcome<void>> {
    return apiClient.post(`/challenges/${challengeId}/submissions/${submissionId}/unvote`, {})
  }

  /**
   * Get winner of a completed challenge
   */
  static async getChallengeWinner(
    challengeId: string
  ): Promise<
    Outcome<{
      submissionId: string
      userId: string
      username: string
      displayName: string
      audioUrl: string
      voteCount: number
    }>
  > {
    const result = await apiClient.get<{ winner: any }>(`/challenges/${challengeId}/winner`)

    if (result.isOk()) {
      try {
        const w = result.getValue().winner
        return Outcome.ok({
          submissionId: w.submission_id,
          userId: w.user_id,
          username: w.username,
          displayName: w.display_name,
          audioUrl: w.audio_url,
          voteCount: w.vote_count,
        })
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }
}
