import { useState, useEffect } from 'react'
import { UserProfileClient } from '@/api/UserProfileClient'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'

interface BlockedUser {
  id: string
  username: string
  displayName: string
  profilePictureUrl: string
}

/**
 * BlockedUsersList - Manage blocked users
 */
export function BlockedUsersList() {
  const [blockedUsers, setBlockedUsers] = useState<BlockedUser[]>([])
  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState('')

  useEffect(() => {
    loadBlockedUsers()
  }, [])

  const loadBlockedUsers = async () => {
    setIsLoading(true)
    setError('')

    const result = await UserProfileClient.getBlockedUsers()

    if (result.isOk()) {
      setBlockedUsers(result.getValue())
    } else {
      setError(result.getError())
    }

    setIsLoading(false)
  }

  const handleUnblock = async (userId: string) => {
    const result = await UserProfileClient.unblockUser(userId)

    if (result.isOk()) {
      setBlockedUsers(blockedUsers.filter(u => u.id !== userId))
    } else {
      setError(result.getError())
    }
  }

  if (isLoading) {
    return (
      <div className="flex justify-center py-8">
        <Spinner size="sm" />
      </div>
    )
  }

  if (error) {
    return (
      <div className="p-4 rounded-lg bg-red-500/10 border border-red-500/20 text-sm text-red-400">
        {error}
      </div>
    )
  }

  if (blockedUsers.length === 0) {
    return (
      <div className="p-4 rounded-lg bg-muted/50 border border-border/30 text-sm text-muted-foreground text-center">
        You haven't blocked anyone yet
      </div>
    )
  }

  return (
    <div className="space-y-3">
      <h3 className="font-semibold text-foreground mb-3">Blocked Users</h3>
      {blockedUsers.map(user => (
        <div key={user.id} className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
          <div className="flex items-center gap-3">
            <img
              src={user.profilePictureUrl}
              alt={user.displayName}
              className="w-10 h-10 rounded-full"
            />
            <div>
              <p className="font-semibold text-foreground text-sm">{user.displayName}</p>
              <p className="text-xs text-muted-foreground">@{user.username}</p>
            </div>
          </div>
          <Button
            onClick={() => handleUnblock(user.id)}
            variant="ghost"
            size="sm"
            className="text-coral-pink hover:text-rose-pink"
          >
            Unblock
          </Button>
        </div>
      ))}
    </div>
  )
}
