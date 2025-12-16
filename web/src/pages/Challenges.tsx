import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { ChallengesClient } from '@/api/ChallengesClient'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import { Challenge } from '@/models/Challenge'
import { useNavigate } from 'react-router-dom'

/**
 * Challenges - Main page for MIDI battle royale challenges
 *
 * Features:
 * - View active challenges
 * - View past/completed challenges
 * - View upcoming challenges
 * - Filter by status
 * - Participate in challenges
 */
export function Challenges() {
  const navigate = useNavigate()
  const [activeTab, setActiveTab] = useState('active')

  // Fetch challenges by tab
  const {
    data: challenges = [],
    isLoading,
  } = useQuery({
    queryKey: ['challenges', activeTab],
    queryFn: async () => {
      let result
      if (activeTab === 'active') {
        result = await ChallengesClient.getActiveChallenges(50, 0)
      } else if (activeTab === 'upcoming') {
        result = await ChallengesClient.getUpcomingChallenges(50, 0)
      } else {
        result = await ChallengesClient.getPastChallenges(50, 0)
      }

      if (result.isError()) {
        throw new Error(result.getError())
      }

      return result.getValue()
    },
  })

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Header */}
      <div className="bg-card/50 border-b border-border">
        <div className="max-w-6xl mx-auto px-4 py-8">
          <div className="flex items-center justify-between">
            <div>
              <h1 className="text-4xl font-bold text-foreground flex items-center gap-3">
                <span>⚔️</span> MIDI Challenges
              </h1>
              <p className="text-muted-foreground mt-2">Create, compete, and win in musical battles</p>
            </div>
            <Button
              onClick={() => navigate('/challenges/create')}
              className="bg-coral-pink hover:bg-coral-pink/90 text-white"
              size="lg"
            >
              + Create Challenge
            </Button>
          </div>
        </div>
      </div>

      {/* Content */}
      <div className="max-w-6xl mx-auto px-4 py-8">
        <Tabs value={activeTab} onValueChange={setActiveTab} className="w-full">
          <TabsList className="grid w-full grid-cols-3 mb-8">
            <TabsTrigger value="active">🔴 Active</TabsTrigger>
            <TabsTrigger value="upcoming">⏳ Upcoming</TabsTrigger>
            <TabsTrigger value="past">✅ Past</TabsTrigger>
          </TabsList>

          {/* Active Challenges */}
          <TabsContent value="active">
            {isLoading ? (
              <div className="flex items-center justify-center py-12">
                <Spinner size="lg" />
              </div>
            ) : challenges.length > 0 ? (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                {challenges.map((challenge) => (
                  <ChallengeCard
                    key={challenge.id}
                    challenge={challenge}
                    onViewDetails={() => navigate(`/challenges/${challenge.id}`)}
                  />
                ))}
              </div>
            ) : (
              <div className="text-center py-12 text-muted-foreground">
                <p className="text-lg mb-2">No active challenges</p>
                <p className="text-sm">Check back soon or create one!</p>
              </div>
            )}
          </TabsContent>

          {/* Upcoming Challenges */}
          <TabsContent value="upcoming">
            {isLoading ? (
              <div className="flex items-center justify-center py-12">
                <Spinner size="lg" />
              </div>
            ) : challenges.length > 0 ? (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                {challenges.map((challenge) => (
                  <ChallengeCard
                    key={challenge.id}
                    challenge={challenge}
                    onViewDetails={() => navigate(`/challenges/${challenge.id}`)}
                  />
                ))}
              </div>
            ) : (
              <div className="text-center py-12 text-muted-foreground">
                <p className="text-lg mb-2">No upcoming challenges</p>
              </div>
            )}
          </TabsContent>

          {/* Past Challenges */}
          <TabsContent value="past">
            {isLoading ? (
              <div className="flex items-center justify-center py-12">
                <Spinner size="lg" />
              </div>
            ) : challenges.length > 0 ? (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                {challenges.map((challenge) => (
                  <ChallengeCard
                    key={challenge.id}
                    challenge={challenge}
                    onViewDetails={() => navigate(`/challenges/${challenge.id}`)}
                  />
                ))}
              </div>
            ) : (
              <div className="text-center py-12 text-muted-foreground">
                <p className="text-lg mb-2">No past challenges</p>
              </div>
            )}
          </TabsContent>
        </Tabs>
      </div>
    </div>
  )
}

