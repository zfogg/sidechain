import { useEffect, ReactNode } from 'react'
import { useUserStore } from '@/stores/useUserStore'
import { useWebSocket } from '@/hooks/useWebSocket'

interface AuthProviderProps {
  children: ReactNode
}

/**
 * AuthProvider - Initializes authentication state on app load
 *
 * Responsibilities:
 * - Restore session from localStorage
 * - Setup WebSocket connection for real-time updates
 * - Handle logout on token expiration
 */
export function AuthProvider({ children }: AuthProviderProps) {
  const { restoreSession } = useUserStore()

  // Initialize WebSocket (will only connect if authenticated)
  useWebSocket()

  // Restore session from localStorage on app load
  useEffect(() => {
    restoreSession()
  }, [restoreSession])

  return <>{children}</>
}
