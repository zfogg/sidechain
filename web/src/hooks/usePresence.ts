import { useQuery } from '@tanstack/react-query'
import { apiClient } from '@/api/client'

export interface PresenceData {
  status: 'online' | 'offline' | 'in_studio'
  daw?: string
  last_activity?: number
}

export interface PresenceResponse {
  [userId: string]: PresenceData
}

/**
 * usePresence - Fetch presence data for one or more users
 *
 * Features:
 * - Gets online/offline status and DAW information for users
 * - Caches results for 30 seconds
 * - Automatically refetches when user IDs change
 * - Shows offline status by default if user not online
 *
 * Usage:
 * ```tsx
 * const { data: presence, isLoading } = usePresence(['user-1', 'user-2'])
 * if (presence?.['user-1']?.status === 'in_studio') {
 *   return <span>🎚️ In {presence['user-1'].daw}</span>
 * }
 * ```
 */
export function usePresence(userIds: string | string[] | null | undefined) {
  const ids = Array.isArray(userIds) ? userIds : userIds ? [userIds] : []

  return useQuery<PresenceResponse>({
    queryKey: ['presence', ids],
    queryFn: async () => {
      if (ids.length === 0) {
        return {}
      }

      const response = await apiClient.post<{ presence: PresenceResponse }>('/users/presence', {
        user_ids: ids,
      })

      if (response.isOk()) {
        return response.getValue().presence || {}
      }
      return {}
    },
    enabled: ids.length > 0,
    staleTime: 30 * 1000, // 30 seconds
    refetchInterval: 60 * 1000, // Refetch every minute for fresh status
  })
}

/**
 * Get presence status description for display
 *
 * Usage:
 * ```tsx
 * const presence = usePresence('user-123').data?.['user-123']
 * const status = getPresenceStatus(presence)
 * // Returns: "In Ableton Live" or "Online" or "Offline" etc
 * ```
 */
export function getPresenceStatus(presence: PresenceData | undefined): string {
  if (!presence) return 'Offline'

  switch (presence.status) {
    case 'in_studio':
      return `In ${presence.daw || 'DAW'}`
    case 'online':
      return 'Online'
    case 'offline':
    default:
      return presence.last_activity
        ? `Active ${getTimeAgo(presence.last_activity)}`
        : 'Offline'
  }
}

/**
 * Get presence icon/emoji for display
 *
 * Usage:
 * ```tsx
 * <span>{getPresenceIcon(presence)}</span>
 * ```
 */
export function getPresenceIcon(presence: PresenceData | undefined): string {
  if (!presence) return '⚪'

  switch (presence.status) {
    case 'in_studio': {
      // Show DAW-specific emoji
      const daw = (presence.daw || '').toLowerCase()
      if (daw.includes('ableton')) return '🎚️'
      if (daw.includes('logic')) return '🎹'
      if (daw.includes('cubase')) return '🎼'
      if (daw.includes('fl studio') || daw.includes('fl_studio')) return '🔴'
      if (daw.includes('reaper')) return '📋'
      return '🎵' // Generic DAW icon
    }
    case 'online':
      return '🟢'
    case 'offline':
    default:
      return '⚪'
  }
}

/**
 * Format time since last activity
 */
function getTimeAgo(timestampMs: number): string {
  const now = Date.now()
  const diff = now - timestampMs
  const seconds = Math.floor(diff / 1000)
  const minutes = Math.floor(seconds / 60)
  const hours = Math.floor(minutes / 60)
  const days = Math.floor(hours / 24)

  if (seconds < 60) return 'just now'
  if (minutes < 60) return `${minutes}m ago`
  if (hours < 24) return `${hours}h ago`
  return `${days}d ago`
}