interface ChallengeCardProps {
  challenge: Challenge
  onViewDetails: () => void
}

/**
 * ChallengeCard - Individual challenge preview card
 */
function ChallengeCard({ challenge, onViewDetails }: ChallengeCardProps) {
  const statusBadge = require('@/models/Challenge').ChallengeModel.getStatusBadge(challenge.status)
  const daysLeft = Math.ceil((challenge.endDate.getTime() - Date.now()) / (1000 * 60 * 60 * 24))

  return (
    <div
      onClick={onViewDetails}
      className="bg-card border border-border rounded-lg p-6 hover:border-coral-pink/50 hover:shadow-lg transition-all cursor-pointer group"
    >
      {/* Header with status */}
      <div className="flex items-start justify-between mb-4">
        <div>
          <h3 className="text-xl font-bold text-foreground group-hover:text-coral-pink transition-colors">
            {challenge.title}
          </h3>
          <p className="text-sm text-muted-foreground mt-1">
            by @{challenge.creatorUsername}
          </p>
        </div>
        <span className="px-3 py-1 bg-bg-secondary rounded-full text-sm font-semibold text-foreground">
          {statusBadge.emoji} {statusBadge.label}
        </span>
      </div>

      {/* Description */}
      <p className="text-foreground text-sm mb-4 line-clamp-2">{challenge.description}</p>

      {/* Constraints */}
      {challenge.constraints.length > 0 && (
        <div className="mb-4 pb-4 border-b border-border">
          <p className="text-xs font-semibold text-muted-foreground mb-2">Constraints:</p>
          <div className="flex flex-wrap gap-2">
            {challenge.constraints.map((constraint, idx) => (
              <span key={idx} className="px-2 py-1 bg-bg-secondary rounded text-xs text-foreground">
                {constraint.type === 'bpm' && `${constraint.minValue}-${constraint.maxValue} BPM`}
                {constraint.type === 'key' && `Key: ${constraint.value}`}
                {constraint.type === 'genre' && `${constraint.value}`}
                {constraint.type === 'daw' && `${constraint.value}`}
                {constraint.type === 'duration' && `${constraint.value}s`}
              </span>
            ))}
          </div>
        </div>
      )}

      {/* Stats */}
      <div className="grid grid-cols-3 gap-4 mb-4 text-center">
        <div>
          <p className="text-lg font-bold text-foreground">{challenge.submissionCount}</p>
          <p className="text-xs text-muted-foreground">Submissions</p>
        </div>
        <div>
          <p className="text-lg font-bold text-foreground">{challenge.participantCount}</p>
          <p className="text-xs text-muted-foreground">Participants</p>
        </div>
        <div>
          <p className="text-lg font-bold text-coral-pink">{daysLeft}d</p>
          <p className="text-xs text-muted-foreground">Left</p>
        </div>
      </div>

      {/* Prize */}
      {challenge.prize && (
        <div className="p-3 bg-gradient-to-r from-coral-pink/10 to-rose-pink/10 border border-coral-pink/20 rounded-lg mb-4">
          <p className="text-sm font-semibold text-coral-pink">🏆 {challenge.prize}</p>
          {challenge.prizeDescription && (
            <p className="text-xs text-muted-foreground mt-1">{challenge.prizeDescription}</p>
          )}
        </div>
      )}

      {/* View details button */}
      <Button
        onClick={(e) => {
          e.stopPropagation()
          onViewDetails()
        }}
        className="w-full bg-coral-pink hover:bg-coral-pink/90 text-white"
      >
        View Challenge →
      </Button>
    </div>
  )
}
