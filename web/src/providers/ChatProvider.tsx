import { useEffect, useState, ReactNode } from 'react'
import { Chat } from 'stream-chat-react'
import { StreamChat } from 'stream-chat'
import { useUserStore } from '@/stores/useUserStore'
import { useChatStore } from '@/stores/useChatStore'
import { AuthClient } from '@/api/AuthClient'
import { Spinner } from '@/components/ui/spinner'
import 'stream-chat-react/css/v2/index.css'

interface ChatProviderProps {
  children: ReactNode
}

/**
 * ChatProvider - Stream Chat integration provider
 *
 * Handles:
 * - Stream Chat client initialization
 * - Token acquisition from backend
 * - User connection
 * - Error handling and reconnection
 */
export function ChatProvider({ children }: ChatProviderProps) {
  const { user, token } = useUserStore()
  const { setStreamToken, setStreamConnected } = useChatStore()
  const [chatClient, setChatClient] = useState<StreamChat | null>(null)
  const [isConnecting, setIsConnecting] = useState(false)
  const [error, setError] = useState<string | null>(null)

  const apiKey = import.meta.env.VITE_STREAM_API_KEY
  if (!apiKey) {
    console.warn('[Chat] VITE_STREAM_API_KEY not configured')
  }

  // Initialize Stream Chat client
  useEffect(() => {
    if (!user || !token || !apiKey) return

    const initChat = async () => {
      setIsConnecting(true)
      setError(null)

      try {
        // Validate user has required fields
        if (!user.id) {
          throw new Error('User ID is not set. Please log in again.')
        }

        // Create Stream Chat instance if not already created
        let client = chatClient
        if (!client) {
          client = StreamChat.getInstance(apiKey)
          setChatClient(client)
        }

        // Get Stream token from backend
        console.log('[Chat] Fetching Stream token for user:', user.id)
        const result = await AuthClient.getStreamToken()
        if (result.isError()) {
          throw new Error(`Failed to get Stream token: ${result.getError()}`)
        }

        const streamToken = result.getValue().token
        if (!streamToken) {
          throw new Error('Stream token is empty')
        }

        setStreamToken(streamToken)

        // Connect user to Stream Chat
        console.log('[Chat] Connecting user to Stream Chat:', {
          userId: user.id,
          displayName: user.displayName,
        })

        await client.connectUser(
          {
            id: user.id,
            name: user.displayName || user.username,
            image: user.profilePictureUrl,
          },
          streamToken
        )

        setStreamConnected(true)
        setIsConnecting(false)
        console.log('[Chat] Successfully connected to Stream Chat')
      } catch (err) {
        const errorMessage = err instanceof Error ? err.message : 'Failed to connect to Stream Chat'
        console.error('[Chat] Connection failed:', {
          error: errorMessage,
          userId: user.id,
          hasToken: !!token,
        })
        setError(errorMessage)
        setIsConnecting(false)
      }
    }

    initChat()

    // Cleanup on unmount or when user logs out
    return () => {
      if (chatClient) {
        chatClient.disconnectUser()
        setStreamConnected(false)
      }
    }
  }, [user, token, apiKey, chatClient])

  // If chat not initialized, show children anyway (graceful degradation)
  if (!chatClient) {
    return <>{children}</>
  }

  // If connecting, show loading state
  if (isConnecting) {
    return (
      <div className="flex items-center justify-center h-screen">
        <Spinner size="lg" />
      </div>
    )
  }

  // If error, show error message but still render children
  if (error) {
    console.warn('[Chat] Chat not available:', error)
    // Gracefully degrade - still show app without chat
    return <>{children}</>
  }

  return (
    <Chat client={chatClient} theme="dark">
      {children}
    </Chat>
  )
}
