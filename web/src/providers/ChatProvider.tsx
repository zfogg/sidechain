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

  // If chat not initialized, create a temporary client for context
  // This ensures useChatContext() works even if not yet connected
  let client = chatClient
  if (!client) {
    try {
      if (apiKey) {
        client = StreamChat.getInstance(apiKey)
      }
    } catch (err) {
      console.error('[Chat] Failed to create Stream Chat instance:', err)
    }
  }

  // If still no client, show error
  if (!client) {
    return (
      <div className="flex items-center justify-center h-screen">
        <div className="text-center space-y-4">
          <div className="text-red-500">⚠️</div>
          <p className="text-foreground">Failed to initialize chat</p>
          <p className="text-sm text-muted-foreground">Please refresh the page</p>
        </div>
      </div>
    )
  }

  // If connecting, show loading state over the Chat context
  if (isConnecting) {
    return (
      <Chat client={client} theme="dark">
        <div className="flex items-center justify-center h-screen">
          <Spinner size="lg" />
        </div>
      </Chat>
    )
  }

  // Always render Chat wrapper so useChatContext works
  // If there's an error, log it but still render (graceful degradation)
  if (error) {
    console.warn('[Chat] Chat connection error:', error)
  }

  return (
    <Chat client={client} theme="dark">
      {children}
    </Chat>
  )
}
