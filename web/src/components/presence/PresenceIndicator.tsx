import { usePresence, getPresenceStatus, getPresenceIcon } from '@/hooks/usePresence'

interface PresenceIndicatorProps {
  userId: string
  showLabel?: boolean
  size?: 'sm' | 'md' | 'lg'
}

/**
 * PresenceIndicator - Shows online/offline status with custom status from GetStream.io
 *
 * Features:
 * - Displays status icon (green dot for online, gray for offline, custom for status)
 * - Shows status text ("Online", custom status, or "Last active X ago")
 * - Real-time updates via GetStream.io events
 * - Respects invisible mode
 * - Graceful fallback if offline
 *
 * Usage:
 * ```tsx
 * <PresenceIndicator userId="user-123" showLabel size="md" />
 * // Shows: 🟢 Online or 🎵 jamming (if custom status set)
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

  // If offline/invisible and not showing label, just show the gray dot
  if (!userPresence?.online && !showLabel) {
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
 * PresenceBadge - Inline badge showing online presence with styling
 *
 * Only shows badge if user is actually online (not offline or invisible)
 *
 * Usage:
 * ```tsx
 * <PresenceBadge userId="user-123" variant="default" />
 * // Shows: 🟢 Online or 🎵 jamming
 * ```
 */
export function PresenceBadge({ userId, variant = 'default' }: PresenceBadgeProps) {
  const { data: presence } = usePresence(userId)
  const userPresence = presence?.[userId]

  // Only show badge if user is online and not invisible
  if (!userPresence || !userPresence.online || userPresence.invisible) {
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
 * PresenceList - Shows presence for multiple users from GetStream.io
 *
 * Features:
 * - Lists all online users (excluding invisible)
 * - Shows custom status if available
 * - Real-time updates
 *
 * Usage:
 * ```tsx
 * <PresenceList userIds={followingIds} />
 * // Shows all online users with their current status
 * ```
 */
export function PresenceList({ userIds }: PresenceListProps) {
  const { data: presence } = usePresence(userIds)

  const onlineUsers = userIds
    .filter((id) => presence?.[id]?.online && !presence?.[id]?.invisible)
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
