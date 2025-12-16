/**
 * Challenge model - MIDI battle royale challenges
 * Users compete by submitting MIDI entries, others vote on winners
 */

export type ChallengeStatus = 'upcoming' | 'active' | 'voting' | 'completed'

export interface ChallengeConstraint {
  type: 'bpm' | 'key' | 'genre' | 'daw' | 'duration'
  value: string | number
  minValue?: number
  maxValue?: number
}

export interface ChallengeSubmission {
  id: string
  userId: string
  username: string
  displayName: string
  userAvatarUrl?: string

  audioUrl: string
  midiUrl?: string

  voteCount: number
  hasUserVoted?: boolean

  submittedAt: Date
}

export interface Challenge {
  id: string
  title: string
  description: string
  creatorId: string
  creatorUsername: string

  status: ChallengeStatus
  constraints: ChallengeConstraint[]

  // Dates
  startDate: Date
  endDate: Date
  votingEndDate: Date

  // Submissions
  submissions: ChallengeSubmission[]
  submissionCount: number

  // Prize/rewards
  prize?: string
  prizeDescription?: string

  // Metadata
  participantCount: number
  viewCount: number
  createdAt: Date
  updatedAt: Date
}

export class ChallengeModel {
  static fromJson(json: any): Challenge {
    if (!json) throw new Error('Cannot create Challenge from null/undefined')

    return {
      id: json.id || '',
      title: json.title || '',
      description: json.description || '',
      creatorId: json.creator_id || '',
      creatorUsername: json.creator_username || '',

      status: (json.status || 'active') as ChallengeStatus,
      constraints: (json.constraints || []).map((c: any) => ({
        type: c.type,
        value: c.value,
        minValue: c.min_value,
        maxValue: c.max_value,
      })),

      startDate: new Date(json.start_date || Date.now()),
      endDate: new Date(json.end_date || Date.now() + 7 * 24 * 60 * 60 * 1000),
      votingEndDate: new Date(json.voting_end_date || Date.now() + 14 * 24 * 60 * 60 * 1000),

      submissions: (json.submissions || []).map((s: any) => ({
        id: s.id,
        userId: s.user_id,
        username: s.username,
        displayName: s.display_name,
        userAvatarUrl: s.user_avatar_url,
        audioUrl: s.audio_url,
        midiUrl: s.midi_url,
        voteCount: s.vote_count || 0,
        hasUserVoted: s.has_user_voted || false,
        submittedAt: new Date(s.submitted_at),
      })),
      submissionCount: json.submission_count || 0,

      prize: json.prize,
      prizeDescription: json.prize_description,

      participantCount: json.participant_count || 0,
      viewCount: json.view_count || 0,
      createdAt: new Date(json.created_at || Date.now()),
      updatedAt: new Date(json.updated_at || Date.now()),
    }
  }

  static isValid(challenge: Challenge): boolean {
    return challenge.id !== '' && challenge.title !== ''
  }

  static toJson(challenge: Challenge): any {
    return {
      id: challenge.id,
      title: challenge.title,
      description: challenge.description,
      creator_id: challenge.creatorId,
      creator_username: challenge.creatorUsername,
      status: challenge.status,
      constraints: challenge.constraints.map((c) => ({
        type: c.type,
        value: c.value,
        min_value: c.minValue,
        max_value: c.maxValue,
      })),
      start_date: challenge.startDate.toISOString(),
      end_date: challenge.endDate.toISOString(),
      voting_end_date: challenge.votingEndDate.toISOString(),
      submission_count: challenge.submissionCount,
      prize: challenge.prize,
      prize_description: challenge.prizeDescription,
      participant_count: challenge.participantCount,
      view_count: challenge.viewCount,
      created_at: challenge.createdAt.toISOString(),
      updated_at: challenge.updatedAt.toISOString(),
    }
  }

  static getStatusBadge(status: ChallengeStatus): { label: string; emoji: string } {
    switch (status) {
      case 'upcoming':
        return { label: 'Upcoming', emoji: '⏳' }
      case 'active':
        return { label: 'Active', emoji: '🔴' }
      case 'voting':
        return { label: 'Voting', emoji: '🗳️' }
      case 'completed':
        return { label: 'Completed', emoji: '✅' }
    }
  }
}
