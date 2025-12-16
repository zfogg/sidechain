import { ReactNode } from 'react'
import { useChatStore } from '@/stores/useChatStore'

interface ChatGuardProps {
  children: ReactNode
  fallback?: ReactNode
}

/**
 * ChatGuard - Wrapper to safely use Chat components only when available
 * Prevents errors when Stream Chat API key is not configured
 */
export function ChatGuard({ children, fallback }: ChatGuardProps) {
  const { isStreamConnected } = useChatStore()

  if (!isStreamConnected) {
    return (
      <>
        {fallback || (
          <div className="flex items-center justify-center h-96 rounded-lg bg-muted border border-border">
            <div className="text-center">
              <div className="text-2xl mb-2">💬</div>
              <p className="text-foreground font-semibold mb-1">Chat Not Available</p>
              <p className="text-sm text-muted-foreground">Stream Chat is not configured</p>
            </div>
          </div>
        )}
      </>
    )
  }

  return <>{children}</>
}
