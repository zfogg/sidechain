import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { SocialClient } from '@/api/SocialClient'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'

interface FollowRequest {
  id: string
  username: string
  displayName: string
  profilePictureUrl?: string
}

/**
 * FollowRequestsList - Display and manage pending follow requests for private accounts
 *
 * Features:
 * - Lists all pending follow requests
 * - Accept/reject individual requests
 * - Loading and empty states
 * - Real-time updates after accepting/rejecting
 */
export function FollowRequestsList() {
  const [acceptingId, setAcceptingId] = useState<string | null>(null)
  const [rejectingId, setRejectingId] = useState<string | null>(null)

  const {
    data: requests = [],
    isLoading,
    error,
    refetch,
  } = useQuery({
    queryKey: ['followRequests'],
    queryFn: async () => {
      const result = await SocialClient.getPendingFollowRequests()
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
    staleTime: 1 * 60 * 1000, // 1 minute
    gcTime: 10 * 60 * 1000, // 10 minutes
  })

  const handleAccept = async (userId: string) => {
    setAcceptingId(userId)
    const result = await SocialClient.acceptFollowRequest(userId)
    setAcceptingId(null)

    if (result.isOk()) {
      refetch()
    }
  }

  const handleReject = async (userId: string) => {
    setRejectingId(userId)
    const result = await SocialClient.rejectFollowRequest(userId)
    setRejectingId(null)

    if (result.isOk()) {
      refetch()
    }
  }

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-8">
        <Spinner size="md" />
      </div>
    )
  }

  if (error) {
    return (
      <div className="text-center py-8 text-muted-foreground">
        Failed to load follow requests
      </div>
    )
  }

  if (!requests || requests.length === 0) {
    return (
      <div className="text-center py-8 text-muted-foreground">
        No pending follow requests
      </div>
    )
  }

  return (
    <div className="space-y-3">
      {requests.map((request) => (
        <FollowRequestCard
          key={request.id}
          request={request}
          onAccept={handleAccept}
          onReject={handleReject}
          isAccepting={acceptingId === request.id}
          isRejecting={rejectingId === request.id}
        />
      ))}
    </div>
  )
}

interface FollowRequestCardProps {
  request: FollowRequest
  onAccept: (userId: string) => void
  onReject: (userId: string) => void
  isAccepting: boolean
  isRejecting: boolean
}

/**
 * FollowRequestCard - Individual follow request card
 */
function FollowRequestCard({
  request,
  onAccept,
  onReject,
  isAccepting,
  isRejecting,
}: FollowRequestCardProps) {
  const isLoading = isAccepting || isRejecting

  return (
    <div className="flex items-center gap-3 bg-card border border-border rounded-lg p-3 hover:border-coral-pink/50 transition-colors">
      {/* Avatar */}
      <img
        src={
          request.profilePictureUrl ||
          `https://api.dicebear.com/7.x/avataaars/svg?seed=${request.id}`
        }
        alt={request.displayName}
        className="w-10 h-10 rounded-full flex-shrink-0"
      />

      {/* User Info */}
      <div className="flex-1 min-w-0">
        <p className="font-medium text-sm text-foreground truncate">{request.displayName}</p>
        <p className="text-xs text-muted-foreground truncate">@{request.username}</p>
      </div>

      {/* Actions */}
      <div className="flex gap-2 flex-shrink-0">
        <Button
          size="sm"
          onClick={() => onAccept(request.id)}
          disabled={isLoading}
          className="bg-coral-pink hover:bg-coral-pink/90 text-white"
        >
          {isAccepting ? <Spinner size="sm" /> : 'Accept'}
        </Button>
        <Button
          size="sm"
          variant="outline"
          onClick={() => onReject(request.id)}
          disabled={isLoading}
        >
          {isRejecting ? <Spinner size="sm" /> : 'Reject'}
        </Button>
      </div>
    </div>
  )
}
