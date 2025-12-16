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

  // Ensure we have the connected chat client before rendering
  // Don't fall back to creating an unconnected client
  if (!chatClient) {
    // Still waiting for client initialization
    if (isConnecting) {
      // Initialize a temporary client just for the Chat wrapper context
      let tempClient: StreamChat | null = null
      try {
        if (apiKey) {
          tempClient = StreamChat.getInstance(apiKey)
        }
      } catch (err) {
        console.error('[Chat] Failed to create temporary Stream Chat instance:', err)
      }

      if (tempClient) {
        return (
          <Chat client={tempClient} theme="dark">
            <div className="flex items-center justify-center h-screen">
              <Spinner size="lg" />
            </div>
          </Chat>
        )
      }
    }

    // Not connecting and no client - show error
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

  // If still connecting, show loading state
  if (isConnecting) {
    return (
      <Chat client={chatClient} theme="dark">
        <div className="flex items-center justify-center h-screen">
          <Spinner size="lg" />
        </div>
      </Chat>
    )
  }

  // Connected successfully - render children with the connected client
  if (error) {
    console.warn('[Chat] Chat connection error (graceful degradation):', error)
  }

  return (
    <Chat client={chatClient} theme="dark">
      {children}
    </Chat>
  )
}
