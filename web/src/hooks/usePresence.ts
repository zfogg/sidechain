import { useEffect, useState } from 'react'
import { useChatContext } from 'stream-chat-react'
import type { StreamUser } from '@/types/stream'

export interface PresenceData {
  online: boolean
  status?: string // Custom status message (e.g., "jamming", "in studio")
  invisible?: boolean // Appears offline while connected
  last_active?: string // ISO timestamp
  name?: string
  image?: string
}

export interface PresenceResponse {
  [userId: string]: PresenceData
}

/**
 * usePresence - Get presence data from GetStream.io for one or more users
 *
 * Features:
 * - Gets online/offline status directly from GetStream.io
 * - Real-time updates via GetStream.io events
 * - Custom status messages
 * - Invisible mode detection
 * - Automatic event listener cleanup
 *
 * Usage:
 * ```tsx
 * const { data: presence, isLoading } = usePresence(['user-1', 'user-2'])
 * if (presence?.['user-1']?.online) {
 *   return <span>🟢 {presence['user-1'].status || 'Online'}</span>
 * }
 * ```
 */
export function usePresence(userIds: string | string[] | null | undefined) {
  const ids = Array.isArray(userIds) ? userIds : userIds ? [userIds] : []
  const { client } = useChatContext()
  const [presence, setPresence] = useState<PresenceResponse>({})
  const [isLoading, setIsLoading] = useState(true)

  // Fetch initial presence and subscribe to updates
  useEffect(() => {
    if (!client || ids.length === 0) {
      setIsLoading(false)
      return
    }

    const fetchPresence = async () => {
      try {
        // Query user data from GetStream.io to get presence
        const response = await client.queryUsers({ id: { $in: ids } })

        const presenceMap: PresenceResponse = {}
        for (const user of response.users as StreamUser[]) {
          presenceMap[user.id] = {
            online: user.online || false,
            status: user.status || undefined,
            invisible: user.invisible || false,
            last_active: user.last_active || undefined,
            name: user.name,
            image: user.image,
          }
        }

        setPresence(presenceMap)
        setIsLoading(false)
      } catch (error) {
        console.error('[Presence] Failed to fetch user presence:', error)
        setIsLoading(false)
      }
    }

    fetchPresence()

    // Listen to presence changes for all queried users
    const handlePresenceChange = (event: any) => {
      const userId = event.user?.id
      if (userId && ids.includes(userId)) {
        setPresence(prev => ({
          ...prev,
          [userId]: {
            online: event.user.online || false,
            status: event.user.status || undefined,
            invisible: event.user.invisible || false,
            last_active: event.user.last_active || undefined,
            name: event.user.name,
            image: event.user.image,
          }
        }))
      }
    }

    const handleUserUpdate = (event: any) => {
      const userId = event.user?.id
      if (userId && ids.includes(userId)) {
        setPresence(prev => ({
          ...prev,
          [userId]: {
            online: event.user.online || false,
            status: event.user.status || undefined,
            invisible: event.user.invisible || false,
            last_active: event.user.last_active || undefined,
            name: event.user.name,
            image: event.user.image,
          }
        }))
      }
    }

    // Subscribe to GetStream.io presence events
    client.on('user.presence.changed', handlePresenceChange)
    client.on('user.online', handlePresenceChange)
    client.on('user.offline', handlePresenceChange)
    client.on('user.updated', handleUserUpdate)

    return () => {
      client.off('user.presence.changed', handlePresenceChange)
      client.off('user.online', handlePresenceChange)
      client.off('user.offline', handlePresenceChange)
      client.off('user.updated', handleUserUpdate)
    }
  }, [client, ids.join(',')])

  return { data: presence, isLoading }

}

/**
 * Get presence status description for display
 *
 * GetStream.io presence includes:
 * - online: Boolean indicating if connected
 * - invisible: Boolean indicating if appearing offline
 * - status: Custom status message (e.g., "jamming", "in studio")
 * - last_active: ISO timestamp of last activity
 *
 * Usage:
 * ```tsx
 * const presence = usePresence('user-123').data?.['user-123']
 * const status = getPresenceStatus(presence)
 * // Returns: "jamming" or "Online" or "Offline" etc
 * ```
 */
export function getPresenceStatus(presence: PresenceData | undefined): string {
  if (!presence) return 'Offline'

  // If invisible, appear offline
  if (presence.invisible) {
    return 'Offline'
  }

  // Show custom status if online
  if (presence.online && presence.status) {
    return presence.status
  }

  // Show online/offline
  if (presence.online) {
    return 'Online'
  }

  // Show last active time if available
  if (presence.last_active) {
    const ago = getTimeAgo(new Date(presence.last_active).getTime())
    return `Last active ${ago}`
  }

  return 'Offline'
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

  // If invisible, show offline
  if (presence.invisible) {
    return '⚪'
  }

  if (presence.online) {
    // If custom status, try to detect DAW
    if (presence.status) {
      const status = presence.status.toLowerCase()
      if (status.includes('ableton')) return '🎚️'
      if (status.includes('logic')) return '🎹'
      if (status.includes('cubase')) return '🎼'
      if (status.includes('fl studio') || status.includes('fl_studio')) return '🔴'
      if (status.includes('reaper')) return '📋'
      if (status.includes('studio') || status.includes('jamming')) return '🎵'
    }
    return '🟢' // Generic online
  }

  return '⚪' // Offline
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
