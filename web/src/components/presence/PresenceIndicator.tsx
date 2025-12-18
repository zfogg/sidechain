import { usePresence, getPresenceStatus, getPresenceIcon } from '@/hooks/usePresence'

interface PresenceIndicatorProps {
  userId: string
  showLabel?: boolean
  showDaw?: boolean
  size?: 'sm' | 'md' | 'lg'
}

/**
 * PresenceIndicator - Shows online/offline status with DAW information
 *
 * Features:
 * - Displays status icon (green dot, DAW emoji, etc.)
 * - Shows status text ("Online", "In Logic Pro", etc.)
 * - Auto-updates every minute
 * - Graceful fallback if offline
 *
 * Usage:
 * ```tsx
 * <PresenceIndicator userId="user-123" showLabel showDaw size="md" />
 * // Shows: 🎚️ In Ableton Live
 * ```
 */
export function PresenceIndicator({
  userId,
  showLabel = false,
  size = 'md',
}: PresenceIndicatorProps) {
  const { data: presence, isLoading } = usePresence(userId)
  const userPresence = presence?.[userId]

  if (isLoading) {
    return (
      <div className="flex items-center gap-2">
        <div className="w-2 h-2 rounded-full bg-gray-300 animate-pulse" />
        {showLabel && <span className="text-xs text-gray-500">Loading...</span>}
      </div>
    )
  }

  const icon = getPresenceIcon(userPresence)
  const status = getPresenceStatus(userPresence)

  const dotSizeClass = {
    sm: 'w-1.5 h-1.5',
    md: 'w-2 h-2',
    lg: 'w-3 h-3',
  }[size]

  const textSizeClass = {
    sm: 'text-xs',
    md: 'text-sm',
    lg: 'text-base',
  }[size]

  // If offline and not showing label, just show the gray dot
  if (userPresence?.status === 'offline' && !showLabel) {
    return (
      <div className={`${dotSizeClass} rounded-full bg-gray-300`} title="Offline" />
    )
  }

  return (
    <div className="flex items-center gap-1">
      <span className="text-lg">{icon}</span>
      {showLabel && <span className={`${textSizeClass} text-foreground`}>{status}</span>}
    </div>
  )
}

interface PresenceBadgeProps {
  userId: string
  variant?: 'default' | 'subtle'
}

/**
 * PresenceBadge - Inline badge showing presence with styling
 *
 * Usage:
 * ```tsx
 * <PresenceBadge userId="user-123" variant="default" />
 * ```
 */
export function PresenceBadge({ userId, variant = 'default' }: PresenceBadgeProps) {
  const { data: presence } = usePresence(userId)
  const userPresence = presence?.[userId]

  if (!userPresence || userPresence.status === 'offline') {
    return null
  }

  const icon = getPresenceIcon(userPresence)
  const status = getPresenceStatus(userPresence)

  const bgClass = variant === 'default' ? 'bg-blue-100 text-blue-900' : 'bg-gray-100 text-gray-700'

  return (
    <span className={`inline-flex items-center gap-1 px-2.5 py-0.5 rounded-full text-xs font-medium ${bgClass}`}>
      <span>{icon}</span>
      {status}
    </span>
  )
}

interface PresenceListProps {
  userIds: string[]
}

/**
 * PresenceList - Shows presence for multiple users
 *
 * Usage:
 * ```tsx
 * <PresenceList userIds={followingIds} />
 * // Shows all online users with their DAWs
 * ```
 */
export function PresenceList({ userIds }: PresenceListProps) {
  const { data: presence } = usePresence(userIds)

  const onlineUsers = userIds
    .filter((id) => presence?.[id]?.status !== 'offline')
    .map((id) => ({
      id,
      status: getPresenceStatus(presence?.[id]),
      icon: getPresenceIcon(presence?.[id]),
    }))

  if (onlineUsers.length === 0) {
    return <div className="text-sm text-muted-foreground">No one online</div>
  }

  return (
    <div className="space-y-2">
      {onlineUsers.map((user) => (
        <div key={user.id} className="flex items-center gap-2 text-sm">
          <span className="text-lg">{user.icon}</span>
          <span className="text-foreground">{user.status}</span>
        </div>
      ))}
    </div>
  )
}
