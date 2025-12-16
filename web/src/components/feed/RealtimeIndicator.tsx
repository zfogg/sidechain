import { useWebSocketStatus } from '@/hooks/useWebSocket'
import { useEffect, useState } from 'react'

/**
 * RealtimeIndicator - Shows WebSocket connection status
 */
export function RealtimeIndicator() {
  const { isConnected } = useWebSocketStatus()
  const [showIndicator, setShowIndicator] = useState(true)

  useEffect(() => {
    // Auto-hide after 3 seconds if connected
    if (isConnected) {
      const timer = setTimeout(() => setShowIndicator(false), 3000)
      return () => clearTimeout(timer)
    } else {
      setShowIndicator(true)
    }
  }, [isConnected])

  if (!showIndicator && isConnected) {
    return null
  }

  return (
    <div
      className={`fixed bottom-6 right-6 px-4 py-2 rounded-full text-sm font-medium transition-all ${
        isConnected
          ? 'bg-green-500/20 text-green-400 border border-green-500/30'
          : 'bg-yellow-500/20 text-yellow-400 border border-yellow-500/30 animate-pulse'
      }`}
    >
      <div className="flex items-center gap-2">
        <div
          className={`w-2 h-2 rounded-full ${
            isConnected ? 'bg-green-400' : 'bg-yellow-400 animate-pulse'
          }`}
        />
        {isConnected ? 'Live updates active' : 'Connecting...'}
      </div>
    </div>
  )
}
